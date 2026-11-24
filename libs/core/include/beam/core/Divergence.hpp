#pragma once

#include "beam/core/M2Fit.hpp"

namespace beam::core {

/// Beam divergence / BPP helpers derived from a hyperbolic M² fit
/// (engineering diagnostics; not a metrology certificate).
struct DivergenceMetrics {
  double wavelength_nm = 0.0;
  double waistDiameter_um = 0.0;     ///< d0 = 2 w0
  double waistRadius_um = 0.0;       ///< w0
  double rayleigh_mm = 0.0;
  double divergenceFull_mrad = 0.0;  ///< full angle ≈ 2 θ
  double divergenceHalf_mrad = 0.0;  ///< half-angle θ
  double bpp_mm_mrad = 0.0;          ///< Beam Parameter Product = w0(mm)·θ(mrad)
  double m2 = 0.0;
  bool valid = false;
};

/// Extract divergence / BPP from an existing M² fit result.
[[nodiscard]] DivergenceMetrics divergenceFromM2Fit(const M2FitResult& fit,
                                                    double wavelength_nm);

}  // namespace beam::core
