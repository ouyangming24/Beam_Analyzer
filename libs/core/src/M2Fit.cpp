#include "beam/core/M2Fit.hpp"

#include <cmath>
#include <limits>

namespace beam::core {
namespace {

struct HyperbolaParams {
  double d0_um = 0.0;
  double z0_mm = 0.0;
  double a = 0.0;  // (M² λ / (π w0²))² with λ,w0 in consistent units → d² grows with (z-z0)²
};

double modelDiameterSq(const HyperbolaParams& p, double z_mm) {
  const double dz = z_mm - p.z0_mm;
  return p.d0_um * p.d0_um + p.a * dz * dz;
}

}  // namespace

M2FitResult fitM2(const std::vector<M2Sample>& samples, double wavelength_nm) {
  M2FitResult result;
  result.wavelength_nm = wavelength_nm;

  if (samples.size() < 3) {
    result.message = "Need at least 3 axial samples for M² fit";
    return result;
  }
  if (!(wavelength_nm > 0.0)) {
    result.message = "Wavelength must be positive";
    return result;
  }

  for (const auto& s : samples) {
    if (!(s.diameter_um > 0.0) || !std::isfinite(s.diameter_um) || !std::isfinite(s.z_mm)) {
      result.message = "Invalid sample (non-positive diameter or non-finite z)";
      return result;
    }
  }

  // Coarse grid search for (z0, d0), then refine a analytically.
  double zMin = samples.front().z_mm;
  double zMax = samples.front().z_mm;
  double dMin = samples.front().diameter_um;
  for (const auto& s : samples) {
    zMin = std::min(zMin, s.z_mm);
    zMax = std::max(zMax, s.z_mm);
    dMin = std::min(dMin, s.diameter_um);
  }
  if (zMax <= zMin) {
    result.message = "Samples must span a non-zero z range";
    return result;
  }

  HyperbolaParams best;
  double bestRss = std::numeric_limits<double>::infinity();

  const int zSteps = 41;
  const int dSteps = 31;
  for (int iz = 0; iz < zSteps; ++iz) {
    const double z0 = zMin + (zMax - zMin) * static_cast<double>(iz) / (zSteps - 1);
    for (int id = 0; id < dSteps; ++id) {
      const double d0 = dMin * (0.5 + 1.5 * static_cast<double>(id) / (dSteps - 1));
      // Solve least-squares for a: min Σ (d²_i - d0² - a·dz²)²
      double num = 0.0;
      double den = 0.0;
      for (const auto& s : samples) {
        const double dz = s.z_mm - z0;
        const double dz2 = dz * dz;
        const double y = s.diameter_um * s.diameter_um - d0 * d0;
        num += dz2 * y;
        den += dz2 * dz2;
      }
      if (den <= 0.0) {
        continue;
      }
      const double a = std::max(0.0, num / den);
      double rss = 0.0;
      for (const auto& s : samples) {
        const double pred = d0 * d0 + a * (s.z_mm - z0) * (s.z_mm - z0);
        const double err = s.diameter_um * s.diameter_um - pred;
        rss += err * err;
      }
      if (rss < bestRss) {
        bestRss = rss;
        best = {d0, z0, a};
      }
    }
  }

  if (!std::isfinite(bestRss) || best.d0_um <= 0.0) {
    result.message = "M² fit failed to converge";
    return result;
  }

  // Convert a → M².
  // w²(z) = w0² [1 + (M² λ (z−z0)/(π w0²))²], d = 2w ⇒
  // d²(z) = d0² + d0² (M² λ /(π w0²))² (z−z0)²
  // Fit uses z in mm → a has units µm²/mm²; convert to µm: a_um = a / 1e6.
  // sqrt(a_um) = d0 · M² λ /(π w0²)  ⇒  M² = sqrt(a_um) · π w0² / (λ · d0)
  const double w0_um = best.d0_um * 0.5;
  const double lambda_um = wavelength_nm * 1e-3;
  const double a_per_um2 = best.a / 1.0e6;  // mm→µm: / (1000)²
  const double slope = std::sqrt(std::max(0.0, a_per_um2));
  const double m2 =
      (std::acos(-1.0) * w0_um * w0_um / lambda_um) * (slope / best.d0_um);

  result.m2 = m2;
  result.waistDiameter_um = best.d0_um;
  result.waistZ_mm = best.z0_mm;
  result.rmsResidual = std::sqrt(bestRss / static_cast<double>(samples.size()));
  result.valid = std::isfinite(m2) && m2 > 0.0;
  result.message = result.valid ? "OK" : "Non-physical M²";
  return result;
}

}  // namespace beam::core
