cmake_minimum_required(VERSION 3.21)
project(BeamAnalyzer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BEAM_BUILD_APP "Build Qt BeamAnalyzer GUI" ON)
option(BEAM_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(libs/core)
add_subdirectory(libs/io)
add_subdirectory(libs/camera)

if(BEAM_BUILD_APP)
  add_subdirectory(apps/BeamAnalyzer)
endif()

if(BEAM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/core)
endif()
