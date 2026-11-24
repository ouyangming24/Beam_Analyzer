add_library(beam_core
  src/BeamFrame.cpp
  src/Iso11146.cpp
  src/M2Fit.cpp
  src/M2Scan.cpp
  src/Roi.cpp
  src/PowerMetrics.cpp
  src/Profile.cpp
)

add_library(beam::core ALIAS beam_core)

target_include_directories(beam_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(beam_core PUBLIC cxx_std_17)
