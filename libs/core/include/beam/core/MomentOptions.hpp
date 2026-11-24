#pragma once

#include "beam/core/Roi.hpp"

namespace beam::core {

/// How to treat detector noise / low-level pixels before second moments.
/// Lab practice and ISO 11146 workflows typically subtract a background estimate,
/// then optionally suppress residual noise with a threshold (never replace the
/// ISO dσ = 4σ definition itself).
enum class NoiseFloorMode {
  None,               ///< No additional noise gate after background prep
  Absolute,           ///< Zero pixels with I < absoluteLevel
  FractionOfPeak,     ///< Zero pixels with I < fractionOfPeak * I_peak
  SigmaAboveBg,       ///< Zero pixels with I < noiseSigmaMult * estimated σ_noise
};

struct NoiseFloorOptions {
  NoiseFloorMode mode = NoiseFloorMode::None;
  double absoluteLevel = 0.0;     ///< For Absolute
  double fractionOfPeak = 0.01;   ///< For FractionOfPeak (e.g. 1% of peak)
  double noiseSigmaMult = 3.0;    ///< For SigmaAboveBg
  /// Optional: known detector noise σ in intensity units (if ≤0, estimate from
  /// border strip when possible).
  double knownNoiseSigma = 0.0;
};

/// Full second-moment computation options (background + noise + clip).
struct MomentOptions {
  AnalysisPrep prep{};             ///< ROI + background (see Roi.hpp)
  NoiseFloorOptions noise{};
  bool clipNegative = true;        ///< Clip I < 0 after prep / noise gate
  /// If true, recompute centroid after noise gating (two-pass ISO style).
  bool refineCentroidAfterGate = true;
};

}  // namespace beam::core
