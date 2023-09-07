cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(speaker LANGUAGES CXX)

option(USE_ALSA "use alsa-lib" OFF)

set(CMAKE_BUILD_TYPE "Release")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

if (USE_ALSA)
    find_package(ALSA REQUIRED)
endif()
find_package(ONNXRuntime REQUIRED)

message(STATUS "USE_ALSA: ${USE_ALSA}")

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

add_definitions(-DNAPI_VERSION=4)

add_library(${PROJECT_NAME} SHARED src/binding.cpp src/speaker.cpp ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

if (USE_ALSA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ALSA)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ALSA::ALSA)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_JS_INC}
    ${ONNXRUNTIME_INCLUDE_DIR}
    include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_JS_LIB}
    ${ONNXRUNTIME_LIBRARY}
)

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  # Generate node.lib
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()
