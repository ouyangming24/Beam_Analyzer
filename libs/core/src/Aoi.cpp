#include "beam/core/Aoi.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

bool AoiRegion::containsPx(double x, double y) const noexcept {
  if (empty()) {
    return false;
  }
  switch (shape) {
    case AoiShape::Rect:
      return x >= static_cast<double>(bounds.x0) &&
             x <= static_cast<double>(bounds.x1) + 1e-9 &&
             y >= static_cast<double>(bounds.y0) &&
             y <= static_cast<double>(bounds.y1) + 1e-9;
    case AoiShape::Circle: {
      const double cx = centerX_px();
      const double cy = centerY_px();
      const double r = std::min(radiusX_px(), radiusY_px());
      if (r <= 0.0) {
        return false;
      }
      const double dx = x - cx;
      const double dy = y - cy;
      return (dx * dx + dy * dy) <= (r * r + 1e-9);
    }
    case AoiShape::Ellipse: {
      const double cx = centerX_px();
      const double cy = centerY_px();
      const double rx = radiusX_px();
      const double ry = radiusY_px();
      if (rx <= 0.0 || ry <= 0.0) {
        return false;
      }
      const double nx = (x - cx) / rx;
      const double ny = (y - cy) / ry;
      return (nx * nx + ny * ny) <= 1.0 + 1e-9;
    }
  }
  return false;
}

std::optional<AoiRegion> AoiRegion::clampedTo(std::size_t width,
                                             std::size_t height) const {
  const auto c = bounds.clampedTo(width, height);
  if (!c) {
    return std::nullopt;
  }
  AoiRegion out = *this;
  out.bounds = *c;
  return out;
}

BeamFrame applyAoiMask(const BeamFrame& frame, const AoiRegion& aoi) {
  if (frame.empty()) {
    return {};
  }
  const auto clamped = aoi.clampedTo(frame.width, frame.height);
  if (!clamped || clamped->empty()) {
    BeamFrame zero = frame;
    std::fill(zero.intensity.begin(), zero.intensity.end(), 0.0);
    return zero;
  }

  BeamFrame out = frame;
  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double px = static_cast<double>(x) + 0.5;
      const double py = static_cast<double>(y) + 0.5;
      if (!clamped->containsPx(px, py)) {
        out.intensity[y * frame.width + x] = 0.0;
      }
    }
  }
  return out;
}

AoiRegion autoAoiFromCentroid(double centroidX_um,
                              double centroidY_um,
                              double diameterX_um,
                              double diameterY_um,
                              const BeamFrame& frame,
                              AoiShape shape,
                              double scale) {
  AoiRegion aoi;
  aoi.shape = shape;
  if (frame.empty()) {
    aoi.bounds = {0, 0, 0, 0};
    return aoi;
  }

  const double dx = std::max(1e-9, frame.pixelSizeX_um);
  const double dy = std::max(1e-9, frame.pixelSizeY_um);
  const double cxPx = centroidX_um / dx;
  const double cyPx = centroidY_um / dy;
  double hx = scale * 0.5 * diameterX_um / dx;
  double hy = scale * 0.5 * diameterY_um / dy;
  if (shape == AoiShape::Circle) {
    const double r = std::max(hx, hy);
    hx = r;
    hy = r;
  }
  hx = std::max(1.0, hx);
  hy = std::max(1.0, hy);

  aoi.bounds.x0 = static_cast<int>(std::floor(cxPx - hx));
  aoi.bounds.y0 = static_cast<int>(std::floor(cyPx - hy));
  aoi.bounds.x1 = static_cast<int>(std::ceil(cxPx + hx));
  aoi.bounds.y1 = static_cast<int>(std::ceil(cyPx + hy));
  if (const auto c = aoi.clampedTo(frame.width, frame.height)) {
    return *c;
  }
  aoi.bounds = {0, 0, static_cast<int>(frame.width) - 1,
                static_cast<int>(frame.height) - 1};
  return aoi;
}

}  // namespace beam::core
