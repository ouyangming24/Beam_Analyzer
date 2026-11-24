#pragma once

#include "beam/core/BeamFrame.hpp"

#include <cmath>

namespace beam::core {

/// Placeholder radiometric calibration: counts → watts / joules.
/// Phase 2 ships hooks only; real sensor cal belongs with the camera SDK.
struct PowerCalibration {
  /// Multiplier: power_W = sum(counts) * countsToWatts.
  /// 0 → report uncalibrated counts only.
  double countsToWatts = 0.0;

  /// Exposure / integration time used when converting power → energy.
  double exposure_s = 1.0;

  /// Optional wavelength label for reports (nm); unused in conversion.
  double wavelength_nm = 1064.0;

  [[nodiscard]] bool isCalibrated() const noexcept {
    return countsToWatts > 0.0 && std::isfinite(countsToWatts);
  }
};

struct PowerMetrics {
  double sumCounts = 0.0;
  double peakCounts = 0.0;
  double meanCounts = 0.0;
  double power_W = 0.0;   ///< Valid when calibrated
  double energy_J = 0.0;  ///< power_W * exposure_s when calibrated
  bool calibrated = false;
  bool valid = false;
};

/// Integrate intensity over the frame (after any ROI/background prep).
[[nodiscard]] PowerMetrics computePowerMetrics(const BeamFrame& frame,
                                               const PowerCalibration& cal = {});

}  // namespace beam::core
