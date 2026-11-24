#pragma once

#include "beam/core/BeamFrame.hpp"
#include "beam/core/Roi.hpp"

#include <optional>

namespace beam::core {

/// Interactive / analysis Area of Interest (AOI). Coordinates are inclusive
/// pixel indices in the full frame. Circle/Ellipse use the bounding box center
/// and half-widths; Rect uses the box itself.
enum class AoiShape {
  Rect = 0,
  Circle = 1,
  Ellipse = 2,
};

struct AoiRegion {
  AoiShape shape = AoiShape::Rect;
  RectRoi bounds{};  ///< Inclusive bounding box

  [[nodiscard]] bool empty() const noexcept { return bounds.empty(); }

  [[nodiscard]] double centerX_px() const noexcept {
    return 0.5 * (static_cast<double>(bounds.x0) + static_cast<double>(bounds.x1));
  }
  [[nodiscard]] double centerY_px() const noexcept {
    return 0.5 * (static_cast<double>(bounds.y0) + static_cast<double>(bounds.y1));
  }
  [[nodiscard]] double radiusX_px() const noexcept {
    return 0.5 * static_cast<double>(bounds.width());
  }
  [[nodiscard]] double radiusY_px() const noexcept {
    return 0.5 * static_cast<double>(bounds.height());
  }

  /// Pixel containment test (continuous coords OK; uses ellipse equation for
  /// Circle/Ellipse, axis-aligned box for Rect).
  [[nodiscard]] bool containsPx(double x, double y) const noexcept;

  [[nodiscard]] std::optional<AoiRegion> clampedTo(std::size_t width,
                                                   std::size_t height) const;
};

/// Zero intensity outside the AOI (same size as input — keeps lab-frame coords).
[[nodiscard]] BeamFrame applyAoiMask(const BeamFrame& frame, const AoiRegion& aoi);

/// Build an elliptical (or circular / rect) AOI around a centroid.
/// Diameters are ISO dσ = 4σ (µm). Half-widths are `scale · dσ / 2`, so the
/// AOI axis lengths equal `scale · dσ` (default 1.5 ≈ tight enclosure around
/// the 4σ beam; raise toward 2–3 for more background margin).
[[nodiscard]] AoiRegion autoAoiFromCentroid(double centroidX_um,
                                            double centroidY_um,
                                            double diameterX_um,
                                            double diameterY_um,
                                            const BeamFrame& frame,
                                            AoiShape shape = AoiShape::Ellipse,
                                            double scale = 1.5);

/// Convert legacy RectRoi → rectangular AOI.
[[nodiscard]] inline AoiRegion aoiFromRect(const RectRoi& roi) {
  AoiRegion a;
  a.shape = AoiShape::Rect;
  a.bounds = roi;
  return a;
}

}  // namespace beam::core
