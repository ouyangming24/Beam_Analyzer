#pragma once

#include <cstddef>
#include <vector>

namespace beam::core {

/// One sample of beam centroid / power for stability analysis.
struct StabilitySample {
  double t_s = 0.0;           ///< Time stamp (seconds, relative OK)
  double centroidX_um = 0.0;  ///< First-moment X (µm)
  double centroidY_um = 0.0;  ///< First-moment Y (µm)
  double power = 0.0;         ///< Optional power / sum (arbitrary units)
};

/// ISO 11670–style positional / pointing stability (engineering preview).
/// Not a claim of accredited ISO 11670 certification.
struct StabilityMetrics {
  std::size_t sampleCount = 0;
  double duration_s = 0.0;

  double meanX_um = 0.0;
  double meanY_um = 0.0;
  double stdX_um = 0.0;   ///< σ_x of centroid
  double stdY_um = 0.0;   ///< σ_y of centroid
  /// Radial positional stability Δr = 2√(σx²+σy²) (common lab reporting form).
  double deltaR_um = 0.0;
  /// Peak-to-peak excursion of radial offset from mean.
  double radialPeakToPeak_um = 0.0;

  double meanPower = 0.0;
  double stdPower = 0.0;
  double powerStability = 0.0;  ///< 1 − σ/μ when μ>0

  /// Angular pointing stability if an effective focal length is supplied (mrad).
  /// θx ≈ σx / f , θy ≈ σy / f  (small-angle, σ in same length unit as f).
  double focalLength_mm = 0.0;
  double pointingStdX_mrad = 0.0;
  double pointingStdY_mrad = 0.0;
  double pointingDeltaR_mrad = 0.0;

  bool valid = false;
};

/// Compute positional stability statistics from a centroid time series.
/// @param focalLength_mm If >0, also fill angular pointing fields (ISO 11670 concept).
[[nodiscard]] StabilityMetrics computeStability(const std::vector<StabilitySample>& samples,
                                                double focalLength_mm = 0.0);

}  // namespace beam::core
