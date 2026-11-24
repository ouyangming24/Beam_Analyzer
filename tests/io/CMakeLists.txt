add_executable(test_image_io
  test_image_io.cpp
)

target_link_libraries(test_image_io PRIVATE beam::io beam::core)
target_compile_features(test_image_io PRIVATE cxx_std_17)

add_test(NAME image_io_bmp_roundtrip COMMAND test_image_io)
