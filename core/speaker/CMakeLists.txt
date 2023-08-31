cmake_minimum_required(VERSION 3.14)

project(speaker LANGUAGES CXX)

find_package(Python3 REQUIRED)
find_package(PythonLibs REQUIRED)

if(DEFINED ENV{CONDA_PREFIX})
    set(PYTHON_VERSION "3.8")
    set(Python3_EXECUTABLE "$ENV{CONDA_PREFIX}/bin/python")
    set(PYTHON_INCLUDE_DIRS "$ENV{CONDA_PREFIX}/include/python${PYTHON_VERSION}")
endif()

message(STATUS "python executeable " ${Python3_EXECUTABLE})
message(STATUS "python include path " ${PYTHON_INCLUDE_DIRS})

set(CMAKE_BUILD_TYPE "Release")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SPEAKER_VERSION)

message(STATUS "speaker " ${SPEAKER_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread --std=c++17 -O2")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -O2 /std:c++17 /arch:AVX /source-charset:utf-8")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread --std=c++17 -O2 -march=native")
endif()

message(STATUS "CMAKE_CXX_FLAGS" ${CMAKE_CXX_FLAGS})

include(FetchContent)
set(FETCHCONTENT_QUIET off)
get_filename_component(fc_base "fc_base" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
set(FETCHCONTENT_BASE_DIR ${fc_base})

message(STATUS "Fetching pybind11")
FetchContent_Declare(pybind11
  URL      https://github.com/pybind/pybind11/archive/refs/tags/v2.11.1.tar.gz
  URL_HASH      SHA256=d475978da0cdc2d43b73f30910786759d593a9d8ee05b1b6846d1eb16c6d2e0c
)
FetchContent_MakeAvailable(pybind11)

include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/../../common/include)

add_library(speaker MODULE src/pybinding.cpp src/speaker.cpp)

target_compile_definitions(speaker PUBLIC SPEAKER_VERSION=${SPEAKER_VERSION})

target_link_libraries(speaker PUBLIC pybind11::module)
target_link_libraries(speaker PUBLIC onnxruntime)
target_link_libraries(speaker PUBLIC pthread)

pybind11_extension(speaker)