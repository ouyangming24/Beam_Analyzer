#include "beam/core/Divergence.hpp"

#include <cmath>

namespace beam::core {

DivergenceMetrics divergenceFromM2Fit(const M2FitResult& fit, double wavelength_nm) {
  DivergenceMetrics out;
  out.wavelength_nm = wavelength_nm > 0.0 ? wavelength_nm : fit.wavelength_nm;
  out.m2 = fit.m2;
  out.waistDiameter_um = fit.waistDiameter_um;
  out.waistRadius_um = 0.5 * fit.waistDiameter_um;
  out.rayleigh_mm = fit.rayleighRange_mm;

  if (!fit.valid || !(fit.waistDiameter_um > 0.0) || !(fit.rayleighRange_mm > 0.0)) {
    return out;
  }

  // Embedded Gaussian: θ = w0 / zR (half-angle, rad) with consistent units.
  // w0 in mm, zR in mm → θ in rad → ·1000 = mrad.
  const double w0_mm = out.waistRadius_um / 1000.0;
  const double theta_rad = w0_mm / fit.rayleighRange_mm;
  out.divergenceHalf_mrad = theta_rad * 1000.0;
  out.divergenceFull_mrad = 2.0 * out.divergenceHalf_mrad;
  out.bpp_mm_mrad = w0_mm * out.divergenceHalf_mrad;
  out.valid = true;
  return out;
}

}  // namespace beam::core
