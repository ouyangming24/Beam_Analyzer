#pragma once

#include "beam/core/BeamFrame.hpp"

#include <QImage>

namespace beam::ui {

enum class ColorMap {
  Grayscale,
  FalseColor,  ///< Blue → cyan → green → yellow → red
};

[[nodiscard]] QImage frameToImage(const beam::core::BeamFrame& frame,
                                  ColorMap map,
                                  double* outMin = nullptr,
                                  double* outMax = nullptr);

}  // namespace beam::ui
