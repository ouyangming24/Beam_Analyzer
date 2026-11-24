add_library(beam_io
  src/CsvImporter.cpp
  src/ResultExporter.cpp
)

add_library(beam::io ALIAS beam_io)

target_include_directories(beam_io
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(beam_io PUBLIC beam::core)
target_compile_features(beam_io PUBLIC cxx_std_17)
