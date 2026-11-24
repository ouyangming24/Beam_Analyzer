add_executable(test_camera
  test_camera.cpp
)

target_link_libraries(test_camera PRIVATE beam::camera)
target_compile_features(test_camera PRIVATE cxx_std_17)

add_test(NAME camera_registry_simulated COMMAND test_camera)
