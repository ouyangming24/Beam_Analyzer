#pragma once

#include "beam/core/BeamFrame.hpp"

namespace beam::core {

/// ISO 13694–style beam intensity distribution metrics (engineering preview).
/// Not a claim of accredited ISO 13694 certification.
struct Iso13694Metrics {
  /// Threshold fraction η of peak used for the effective irradiation area A_η.
  double eta = 0.5;
  /// Peak fluence / intensity in the frame (same units as input).
  double peak = 0.0;
  /// Average intensity over pixels with I ≥ η·peak (effective area).
  double averageOnEffective = 0.0;
  /// Min intensity on the effective area.
  double minOnEffective = 0.0;
  /// RMS of intensity on the effective area.
  double rmsOnEffective = 0.0;
  /// Effective irradiation area A_η (µm²).
  double effectiveArea_um2 = 0.0;
  /// Flatness factor F = peak / averageOnEffective (≥ 1). ISO 13694 concept.
  double flatnessFactor = 0.0;
  /// Normalized flatness (display) = average / peak ∈ (0, 1].
  double flatnessNormalized = 0.0;
  /// Plateau uniformity U = 1 − σ/μ on the effective area (clamped to [0, 1]).
  double uniformity = 0.0;
  /// Alternative edge-style uniformity: 1 − (max−min)/(max+min) on A_η.
  double uniformityMinMax = 0.0;
  std::size_t pixelsEffective = 0;
  bool valid = false;
};

/// Compute ISO 13694–concept flatness / uniformity on a prepared intensity frame.
/// @param eta Fraction of peak defining the effective irradiation area (default 0.5).
[[nodiscard]] Iso13694Metrics computeIso13694(const BeamFrame& frame,
                                              double eta = 0.5);

}  // namespace beam::core
