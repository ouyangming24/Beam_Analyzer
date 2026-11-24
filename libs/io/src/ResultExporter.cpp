#include "beam/io/ResultExporter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

namespace beam::io {
namespace {

constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

std::string escapeJson(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

void writeFitJson(std::ostream& out, const char* key, const beam::core::M2FitResult& f) {
  out << "  \"" << key << "\": {\n";
  out << "    \"valid\": " << (f.valid ? "true" : "false") << ",\n";
  out << "    \"m2\": " << f.m2 << ",\n";
  out << "    \"m2_uncertainty\": " << f.m2Uncertainty << ",\n";
  out << "    \"waist_diameter_um\": " << f.waistDiameter_um << ",\n";
  out << "    \"waist_diameter_uncertainty_um\": " << f.waistDiameterUncertainty_um << ",\n";
  out << "    \"waist_z_mm\": " << f.waistZ_mm << ",\n";
  out << "    \"waist_z_uncertainty_mm\": " << f.waistZUncertainty_mm << ",\n";
  out << "    \"rayleigh_range_mm\": " << f.rayleighRange_mm << ",\n";
  out << "    \"rms_residual_um\": " << f.rmsResidual << ",\n";
  out << "    \"rms_residual_d2\": " << f.rmsResidualD2 << ",\n";
  out << "    \"relative_rms\": " << f.relativeRms << ",\n";
  out << "    \"quality_flags\": " << f.qualityFlags << ",\n";
  out << "    \"quality_flags_text\": \""
      << escapeJson(beam::core::describeQualityFlags(f.qualityFlags)) << "\",\n";
  out << "    \"residuals\": [\n";
  for (std::size_t i = 0; i < f.residuals.size(); ++i) {
    const auto& r = f.residuals[i];
    out << "      {\"z_mm\": " << r.z_mm << ", \"measured_um\": " << r.measured_um
        << ", \"fitted_um\": " << r.fitted_um << ", \"residual_um\": " << r.residual_um
        << "}";
    if (i + 1 < f.residuals.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "    ],\n";
  out << "    \"message\": \"" << escapeJson(f.message) << "\"\n";
  out << "  }";
}

}  // namespace

bool exportMetricsJson(const std::string& path,
                       const beam::core::Iso11146Metrics& metrics,
                       const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.iso11146.v1\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"valid\": " << (metrics.valid ? "true" : "false") << ",\n";
  out << "  \"total_power\": " << metrics.totalPower << ",\n";
  out << "  \"centroid_x_um\": " << metrics.centroidX_um << ",\n";
  out << "  \"centroid_y_um\": " << metrics.centroidY_um << ",\n";
  out << "  \"diameter_x_um\": " << metrics.diameterX_um << ",\n";
  out << "  \"diameter_y_um\": " << metrics.diameterY_um << ",\n";
  out << "  \"diameter_major_um\": " << metrics.diameterMajor_um << ",\n";
  out << "  \"diameter_minor_um\": " << metrics.diameterMinor_um << ",\n";
  out << "  \"orientation_rad\": " << metrics.orientation_rad << ",\n";
  out << "  \"orientation_deg\": " << (metrics.orientation_rad * kRadToDeg) << ",\n";
  out << "  \"ellipticity\": " << metrics.ellipticity << "\n";
  out << "}\n";
  return static_cast<bool>(out);
}

bool exportMetricsCsv(const std::string& path,
                      const beam::core::Iso11146Metrics& metrics,
                      const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  out << std::setprecision(10);
  out << "key,value\n";
  out << "schema,beam_analyzer.iso11146.v1\n";
  out << "source," << meta.sourcePath << "\n";
  out << "note," << meta.note << "\n";
  out << "valid," << (metrics.valid ? 1 : 0) << "\n";
  out << "total_power," << metrics.totalPower << "\n";
  out << "centroid_x_um," << metrics.centroidX_um << "\n";
  out << "centroid_y_um," << metrics.centroidY_um << "\n";
  out << "diameter_x_um," << metrics.diameterX_um << "\n";
  out << "diameter_y_um," << metrics.diameterY_um << "\n";
  out << "diameter_major_um," << metrics.diameterMajor_um << "\n";
  out << "diameter_minor_um," << metrics.diameterMinor_um << "\n";
  out << "orientation_rad," << metrics.orientation_rad << "\n";
  out << "orientation_deg," << (metrics.orientation_rad * kRadToDeg) << "\n";
  out << "ellipticity," << metrics.ellipticity << "\n";
  return static_cast<bool>(out);
}

bool exportPowerJson(const std::string& path,
                     const beam::core::PowerMetrics& power,
                     const beam::core::PowerCalibration& cal,
                     const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.power.v1\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"valid\": " << (power.valid ? "true" : "false") << ",\n";
  out << "  \"calibrated\": " << (power.calibrated ? "true" : "false") << ",\n";
  out << "  \"sum_counts\": " << power.sumCounts << ",\n";
  out << "  \"peak_counts\": " << power.peakCounts << ",\n";
  out << "  \"mean_counts\": " << power.meanCounts << ",\n";
  out << "  \"power_W\": " << power.power_W << ",\n";
  out << "  \"energy_J\": " << power.energy_J << ",\n";
  out << "  \"calibration\": {\n";
  out << "    \"counts_to_watts\": " << cal.countsToWatts << ",\n";
  out << "    \"exposure_s\": " << cal.exposure_s << ",\n";
  out << "    \"wavelength_nm\": " << cal.wavelength_nm << "\n";
  out << "  }\n";
  out << "}\n";
  return static_cast<bool>(out);
}

bool exportProfileCsv(const std::string& path,
                      const beam::core::LineProfile& profile,
                      const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "# source=" << meta.sourcePath << "\n";
  out << "# note=" << meta.note << "\n";
  out << "position_um,intensity\n";
  const std::size_t n = std::min(profile.position_um.size(), profile.intensity.size());
  for (std::size_t i = 0; i < n; ++i) {
    out << profile.position_um[i] << "," << profile.intensity[i] << "\n";
  }
  return static_cast<bool>(out);
}

bool exportM2ReportMarkdown(const std::string& path,
                            const beam::core::M2ScanResult& result,
                            const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << beam::core::formatM2Report(result);
  if (!meta.sourcePath.empty() || !meta.note.empty()) {
    out << "\nSource: " << meta.sourcePath << "\n";
    out << "Note: " << meta.note << "\n";
  }
  return static_cast<bool>(out);
}

bool exportM2ReportJson(const std::string& path,
                        const beam::core::M2ScanResult& result,
                        const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.m2_scan.v1\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"wavelength_nm\": " << result.wavelength_nm << ",\n";
  out << "  \"valid\": " << (result.valid ? "true" : "false") << ",\n";
  out << "  \"message\": \"" << escapeJson(result.message) << "\",\n";
  out << "  \"points\": [\n";
  for (std::size_t i = 0; i < result.points.size(); ++i) {
    const auto& p = result.points[i];
    out << "    {\"z_mm\": " << p.z_mm << ", \"diameter_x_um\": " << p.diameterX_um
        << ", \"diameter_y_um\": " << p.diameterY_um << ", \"valid\": "
        << (p.valid ? "true" : "false") << ", \"source\": \""
        << escapeJson(p.sourceLabel) << "\"}";
    if (i + 1 < result.points.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ],\n";
  writeFitJson(out, "fit_x", result.fitX);
  out << ",\n";
  writeFitJson(out, "fit_y", result.fitY);
  out << ",\n";
  writeFitJson(out, "fit_combined", result.fitCombined);
  out << "\n}\n";
  return static_cast<bool>(out);
}

bool exportM2ResidualsCsv(const std::string& path,
                          const beam::core::M2ScanResult& result,
                          const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "# source=" << meta.sourcePath << "\n";
  out << "# note=" << meta.note << "\n";
  out << "z_mm,measured_x_um,fitted_x_um,residual_x_um,"
         "measured_y_um,fitted_y_um,residual_y_um\n";

  const auto& rx = result.fitX.residuals;
  const auto& ry = result.fitY.residuals;
  const std::size_t n = std::max(rx.size(), ry.size());
  for (std::size_t i = 0; i < n; ++i) {
    const bool hasX = i < rx.size();
    const bool hasY = i < ry.size();
    const double z = hasX ? rx[i].z_mm : (hasY ? ry[i].z_mm : 0.0);
    out << z << ",";
    if (hasX) {
      out << rx[i].measured_um << "," << rx[i].fitted_um << "," << rx[i].residual_um;
    } else {
      out << ",,";
    }
    out << ",";
    if (hasY) {
      out << ry[i].measured_um << "," << ry[i].fitted_um << "," << ry[i].residual_um;
    } else {
      out << ",,";
    }
    out << "\n";
  }
  return static_cast<bool>(out);
}

}  // namespace beam::io
