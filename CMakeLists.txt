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

set(HYDRO ${CMAKE_SOURCE_DIR}/HydroSolver)

set(GDM_SOURCES
    src/main.cpp
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

add_executable(gdm ${GDM_SOURCES})

target_include_directories(gdm PRIVATE
    ${HYDRO}
    ${HYDRO}/AMGSolver/amgcl
)

target_compile_definitions(gdm PRIVATE
    AMGCL_NO_BOOST
)
