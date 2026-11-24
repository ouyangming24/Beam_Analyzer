cmake_minimum_required(VERSION 3.21)
project(BeamAnalyzer VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BEAM_BUILD_APP "Build Qt BeamAnalyzer GUI" ON)
option(BEAM_BUILD_TESTS "Build unit tests" ON)
# Optional soft dependency for UVC path — off by default so CI needs no OpenCV.
option(BEAM_ENABLE_OPENCV "Enable OpenCV VideoCapture UVC backend" OFF)

add_subdirectory(libs/core)
add_subdirectory(libs/io)
add_subdirectory(libs/camera)

# CLI scan session tool (no Qt) — always available for labs / CI.
add_subdirectory(apps/beam_m2_scan)

if(BEAM_BUILD_APP)
  add_subdirectory(apps/BeamAnalyzer)
endif()

if(BEAM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/core)
  add_subdirectory(tests/camera)
  add_subdirectory(tests/io)
endif()

