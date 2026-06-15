cmake_minimum_required(VERSION 3.25)
project(GDM
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Two-phase oil-water reservoir simulator"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC-specific settings
if(MSVC)
    add_compile_options(
        /W4             # Warning level 4
        /permissive-    # Strict standards conformance
        /utf-8          # Source and execution charset UTF-8
        /EHsc           # Standard exception handling
    )

    # AddressSanitizer in Debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES)
        add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
    endif()
endif()

# Project directories
set(GDM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(GDM_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

# Header-only for now — no sources to compile yet
# Uncomment when source files appear:
#
# file(GLOB_RECURSE GDM_SOURCES ${GDM_SOURCE_DIR}/*.cpp)
# file(GLOB_RECURSE GDM_HEADERS ${GDM_INCLUDE_DIR}/*.hpp)
#
# add_library(gdm_core ${GDM_SOURCES} ${GDM_HEADERS})
# target_include_directories(gdm_core PUBLIC ${GDM_INCLUDE_DIR})

# Tests (uncomment when test framework is added)
# enable_testing()
# add_subdirectory(tests)
