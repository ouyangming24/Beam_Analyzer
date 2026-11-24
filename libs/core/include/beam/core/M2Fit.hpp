#pragma once

#include <string>
#include <vector>

namespace beam::core {

/// One axial measurement of beam diameter (µm) at position z (mm).
struct M2Sample {
  double z_mm = 0.0;
  double diameter_um = 0.0;
  double weight = 1.0;  ///< Relative weight for weighted least squares (≥0)
};

/// Quality / rigor flags for commercial-grade M² reporting (bitmask).
enum M2QualityFlag : unsigned {
  M2QualityOk = 0,
  M2QualityTooFewPoints = 1u << 0,       ///< < 5 recommended ISO-style stations
  M2QualityNarrowZSpan = 1u << 1,        ///< Span ≪ estimated Rayleigh range
  M2QualityNoNearWaist = 1u << 2,        ///< No sample within ~0.5 zR of waist
  M2QualityHighResidual = 1u << 3,       ///< Relative residual above threshold
  M2QualityNonPhysicalM2 = 1u << 4,      ///< M² < ~0.9 or non-finite
  M2QualityAsymmetricCoverage = 1u << 5, ///< Points mostly on one side of waist
  M2QualityLowWeightOrNaN = 1u << 6,     ///< Invalid weights / samples dropped
};

/// Per-sample residual after hyperbolic fit (d² space and diameter space).
struct M2FitResidual {
  double z_mm = 0.0;
  double measured_um = 0.0;
  double fitted_um = 0.0;
  double residual_um = 0.0;       ///< measured − fitted (diameter)
  double residual_d2 = 0.0;       ///< measured² − fitted²
  double leverage = 0.0;          ///< Approximate influence (normalized)
};

/// Result of hyperbolic M² fit:
/// d²(z) = d0² [1 + ((z−z0)/zR)²],  zR = π w0² / (M² λ),  w0 = d0/2.
struct M2FitResult {
  double m2 = 1.0;
  double m2Uncertainty = 0.0;  ///< 1σ-ish from residual / curvature heuristic
  double waistDiameter_um = 0.0;
  double waistDiameterUncertainty_um = 0.0;
  double waistZ_mm = 0.0;
  double waistZUncertainty_mm = 0.0;
  double rayleighRange_mm = 0.0;
  double wavelength_nm = 1064.0;
  double rmsResidual = 0.0;          ///< RMS of diameter residual (µm)
  double rmsResidualD2 = 0.0;        ///< RMS of d² residual (µm²)
  double relativeRms = 0.0;          ///< rmsResidual / mean(d)
  unsigned qualityFlags = M2QualityOk;
  std::vector<M2FitResidual> residuals;
  bool valid = false;
  std::string message;
};

struct M2FitOptions {
  /// Minimum recommended points for a “rigorous” flag (ISO lab practice ≈5+).
  std::size_t recommendedMinPoints = 5;
  /// Relative RMS threshold for HighResidual (fraction of mean diameter).
  double highResidualRel = 0.05;
  /// Require at least one sample within this fraction of zR from waist.
  double nearWaistFracZR = 0.5;
  /// Flag NarrowZSpan when total |z| span < this × estimated zR.
  double minSpanFracZR = 1.5;
  bool computeUncertainty = true;
  bool refineLocally = true;  ///< Local Nelder-ish / grid refine around coarse best
};

/// Fit M² from ≥3 diameter samples along z.
[[nodiscard]] M2FitResult fitM2(const std::vector<M2Sample>& samples,
                                double wavelength_nm,
                                const M2FitOptions& options = {});

/// Ideal TEM00 / hyperbolic diameter at z for known M² (synthetic helpers).
[[nodiscard]] double hyperbolicDiameter_um(double z_mm,
                                           double waistDiameter_um,
                                           double waistZ_mm,
                                           double m2,
                                           double wavelength_nm);

}  // namespace beam::core
