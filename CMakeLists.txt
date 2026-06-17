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

# --- Catch2 ---
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)

set(HYDRO ${CMAKE_SOURCE_DIR}/HydroSolver)

# Ядро солвера — все .cpp кроме main()
set(GDM_CORE_SOURCES
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

    # Flow field (needed by ReservoirSimulator)
    ${HYDRO}/Anomaly/FlowField/SomeFlowField.cpp
    ${HYDRO}/Anomaly/FlowField/FlowField.cpp
    ${HYDRO}/Anomaly/FlowField/Point.cpp

    # Descriptors
    ${HYDRO}/Descriptors/Descriptors.cpp
    ${HYDRO}/Descriptors/MER_Descriptor.cpp

    # Helpers
    ${HYDRO}/Helpers/LogFile.cpp
)

# --- Основной executable ---
add_executable(gdm src/main.cpp ${GDM_CORE_SOURCES})
target_include_directories(gdm PRIVATE ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
target_compile_definitions(gdm PRIVATE AMGCL_NO_BOOST)

# --- Тесты (Catch2) ---
add_executable(gdm_tests
    tests/test_smoke.cpp
    tests/test_stationary_pressure.cpp
    tests/test_mass_balance.cpp
    tests/test_buckley_leverett.cpp
)
target_sources(gdm_tests PRIVATE ${GDM_CORE_SOURCES})
target_link_libraries(gdm_tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(gdm_tests PRIVATE ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
target_compile_definitions(gdm_tests PRIVATE AMGCL_NO_BOOST)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gdm_tests)
