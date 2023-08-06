cmake_minimum_required(VERSION 3.13)

project(speaker C CXX)

find_package(PkgConfig)
pkg_check_modules(SPDLOG REQUIRED spdlog)

file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" speaker_version)

set(PROJECT_VERSION ${speaker_version})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(speaker main.cpp speaker.cpp)

string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wl,-rpath,'$ORIGIN'")
string(APPEND CMAKE_C_FLAGS " -Wall -Wextra")

target_link_libraries(speaker
                      onnxruntime
                      pthread
                      ${SPDLOG_LIBRARIES})

if(NOT APPLE)
  target_link_libraries(speaker -static-libgcc -static-libstdc++)
endif()

target_include_directories(speaker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include")

target_include_directories(speaker PUBLIC ${SPDLOG_INCLUDE_DIRS})

target_compile_options(speaker PUBLIC ${SPDLOG_CFLAGS_OTHER})

target_compile_definitions(speaker PUBLIC _SPEAKER_VERSION=${speaker_version})
