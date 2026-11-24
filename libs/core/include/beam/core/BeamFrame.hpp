#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace beam::core {

/// Single intensity frame on a uniform rectangular grid (row-major).
struct BeamFrame {
  std::size_t width = 0;
  std::size_t height = 0;
  double pixelSizeX_um = 1.0;  ///< Horizontal pixel pitch (µm)
  double pixelSizeY_um = 1.0;  ///< Vertical pixel pitch (µm)
  std::vector<double> intensity;  ///< Row-major, size == width * height

  [[nodiscard]] bool empty() const noexcept {
    return width == 0 || height == 0 || intensity.empty();
  }

  [[nodiscard]] double at(std::size_t x, std::size_t y) const {
    if (x >= width || y >= height) {
      throw std::out_of_range("BeamFrame::at: index out of range");
    }
    return intensity[y * width + x];
  }

  void set(std::size_t x, std::size_t y, double value) {
    if (x >= width || y >= height) {
      throw std::out_of_range("BeamFrame::set: index out of range");
    }
    intensity[y * width + x] = value;
  }

  void resize(std::size_t w, std::size_t h) {
    width = w;
    height = h;
    intensity.assign(w * h, 0.0);
  }
};

}  // namespace beam::core
