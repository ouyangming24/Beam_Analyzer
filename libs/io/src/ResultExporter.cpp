#include "beam/io/ResultExporter.hpp"

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

}  // namespace beam::io
