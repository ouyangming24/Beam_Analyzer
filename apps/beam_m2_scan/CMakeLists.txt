add_executable(beam_m2_scan
  src/main.cpp
)

target_link_libraries(beam_m2_scan
  PRIVATE
    beam::core
    beam::io
)

target_compile_features(beam_m2_scan PRIVATE cxx_std_17)

install(TARGETS beam_m2_scan RUNTIME DESTINATION bin)
