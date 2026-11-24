#pragma once

#include <optional>
#include <string>
#include <vector>

namespace beam::core {

/// One axial measurement of beam diameter (µm) at position z (mm).
struct M2Sample {
  double z_mm = 0.0;
  double diameter_um = 0.0;
};

/// Result of hyperbolic M² fit: d²(z) = d0² [1 + (M² λ (z−z0)/(π w0²))²]
/// with w0 = d0/2. Phase 1 provides the interface + a basic least-squares fit.
struct M2FitResult {
  double m2 = 1.0;
  double waistDiameter_um = 0.0;
  double waistZ_mm = 0.0;
  double wavelength_nm = 1064.0;
  double rmsResidual = 0.0;
  bool valid = false;
  std::string message;
};

/// Fit M² from ≥3 diameter samples along z.
/// Wavelength is required to convert Rayleigh range to M².
[[nodiscard]] M2FitResult fitM2(const std::vector<M2Sample>& samples,
                                double wavelength_nm);

}  // namespace beam::core
