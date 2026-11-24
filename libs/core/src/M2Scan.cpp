#include "beam/core/M2Scan.hpp"

#include <cmath>
#include <sstream>
#include <vector>

namespace beam::core {

std::string describeQualityFlags(unsigned flags) {
  if (flags == M2QualityOk) {
    return "none";
  }
  std::vector<const char*> names;
  if (flags & M2QualityTooFewPoints) {
    names.push_back("too_few_points");
  }
  if (flags & M2QualityNarrowZSpan) {
    names.push_back("narrow_z_span");
  }
  if (flags & M2QualityNoNearWaist) {
    names.push_back("no_near_waist");
  }
  if (flags & M2QualityHighResidual) {
    names.push_back("high_residual");
  }
  if (flags & M2QualityNonPhysicalM2) {
    names.push_back("non_physical_m2");
  }
  if (flags & M2QualityAsymmetricCoverage) {
    names.push_back("asymmetric_coverage");
  }
  if (flags & M2QualityLowWeightOrNaN) {
    names.push_back("low_weight_or_nan");
  }
  std::ostringstream ss;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i) {
      ss << ',';
    }
    ss << names[i];
  }
  return ss.str();
}

M2ScanResult analyzeM2Scan(const std::vector<M2ScanPoint>& points,
                           double wavelength_nm,
                           const M2FitOptions& fitOptions) {
  M2ScanResult out;
  out.wavelength_nm = wavelength_nm;
  out.points = points;
  out.multiAxis = true;

  std::vector<M2Sample> sx;
  std::vector<M2Sample> sy;
  std::vector<M2Sample> sc;
  sx.reserve(points.size());
  sy.reserve(points.size());
  sc.reserve(points.size());

  for (const auto& p : points) {
    if (!p.valid) {
      continue;
    }
    if (!(p.diameterX_um > 0.0) || !(p.diameterY_um > 0.0) || !std::isfinite(p.z_mm)) {
      continue;
    }
    sx.push_back({p.z_mm, p.diameterX_um, 1.0});
    sy.push_back({p.z_mm, p.diameterY_um, 1.0});
    const double dComb =
        std::sqrt(0.5 * (p.diameterX_um * p.diameterX_um + p.diameterY_um * p.diameterY_um));
    sc.push_back({p.z_mm, dComb, 1.0});
  }

  if (sx.size() < 3) {
    out.message = "Need at least 3 valid axial points";
    return out;
  }

  out.fitX = fitM2(sx, wavelength_nm, fitOptions);
  out.fitY = fitM2(sy, wavelength_nm, fitOptions);
  out.fitCombined = fitM2(sc, wavelength_nm, fitOptions);
  out.valid = out.fitX.valid || out.fitY.valid || out.fitCombined.valid;
  out.message = out.valid ? "OK" : "Fit failed";
  return out;
}

M2ScanResult analyzeM2ScanSingleAxis(const std::vector<M2ScanPoint>& points,
                                     double wavelength_nm,
                                     bool useY,
                                     const M2FitOptions& fitOptions) {
  M2ScanResult out;
  out.wavelength_nm = wavelength_nm;
  out.points = points;
  out.multiAxis = false;

  std::vector<M2Sample> s;
  s.reserve(points.size());
  for (const auto& p : points) {
    if (!p.valid || !std::isfinite(p.z_mm)) {
      continue;
    }
    const double d = useY ? p.diameterY_um : p.diameterX_um;
    if (!(d > 0.0)) {
      continue;
    }
    s.push_back({p.z_mm, d, 1.0});
  }
  if (s.size() < 3) {
    out.message = "Need at least 3 valid axial points";
    return out;
  }
  auto fit = fitM2(s, wavelength_nm, fitOptions);
  if (useY) {
    out.fitY = fit;
  } else {
    out.fitX = fit;
  }
  out.fitCombined = fit;
  out.valid = fit.valid;
  out.message = fit.message;
  return out;
}

std::string formatM2Report(const M2ScanResult& result, const std::string& title) {
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(6);

  ss << "# " << title << "\n\n";
  ss << "Wavelength: " << result.wavelength_nm << " nm\n";
  ss << "Points: " << result.points.size() << "\n";
  ss << "Multi-axis: " << (result.multiAxis ? "yes" : "no") << "\n";
  ss << "Valid: " << (result.valid ? "yes" : "no") << "\n";
  if (!result.message.empty()) {
    ss << "Status: " << result.message << "\n";
  }
  ss << "\n## Axial samples\n\n";
  ss << "| # | z (mm) | Dx (µm) | Dy (µm) | source |\n";
  ss << "|---|--------|---------|---------|--------|\n";
  for (std::size_t i = 0; i < result.points.size(); ++i) {
    const auto& p = result.points[i];
    ss << "| " << (i + 1) << " | " << p.z_mm << " | " << p.diameterX_um << " | "
       << p.diameterY_um << " | " << p.sourceLabel << " |\n";
  }

  auto dumpFit = [&](const char* name, const M2FitResult& f) {
    ss << "\n## Fit " << name << "\n\n";
    ss << "- valid: " << (f.valid ? "yes" : "no") << "\n";
    ss << "- M²: " << f.m2 << " ± " << f.m2Uncertainty << "\n";
    ss << "- waist diameter (µm): " << f.waistDiameter_um << " ± "
       << f.waistDiameterUncertainty_um << "\n";
    ss << "- waist z (mm): " << f.waistZ_mm << " ± " << f.waistZUncertainty_mm << "\n";
    ss << "- Rayleigh range zR (mm): " << f.rayleighRange_mm << "\n";
    ss << "- RMS residual (µm): " << f.rmsResidual << "\n";
    ss << "- RMS residual d² (µm²): " << f.rmsResidualD2 << "\n";
    ss << "- relative RMS: " << f.relativeRms << "\n";
    ss << "- quality flags: 0x" << std::hex << f.qualityFlags << std::dec << " ("
       << describeQualityFlags(f.qualityFlags) << ")\n";
    if (!f.message.empty()) {
      ss << "- message: " << f.message << "\n";
    }
    if (!f.residuals.empty()) {
      ss << "\n### Residuals\n\n";
      ss << "| z (mm) | measured (µm) | fitted (µm) | residual (µm) |\n";
      ss << "|--------|---------------|-------------|---------------|\n";
      for (const auto& r : f.residuals) {
        ss << "| " << r.z_mm << " | " << r.measured_um << " | " << r.fitted_um << " | "
           << r.residual_um << " |\n";
      }
    }
  };

  dumpFit("X", result.fitX);
  dumpFit("Y", result.fitY);
  dumpFit("combined", result.fitCombined);
  ss << "\n---\nGenerated by Beam Analyzer (ISO 11146-style M² workflow)\n";
  return ss.str();
}

}  // namespace beam::core
