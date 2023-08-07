cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" listener_version)

project(listener)

set(PROJECT_VERSION ${listener_version})

option(BUILD_TESTING "whether to build unit test" OFF)

option(ONNX "whether to build with ONNX" ON)
option(GPU "whether to build with GPU" OFF)

set(CMAKE_VERBOSE_MAKEFILE OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
get_filename_component(fc_base "fc_base" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(FETCHCONTENT_BASE_DIR ${fc_base})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT MSVC)
  # Keep the same with openfst, -fPIC or -fpic
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result -std=c++14 -pthread -fPIC")
else()
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

include(openfst)
if(ONNX)
    include(onnx)
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/kaldi
)

# Build all libraries
add_subdirectory(utils)
add_subdirectory(frontend)
add_subdirectory(post_processor)
add_subdirectory(kaldi)  # kaldi: wfst based decoder
add_subdirectory(decoder)

# Build bin
add_executable(listener main.cc)
target_include_directories(listener PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include")
set_target_properties(listener PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_link_libraries(listener PUBLIC decoder)

# Unit Test
if(BUILD_TESTING)
  include(gtest)
  add_subdirectory(test)
endif()