#include "beam/core/M2Fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace beam::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct HyperbolaParams {
  double d0_um = 0.0;
  double z0_mm = 0.0;
  double a = 0.0;  // µm²/mm² term: d² = d0² + a (z-z0)²
};

double modelDiameterSq(const HyperbolaParams& p, double z_mm) {
  const double dz = z_mm - p.z0_mm;
  return p.d0_um * p.d0_um + p.a * dz * dz;
}

double solveA(const std::vector<M2Sample>& samples, double z0, double d0) {
  double num = 0.0;
  double den = 0.0;
  for (const auto& s : samples) {
    const double w = std::max(0.0, s.weight);
    if (w <= 0.0) {
      continue;
    }
    const double dz = s.z_mm - z0;
    const double dz2 = dz * dz;
    const double y = s.diameter_um * s.diameter_um - d0 * d0;
    num += w * dz2 * y;
    den += w * dz2 * dz2;
  }
  if (den <= 0.0) {
    return 0.0;
  }
  return std::max(0.0, num / den);
}

double rssD2(const std::vector<M2Sample>& samples, const HyperbolaParams& p) {
  double rss = 0.0;
  for (const auto& s : samples) {
    const double w = std::max(0.0, s.weight);
    if (w <= 0.0) {
      continue;
    }
    const double err = s.diameter_um * s.diameter_um - modelDiameterSq(p, s.z_mm);
    rss += w * err * err;
  }
  return rss;
}

HyperbolaParams coarseSearch(const std::vector<M2Sample>& samples,
                             double zMin,
                             double zMax,
                             double dMin) {
  HyperbolaParams best;
  double bestRss = std::numeric_limits<double>::infinity();

  const int zSteps = 61;
  const int dSteps = 41;
  for (int iz = 0; iz < zSteps; ++iz) {
    const double z0 = zMin + (zMax - zMin) * static_cast<double>(iz) / (zSteps - 1);
    for (int id = 0; id < dSteps; ++id) {
      const double d0 = dMin * (0.4 + 1.8 * static_cast<double>(id) / (dSteps - 1));
      const double a = solveA(samples, z0, d0);
      HyperbolaParams cand{d0, z0, a};
      const double rss = rssD2(samples, cand);
      if (rss < bestRss) {
        bestRss = rss;
        best = cand;
      }
    }
  }
  return best;
}

HyperbolaParams refineLocal(const std::vector<M2Sample>& samples,
                            HyperbolaParams seed,
                            double zSpan,
                            double dMin) {
  HyperbolaParams best = seed;
  double bestRss = rssD2(samples, best);

  double zHalf = std::max(1e-6, 0.15 * zSpan);
  double dHalf = std::max(1e-6, 0.15 * std::max(dMin, seed.d0_um));

  for (int pass = 0; pass < 4; ++pass) {
    const int zSteps = 21;
    const int dSteps = 21;
    for (int iz = 0; iz < zSteps; ++iz) {
      const double z0 =
          best.z0_mm - zHalf + 2.0 * zHalf * static_cast<double>(iz) / (zSteps - 1);
      for (int id = 0; id < dSteps; ++id) {
        const double d0 =
            best.d0_um - dHalf + 2.0 * dHalf * static_cast<double>(id) / (dSteps - 1);
        if (!(d0 > 0.0)) {
          continue;
        }
        const double a = solveA(samples, z0, d0);
        HyperbolaParams cand{d0, z0, a};
        const double rss = rssD2(samples, cand);
        if (rss < bestRss) {
          bestRss = rss;
          best = cand;
        }
      }
    }
    zHalf *= 0.4;
    dHalf *= 0.4;
  }
  return best;
}

double m2FromParams(const HyperbolaParams& p, double wavelength_nm) {
  const double w0_um = p.d0_um * 0.5;
  const double lambda_um = wavelength_nm * 1e-3;
  const double a_per_um2 = p.a / 1.0e6;  // mm→µm
  const double slope = std::sqrt(std::max(0.0, a_per_um2));
  return (kPi * w0_um * w0_um / lambda_um) * (slope / p.d0_um);
}

double rayleighFromM2(double d0_um, double m2, double wavelength_nm) {
  const double w0_um = d0_um * 0.5;
  const double lambda_um = wavelength_nm * 1e-3;
  if (!(m2 > 0.0) || !(w0_um > 0.0) || !(lambda_um > 0.0)) {
    return 0.0;
  }
  const double zR_um = kPi * w0_um * w0_um / (m2 * lambda_um);
  return zR_um / 1000.0;
}

}  // namespace

double hyperbolicDiameter_um(double z_mm,
                             double waistDiameter_um,
                             double waistZ_mm,
                             double m2,
                             double wavelength_nm) {
  const double zR = rayleighFromM2(waistDiameter_um, m2, wavelength_nm);
  if (!(zR > 0.0)) {
    return waistDiameter_um;
  }
  const double u = (z_mm - waistZ_mm) / zR;
  return waistDiameter_um * std::sqrt(1.0 + u * u);
}

M2FitResult fitM2(const std::vector<M2Sample>& samples,
                  double wavelength_nm,
                  const M2FitOptions& options) {
  M2FitResult result;
  result.wavelength_nm = wavelength_nm;

  if (samples.size() < 3) {
    result.qualityFlags |= M2QualityTooFewPoints;
    result.message = "Need at least 3 axial samples for M² fit";
    return result;
  }
  if (!(wavelength_nm > 0.0)) {
    result.message = "Wavelength must be positive";
    return result;
  }

  std::vector<M2Sample> clean;
  clean.reserve(samples.size());
  bool weightIssue = false;
  for (const auto& s : samples) {
    if (!(s.diameter_um > 0.0) || !std::isfinite(s.diameter_um) || !std::isfinite(s.z_mm)) {
      result.message = "Invalid sample (non-positive diameter or non-finite z)";
      result.qualityFlags |= M2QualityLowWeightOrNaN;
      return result;
    }
    if (!(s.weight >= 0.0) || !std::isfinite(s.weight)) {
      weightIssue = true;
      continue;
    }
    if (s.weight == 0.0) {
      weightIssue = true;
      continue;
    }
    clean.push_back(s);
  }
  if (weightIssue) {
    result.qualityFlags |= M2QualityLowWeightOrNaN;
  }
  if (clean.size() < 3) {
    result.qualityFlags |= M2QualityTooFewPoints;
    result.message = "Need at least 3 valid weighted samples";
    return result;
  }
  if (clean.size() < options.recommendedMinPoints) {
    result.qualityFlags |= M2QualityTooFewPoints;
  }

  double zMin = clean.front().z_mm;
  double zMax = clean.front().z_mm;
  double dMin = clean.front().diameter_um;
  double dSum = 0.0;
  for (const auto& s : clean) {
    zMin = std::min(zMin, s.z_mm);
    zMax = std::max(zMax, s.z_mm);
    dMin = std::min(dMin, s.diameter_um);
    dSum += s.diameter_um;
  }
  const double zSpan = zMax - zMin;
  if (!(zSpan > 0.0)) {
    result.message = "Samples must span a non-zero z range";
    return result;
  }
  const double meanD = dSum / static_cast<double>(clean.size());

  HyperbolaParams best = coarseSearch(clean, zMin, zMax, dMin);
  if (options.refineLocally) {
    best = refineLocal(clean, best, zSpan, dMin);
  }

  if (!(best.d0_um > 0.0) || !std::isfinite(best.d0_um)) {
    result.message = "M² fit failed to converge";
    return result;
  }

  const double m2 = m2FromParams(best, wavelength_nm);
  const double zR = rayleighFromM2(best.d0_um, m2, wavelength_nm);

  // Residuals
  result.residuals.reserve(clean.size());
  double rssD = 0.0;
  double rssD2v = 0.0;
  double wSum = 0.0;
  for (const auto& s : clean) {
    const double fittedSq = modelDiameterSq(best, s.z_mm);
    const double fitted = std::sqrt(std::max(0.0, fittedSq));
    M2FitResidual r;
    r.z_mm = s.z_mm;
    r.measured_um = s.diameter_um;
    r.fitted_um = fitted;
    r.residual_um = s.diameter_um - fitted;
    r.residual_d2 = s.diameter_um * s.diameter_um - fittedSq;
    const double dz = s.z_mm - best.z0_mm;
    r.leverage = (dz * dz) / (zSpan * zSpan + 1e-30);
    result.residuals.push_back(r);
    rssD += s.weight * r.residual_um * r.residual_um;
    rssD2v += s.weight * r.residual_d2 * r.residual_d2;
    wSum += s.weight;
  }
  const double nEff = std::max(1.0, wSum);
  result.rmsResidual = std::sqrt(rssD / nEff);
  result.rmsResidualD2 = std::sqrt(rssD2v / nEff);
  result.relativeRms = (meanD > 0.0) ? (result.rmsResidual / meanD) : 0.0;

  result.m2 = m2;
  result.waistDiameter_um = best.d0_um;
  result.waistZ_mm = best.z0_mm;
  result.rayleighRange_mm = zR;

  // Quality flags
  if (result.relativeRms > options.highResidualRel) {
    result.qualityFlags |= M2QualityHighResidual;
  }
  if (!(m2 >= 0.9) || !std::isfinite(m2)) {
    result.qualityFlags |= M2QualityNonPhysicalM2;
  }
  if (zR > 0.0 && zSpan < options.minSpanFracZR * zR) {
    result.qualityFlags |= M2QualityNarrowZSpan;
  }
  if (zR > 0.0) {
    bool nearWaist = false;
    int left = 0;
    int right = 0;
    for (const auto& s : clean) {
      const double dz = s.z_mm - best.z0_mm;
      if (std::abs(dz) <= options.nearWaistFracZR * zR) {
        nearWaist = true;
      }
      if (dz < 0.0) {
        ++left;
      } else if (dz > 0.0) {
        ++right;
      }
    }
    if (!nearWaist) {
      result.qualityFlags |= M2QualityNoNearWaist;
    }
    if ((left == 0 && right > 0) || (right == 0 && left > 0)) {
      result.qualityFlags |= M2QualityAsymmetricCoverage;
    }
  }

  // Uncertainty heuristic: finite-difference curvature of RSS around optimum.
  if (options.computeUncertainty && clean.size() >= 3) {
    const double baseRss = rssD2(clean, best);
    const double dof = std::max(1.0, static_cast<double>(clean.size()) - 3.0);
    const double sigma2 = baseRss / dof;

    const double epsZ = std::max(1e-4, 0.01 * zSpan);
    const double epsD = std::max(1e-4, 0.01 * best.d0_um);

    HyperbolaParams pzPlus = best;
    pzPlus.z0_mm += epsZ;
    pzPlus.a = solveA(clean, pzPlus.z0_mm, pzPlus.d0_um);
    HyperbolaParams pzMinus = best;
    pzMinus.z0_mm -= epsZ;
    pzMinus.a = solveA(clean, pzMinus.z0_mm, pzMinus.d0_um);
    const double d2Rss_dz2 =
        (rssD2(clean, pzPlus) - 2.0 * baseRss + rssD2(clean, pzMinus)) / (epsZ * epsZ);

    HyperbolaParams pdPlus = best;
    pdPlus.d0_um += epsD;
    pdPlus.a = solveA(clean, pdPlus.z0_mm, pdPlus.d0_um);
    HyperbolaParams pdMinus = best;
    pdMinus.d0_um = std::max(1e-9, best.d0_um - epsD);
    pdMinus.a = solveA(clean, pdMinus.z0_mm, pdMinus.d0_um);
    const double d2Rss_dd2 =
        (rssD2(clean, pdPlus) - 2.0 * baseRss + rssD2(clean, pdMinus)) / (epsD * epsD);

    const double epsM = std::max(1e-4, 0.02 * std::max(1.0, m2));
    auto paramsFromM2 = [&](double m2Try) {
      HyperbolaParams p = best;
      const double w0 = p.d0_um * 0.5;
      const double lambda_um = wavelength_nm * 1e-3;
      const double slope = (m2Try * lambda_um) / (kPi * w0 * w0) * p.d0_um;
      p.a = slope * slope * 1.0e6;
      return p;
    };
    const double rssMplus = rssD2(clean, paramsFromM2(m2 + epsM));
    const double rssMminus = rssD2(clean, paramsFromM2(std::max(1e-6, m2 - epsM)));
    const double d2Rss_dm2 = (rssMplus - 2.0 * baseRss + rssMminus) / (epsM * epsM);

    auto uncFromCurv = [&](double curv) {
      if (!(curv > 0.0) || !std::isfinite(curv) || !(sigma2 > 0.0)) {
        return 0.0;
      }
      // Var ≈ 2 σ² / (d²RSS/dp²) for least-squares (rough)
      return std::sqrt(std::max(0.0, 2.0 * sigma2 / curv));
    };

    result.waistZUncertainty_mm = uncFromCurv(d2Rss_dz2);
    result.waistDiameterUncertainty_um = uncFromCurv(d2Rss_dd2);
    result.m2Uncertainty = uncFromCurv(d2Rss_dm2);
  }

  result.valid = std::isfinite(m2) && m2 > 0.0;
  if (!result.valid) {
    result.message = "Non-physical M²";
  } else if (result.qualityFlags != M2QualityOk) {
    std::ostringstream ss;
    ss << "OK with quality flags=0x" << std::hex << result.qualityFlags;
    result.message = ss.str();
  } else {
    result.message = "OK";
  }
  return result;
}

}  // namespace beam::core
