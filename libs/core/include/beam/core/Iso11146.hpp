#pragma once

#include "beam/core/BeamFrame.hpp"

namespace beam::core {

/// ISO 11146 second-moment beam metrics for one frame.
struct Iso11146Metrics {
  double totalPower = 0.0;       ///< Sum of intensity (arbitrary units)
  double centroidX_um = 0.0;     ///< First-moment centroid X (µm)
  double centroidY_um = 0.0;     ///< First-moment centroid Y (µm)

  /// Lab-frame second-moment beam diameters dσ = 4σ (µm)
  double diameterX_um = 0.0;
  double diameterY_um = 0.0;

  /// Principal-axis diameters after covariance diagonalization (µm)
  double diameterMajor_um = 0.0;
  double diameterMinor_um = 0.0;

  /// Orientation of major axis (radians, CCW from +X)
  double orientation_rad = 0.0;

  /// Ellipticity ε = d_minor / d_major in [0, 1]
  double ellipticity = 0.0;

  bool valid = false;
};

/// Compute ISO 11146 first/second-moment metrics.
/// Optionally clips negative intensities to zero before moments.
[[nodiscard]] Iso11146Metrics computeIso11146(const BeamFrame& frame,
                                              bool clipNegative = true);

/// Build a synthetic 2D Gaussian for tests / SimulatedCamera.
[[nodiscard]] BeamFrame makeGaussianFrame(std::size_t width,
                                          std::size_t height,
                                          double centerX_px,
                                          double centerY_px,
                                          double sigmaX_px,
                                          double sigmaY_px,
                                          double amplitude = 1.0,
                                          double pixelSizeX_um = 1.0,
                                          double pixelSizeY_um = 1.0,
                                          double background = 0.0);

}  // namespace beam::core
