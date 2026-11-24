#pragma once

#include "beam/core/M2Scan.hpp"

#include <string>
#include <vector>

namespace beam::core {

/// Result of parsing an offline dual-axis caustic CSV.
struct CausticCsvParseResult {
  std::vector<M2ScanPoint> points;
  bool radiiInput = false;  ///< true if columns were wx_um/wy_um (converted ×2 → diameter)
  bool ok = false;
  std::string message;
};

/// Parse UTF-8 CSV. Accepted headers (case-sensitive preferred):
///   z_mm,Dx_um,Dy_um[,included,note]
///   z_mm,wx_um,wy_um[,included,note]   // radius = D4σ/2 → diameter = 2·w
/// included: 1/0 (default 1). note → sourceLabel when present.
[[nodiscard]] CausticCsvParseResult parseCausticCsv(const std::string& text);

/// Engineering coverage diagnostics for one fitted axis (not ISO certification).
struct M2AxisDiagnostics {
  std::size_t includedCount = 0;
  std::size_t leftOfWaist = 0;
  std::size_t rightOfWaist = 0;
  std::size_t nearWaist = 0;   ///< |z−z0| ≤ zR
  std::size_t farField = 0;    ///< |z−z0| ≥ 2 zR
  double bpp_mm_mrad = 0.0;    ///< w0(mm)·θ(mrad) with θ half-angle
  double divergenceHalf_mrad = 0.0;
  std::vector<std::string> warnings;
};

[[nodiscard]] M2AxisDiagnostics diagnoseM2Axis(const M2FitResult& fit,
                                               const std::vector<M2ScanPoint>& points,
                                               bool useY);

/// Metadata block for polished M² reports (analysis record, not a metrology certificate).
struct M2ReportMeta {
  std::string sampleName;
  std::string operatorName;
  std::string deviceId;
  std::string calibrationId;
  std::string environmentNote;
  std::string sourcePath;
  std::string note;
};

[[nodiscard]] std::string formatM2ReportRich(const M2ScanResult& result,
                                             const M2ReportMeta& meta = {},
                                             const std::string& title = "M² Scan Report");

}  // namespace beam::core
