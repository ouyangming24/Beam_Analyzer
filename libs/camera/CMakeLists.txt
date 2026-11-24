set(BEAM_CAMERA_SOURCES
  src/NullCamera.cpp
  src/SimulatedCamera.cpp
  src/CameraRegistry.cpp
  src/GenTLCamera.cpp
  src/UvcCamera.cpp
)

add_library(beam_camera ${BEAM_CAMERA_SOURCES})
add_library(beam::camera ALIAS beam_camera)

target_include_directories(beam_camera
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(beam_camera PUBLIC beam::core PRIVATE Threads::Threads)
target_compile_features(beam_camera PUBLIC cxx_std_17)

if(BEAM_ENABLE_OPENCV)
  find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio)
  target_compile_definitions(beam_camera PUBLIC BEAM_ENABLE_OPENCV=1)
  target_link_libraries(beam_camera PRIVATE ${OpenCV_LIBS})
  message(STATUS "beam_camera: OpenCV VideoCapture backend ENABLED")
else()
  target_compile_definitions(beam_camera PUBLIC BEAM_ENABLE_OPENCV=0)
  message(STATUS "beam_camera: OpenCV VideoCapture backend disabled (stub UVC)")
endif()
