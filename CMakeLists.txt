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
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp

    # Grid / Cells
    ${HYDRO}/Solver/Grids/OilField.cpp
    ${HYDRO}/Solver/Grids/Cells/AbstractCells.cpp
    ${HYDRO}/Solver/Grids/Cells/TwoPhaseFlowCell.cpp

    # Math / Linear algebra
    ${HYDRO}/Solver/Math/LinearProblem.cpp
    ${HYDRO}/Solver/Math/MatrixCSR.cpp
    ${HYDRO}/Solver/Math/SparsityPattern.cpp
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
target_include_directories(gdm_core PUBLIC ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
target_compile_definitions(gdm_core PUBLIC AMGCL_NO_BOOST)

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
)
target_link_libraries(gdm_tests PRIVATE gdm_core Catch2::Catch2WithMain)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gdm_tests
    PROPERTIES TIMEOUT 600
)

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
    # Maps.h включает системную Eigen (std::result_of удалён в C++23)
    target_compile_definitions(gdm_data PUBLIC _HAS_DEPRECATED_RESULT_OF=1)

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
