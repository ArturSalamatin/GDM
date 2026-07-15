cmake_minimum_required(VERSION 3.25)
project(GDM
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Two-phase oil-water reservoir simulator"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W3
        /utf-8
        /EHsc
        /bigobj
    )
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)

# --- Catch2 ---
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)

set(HYDRO ${CMAKE_SOURCE_DIR}/HydroSolver)

# =============================================================================
# gdm_core — статическая библиотека ядра солвера
# =============================================================================
add_library(gdm_core STATIC
    ${HYDRO}/stdafx.cpp

    # Reservoir
    ${HYDRO}/Reservoir/MassBalanceTracker.cpp
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
    ${HYDRO}/Reservoir/TimeIntegrator.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
    ${HYDRO}/Reservoir/PIController.cpp

    # Grid / Cells
    ${HYDRO}/Solver/Grids/OilField.cpp
    ${HYDRO}/Solver/Grids/Cells/AbstractCells.cpp
    ${HYDRO}/Solver/Grids/Cells/TwoPhaseFlowCell.cpp

    # Math / Linear algebra
    ${HYDRO}/Solver/Math/LinearProblem.cpp
    ${HYDRO}/Solver/Math/MatrixCSR.cpp
    ${HYDRO}/Solver/Math/SparsityPattern.cpp
    ${HYDRO}/Solver/Math/CRSStructure.cpp
    ${HYDRO}/Solver/Math/MathRoutines.cpp

    # Wells
    ${HYDRO}/Reservoir/Well/SomeWell.cpp
    ${HYDRO}/Reservoir/Well/Wells.cpp
    ${HYDRO}/Reservoir/Well/WellJobs.cpp
    ${HYDRO}/Reservoir/Well/SetOfPoints.cpp
    ${HYDRO}/Reservoir/Well/WellTrajectory.cpp

    # Flow field / Streamlines
    ${HYDRO}/Anomaly/FlowField/SomeFlowField.cpp
    ${HYDRO}/Anomaly/FlowField/FlowField.cpp
    ${HYDRO}/Anomaly/FlowField/Point.cpp
    ${HYDRO}/Anomaly/Trajectory.cpp

    # Descriptors
    ${HYDRO}/Descriptors/Descriptors.cpp
    ${HYDRO}/Descriptors/MER_Descriptor.cpp

    # Helpers
    ${HYDRO}/Helpers/LogFile.cpp
)
target_include_directories(gdm_core PUBLIC ${HYDRO} ${CMAKE_SOURCE_DIR}/amgcl)
set(GDM_SOLVER "CPR_BICGSTAB" CACHE STRING "AMGCL solver configuration (CPR, CPR_BICGSTAB, CPR_SA, CPR_DRS, ILU0)")
set_property(CACHE GDM_SOLVER PROPERTY STRINGS CPR CPR_BICGSTAB CPR_SA CPR_DRS ILU0)
target_compile_definitions(gdm_core PUBLIC GDM_SOLVER_${GDM_SOLVER})
option(AMGCL_PROFILING "Enable AMGCL built-in profiling (prof.tic/toc)" ON)
target_compile_definitions(gdm_core PUBLIC AMGCL_NO_BOOST)
if(AMGCL_PROFILING)
    target_compile_definitions(gdm_core PUBLIC AMGCL_PROFILING)
endif()
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(gdm_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# =============================================================================
# gdm — executable, инициализация через C++ API
# =============================================================================
add_executable(gdm src/main.cpp)
target_link_libraries(gdm PRIVATE gdm_core)

# =============================================================================
# gdm_tests — Catch2
# =============================================================================
add_executable(gdm_tests
    tests/test_smoke.cpp
    tests/test_stationary_pressure.cpp
    tests/test_mass_balance.cpp
    tests/test_buckley_leverett.cpp
    tests/test_components.cpp
    tests/test_five_spot.cpp
    tests/test_visual_verification.cpp
    tests/test_streamlines.cpp
    tests/test_variable_debit.cpp
    tests/test_3d_completions.cpp
    tests/test_inactive_cells.cpp
)
target_link_libraries(gdm_tests PRIVATE gdm_core Catch2::Catch2WithMain)

# =============================================================================
# gdm_benchmark — AMGCL configuration benchmark (separate binary)
# =============================================================================
add_executable(gdm_benchmark
    tests/test_amgcl_benchmark.cpp
    tests/test_solver_comparison.cpp
)
target_link_libraries(gdm_benchmark PRIVATE gdm_core Catch2::Catch2WithMain)

# =============================================================================
# gdm_unit_level0 — unit tests, zero internal dependencies (STL only)
# =============================================================================
add_executable(gdm_unit_level0
    tests/unit/reservoir/test_PIController.cpp
    tests/unit/geometry/test_GridDescriptors.cpp
    tests/unit/geometry/test_Point.cpp
    tests/unit/physics/test_PropertyDescriptor.cpp
    tests/unit/wells/test_GeosPoint.cpp
    tests/unit/wells/test_Segment.cpp
)
target_link_libraries(gdm_unit_level0 PRIVATE gdm_core Catch2::Catch2WithMain)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gdm_tests
    TEST_PREFIX "integration."
    PROPERTIES TIMEOUT 600
)
catch_discover_tests(gdm_benchmark
    TEST_PREFIX "benchmark."
    TEST_SPEC "[.slow]"
    PROPERTIES TIMEOUT 300
)
catch_discover_tests(gdm_unit_level0
    TEST_PREFIX "unit.level0."
    PROPERTIES TIMEOUT 30
)

# =============================================================================
# gdm_unit_level1 — unit tests, depend on level-0 classes
# =============================================================================
add_executable(gdm_unit_level1
    tests/unit/wells/test_SetOfPoints.cpp
    tests/unit/wells/test_SetOfPerforations.cpp
    tests/unit/reservoir/test_RawHorizon.cpp
    tests/unit/descriptors/test_Descriptors.cpp
    tests/unit/geometry/test_MathRoutines.cpp
)
target_link_libraries(gdm_unit_level1 PRIVATE gdm_core Catch2::Catch2WithMain)
catch_discover_tests(gdm_unit_level1
    TEST_PREFIX "unit.level1."
    PROPERTIES TIMEOUT 30
)

# =============================================================================
# gdm_unit_level2 — unit tests, medium dependency depth
# =============================================================================
add_executable(gdm_unit_level2
    tests/unit/math/test_CRSStructure.cpp
    tests/unit/math/test_SparsityPattern.cpp
    tests/unit/physics/test_AbstractCells.cpp
    tests/unit/wells/test_AccumulatedPerforations.cpp
    tests/unit/descriptors/test_MER_Data.cpp
    tests/unit/reservoir/test_NumericalParameters.cpp
    tests/unit/reservoir/test_BinaryIO.cpp
)
target_link_libraries(gdm_unit_level2 PRIVATE gdm_core Catch2::Catch2WithMain)
catch_discover_tests(gdm_unit_level2
    TEST_PREFIX "unit.level2."
    PROPERTIES TIMEOUT 60
)

# =============================================================================
# gdm_unit_level3 — unit tests, composite classes
# =============================================================================
add_executable(gdm_unit_level3
    tests/unit/math/test_MatrixCSR.cpp
    tests/unit/physics/test_TwoPhaseFlowCell.cpp
    tests/unit/wells/test_WellJobs.cpp
)
target_link_libraries(gdm_unit_level3 PRIVATE gdm_core Catch2::Catch2WithMain)
catch_discover_tests(gdm_unit_level3
    TEST_PREFIX "unit.level3."
    PROPERTIES TIMEOUT 60
)

# =============================================================================
# gdm_unit_level4 — unit tests, matrix assembly verification
# =============================================================================
add_executable(gdm_unit_level4
    tests/unit/math/test_MatrixAssembly.cpp
    tests/unit/math/test_LinearProblemAssembly.cpp
    tests/unit/math/test_JacobianAssembly.cpp
    tests/unit/math/test_JacobianAssembler_standalone.cpp
    tests/unit/math/test_MassBalanceTracker_standalone.cpp
    tests/unit/math/test_NewtonSolver_standalone.cpp
    tests/unit/math/test_TimeIntegrator_standalone.cpp
)
target_link_libraries(gdm_unit_level4 PRIVATE gdm_core Catch2::Catch2WithMain)
catch_discover_tests(gdm_unit_level4
    TEST_PREFIX "unit.level4."
    PROPERTIES TIMEOUT 120
)

# =============================================================================
# Examples — standalone simulation scenarios with CSV export
# =============================================================================
set(EXAMPLE_NAMES
    ex_single_injector
    ex_two_well
    ex_five_spot
    ex_variable_debit
    ex_3d_completions
    ex_benchmark_51x51x4
    ex_benchmark_series_cpr
    ex_benchmark_series_ts
)

foreach(ex IN LISTS EXAMPLE_NAMES)
    add_executable(${ex} examples/${ex}.cpp)
    target_link_libraries(${ex} PRIVATE gdm_core)
endforeach()

add_custom_target(run_examples
    COMMENT "Running all example scenarios and generating animations"
)

set(ANIMATE_2D ${CMAKE_SOURCE_DIR}/scripts/animate_fields.py)
set(ANIMATE_3D ${CMAKE_SOURCE_DIR}/scripts/animate_3d_fields.py)

# 2D examples: run binary, then animate
foreach(ex IN ITEMS ex_single_injector ex_two_well ex_five_spot ex_variable_debit)
    add_custom_target(run_${ex}
        COMMAND ${ex}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running ${ex}"
    )
    add_dependencies(run_${ex} ${ex})
    add_dependencies(run_examples run_${ex})
endforeach()

# 3D examples
foreach(ex3d IN ITEMS ex_3d_completions ex_benchmark_51x51x4)
    add_custom_target(run_${ex3d}
        COMMAND ${ex3d}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running ${ex3d}"
    )
    add_dependencies(run_${ex3d} ${ex3d})
    add_dependencies(run_examples run_${ex3d})
endforeach()

# Animation target — runs after all examples
add_custom_target(animate
    COMMENT "Generating animations from results/"
    DEPENDS run_examples
)

# 2D animations
foreach(case_dir IN ITEMS
    single_injector_21x21
    two_well_31x31
    five_spot
    vardebit_two_rates
    vardebit_shut_in
    vardebit_increasing_inj
    vardebit_alternating
)
    add_custom_command(TARGET animate POST_BUILD
        COMMAND python ${ANIMATE_2D} --results-dir results/${case_dir}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Animating ${case_dir}"
    )
endforeach()

# 3D animations
foreach(case_dir IN ITEMS
    3d_smoke
    3d_delayed_start
    3d_layer_closure
    benchmark_51x51x4
)
    add_custom_command(TARGET animate POST_BUILD
        COMMAND python ${ANIMATE_3D} --results-dir results/${case_dir}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Animating ${case_dir} (3D)"
    )
endforeach()

# =============================================================================
# gdm_data + gdm_app — Data-слой (PostgreSQL + GEOS)
# =============================================================================
find_package(PostgreSQL QUIET)

if(PostgreSQL_FOUND)
    # --- GEOS ---
    set(BUILD_TESTING_SAVE ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        geos
        GIT_REPOSITORY https://github.com/libgeos/geos.git
        GIT_TAG        3.11.4
    )
    FetchContent_MakeAvailable(geos)
    set(BUILD_TESTING ${BUILD_TESTING_SAVE} CACHE BOOL "" FORCE)

    # --- libpqxx ---
    set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG        7.10.0
    )
    FetchContent_MakeAvailable(libpqxx)

    # --- gdm_data (static library) ---
    add_library(gdm_data STATIC
        ${HYDRO}/Data/DBUtils.cpp
        ${HYDRO}/Data/ConnectionFactory.cpp
        ${HYDRO}/Data/ReservoirFactory.cpp
        ${HYDRO}/Data/HorizonFactory.cpp
        ${HYDRO}/Data/Wellfactory.cpp
        ${HYDRO}/Data/RawWellFactory.cpp
        ${HYDRO}/Data/wells/GEOSObject.cpp
        ${HYDRO}/Data/wells/Maps.cpp
        ${HYDRO}/Data/wells/MessageQueue.cpp
        ${HYDRO}/Data/wells/WellDataHandler.cpp
        ${HYDRO}/Data/wells/WellsBase.cpp
        ${HYDRO}/Utils/UniversalSVParser.cpp
        ${HYDRO}/Utils/UniversalSVWriter.cpp
        ${HYDRO}/Reservoir/CalculationManager.cpp
    )
    target_link_libraries(gdm_data PUBLIC gdm_core pqxx GEOS::geos)

    # --- gdm_app (executable, DB-backed entry point) ---
    add_executable(gdm_app
        ${HYDRO}/HydroSolver.cpp
        ${HYDRO}/tests/create_horizon.cpp
        ${HYDRO}/tests/create_merdata.cpp
        ${HYDRO}/tests/create_numericalparameters.cpp
        ${HYDRO}/tests/create_oilfield.cpp
        ${HYDRO}/tests/create_perforations.cpp
        ${HYDRO}/tests/create_reservoirsimulator.cpp
        ${HYDRO}/tests/create_well.cpp
        ${HYDRO}/tests/simulate_reservoir.cpp
    )
    target_link_libraries(gdm_app PRIVATE gdm_data)

    message(STATUS "PostgreSQL found — building gdm_data and gdm_app")
else()
    message(STATUS "PostgreSQL not found — gdm_data and gdm_app will not be built")
endif()
