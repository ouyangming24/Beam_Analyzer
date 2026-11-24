add_executable(test_iso11146
  test_iso11146.cpp
)

target_link_libraries(test_iso11146 PRIVATE beam::core beam::io)
target_compile_features(test_iso11146 PRIVATE cxx_std_17)

add_test(NAME iso11146_gaussian COMMAND test_iso11146)
