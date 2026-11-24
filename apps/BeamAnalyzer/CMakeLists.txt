set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(BeamAnalyzer
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.hpp
  src/FalseColor.cpp
  src/FalseColor.hpp
  src/ImageView.cpp
  src/ImageView.hpp
  src/ProfilePlot.cpp
  src/ProfilePlot.hpp
  src/M2ScanDialog.cpp
  src/M2ScanDialog.hpp
)

target_link_libraries(BeamAnalyzer
  PRIVATE
    Qt6::Widgets
    beam::core
    beam::io
    beam::camera
)

target_compile_features(BeamAnalyzer PRIVATE cxx_std_17)

if(WIN32)
  set_target_properties(BeamAnalyzer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Convenience: copy sample data next to the binary when building.
add_custom_command(TARGET BeamAnalyzer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BeamAnalyzer>/samples
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/samples
          $<TARGET_FILE_DIR:BeamAnalyzer>/samples
  COMMENT "Copying sample CSV data beside BeamAnalyzer"
)
