add_library(beam_camera
  src/NullCamera.cpp
  src/SimulatedCamera.cpp
)

add_library(beam::camera ALIAS beam_camera)

target_include_directories(beam_camera
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(beam_camera PUBLIC beam::core)
target_compile_features(beam_camera PUBLIC cxx_std_17)
