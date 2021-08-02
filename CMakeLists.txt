cmake_minimum_required(VERSION 3.9)

# https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html
cmake_policy(SET CMP0091 NEW)

project(inexor-vulkan-renderer)

# Stop in source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Options
option(INEXOR_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(INEXOR_BUILD_DOC "Build documentation" OFF)
option(INEXOR_BUILD_EXAMPLE "Build example" ON)
option(INEXOR_BUILD_TESTS "Build tests" OFF)
set(INEXOR_CONAN_PROFILE "default" CACHE STRING "conan profile")
option(INEXOR_USE_VMA_RECORDING "Use VulkanMemoryAllocator recording feature" OFF)

message(STATUS "INEXOR_BUILD_BENCHMARKS = ${INEXOR_BUILD_BENCHMARKS}")
message(STATUS "INEXOR_BUILD_DOC = ${INEXOR_BUILD_DOC}")
message(STATUS "INEXOR_BUILD_EXAMPLE = ${INEXOR_BUILD_EXAMPLE}")
message(STATUS "INEXOR_BUILD_TESTS= ${INEXOR_BUILD_TESTS}")
message(STATUS "INEXOR_CONAN_PROFILE = ${INEXOR_CONAN_PROFILE}")
message(STATUS "INEXOR_USE_VMA_RECORDING = ${INEXOR_USE_VMA_RECORDING}")

message(STATUS "CMAKE_VERSION = ${CMAKE_VERSION}")
message(STATUS "CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
message(STATUS "C Compiler executable: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler executable: ${CMAKE_CXX_COMPILER}")
message(STATUS "Linker executable: ${CMAKE_LINKER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

find_package(Vulkan REQUIRED)

# Dependency setup with conan
include(conan_setup)

# The marked constants in application.hpp will be replaced with the following values
set(INEXOR_NAME "Inexor Engine")
set(INEXOR_DEV_TEAM_NAME "Inexor Collective")
set(INEXOR_LICENSE "MIT License")

set(INEXOR_VERSION_MAJOR 0)
set(INEXOR_VERSION_MINOR 1)
set(INEXOR_VERSION_PATCH 0)

# TODO: place it outside of the engine code
set(INEXOR_APP_DEV_TEAM_NAME "Inexor Collective" CACHE STRING "App developer name")
set(INEXOR_APP_NAME "Inexor Engine App Example" CACHE STRING "App name")
set(INEXOR_APP_DESCRIPTION "Inexor Engine App Example" CACHE STRING "App short description")
set(INEXOR_APP_LICENSE "MIT License" CACHE STRING "App license")
set(INEXOR_APP_VERSION_MAJOR 0 CACHE STRING "App version major")
set(INEXOR_APP_VERSION_MINOR 1 CACHE STRING "App version minor")
set(INEXOR_APP_VERSION_PATCH 0 CACHE STRING "App version patch")
set(INEXOR_APP_ICON_ICO "${PROJECT_SOURCE_DIR}\\example\\assets\\textures\\inexor_icon_256px.ico" CACHE PATH "App icon as .ico")

find_package(Git REQUIRED)

# Extract the current git sha
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --abbrev=7
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE INEXOR_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(INEXOR_APP_GIT_SHA ${INEXOR_GIT_SHA} CACHE STRING "App git sha")

add_subdirectory(shaders)

add_subdirectory(src)

if(INEXOR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(INEXOR_BUILD_DOC)
    add_subdirectory(documentation)
endif()

if(INEXOR_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(INEXOR_BUILD_TESTS)
    add_subdirectory(tests)
endif()
