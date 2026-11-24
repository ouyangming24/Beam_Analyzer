#pragma once

#include "beam/core/BeamFrame.hpp"

#include <QImage>

namespace beam::ui {

enum class ColorMap {
  Grayscale,
  FalseColor,  ///< Blue → cyan → green → yellow → red
};

enum class IntensityScale {
  Linear,
  Log,  ///< Display-only log mapping; does not affect ISO metrics
};

[[nodiscard]] QImage frameToImage(const beam::core::BeamFrame& frame,
                                  ColorMap map,
                                  double* outMin = nullptr,
                                  double* outMax = nullptr,
                                  IntensityScale scale = IntensityScale::Linear);

}  // namespace beam::ui
