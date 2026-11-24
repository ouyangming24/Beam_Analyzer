#pragma once

#include "beam/core/BeamWidths.hpp"
#include "beam/core/CausticCsv.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/M2Scan.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Profile.hpp"

#include <string>
#include <vector>

namespace beam::io {

struct ExportMeta {
  std::string sourcePath;
  std::string note;
  std::string sampleName;
  std::string operatorName;
  std::string deviceId;
  std::string calibrationId;
  std::string environmentNote;
};

[[nodiscard]] inline beam::core::M2ReportMeta toM2ReportMeta(const ExportMeta& meta) {
  beam::core::M2ReportMeta m;
  m.sourcePath = meta.sourcePath;
  m.note = meta.note;
  m.sampleName = meta.sampleName;
  m.operatorName = meta.operatorName;
  m.deviceId = meta.deviceId;
  m.calibrationId = meta.calibrationId;
  m.environmentNote = meta.environmentNote;
  return m;
}

[[nodiscard]] bool exportMetricsJson(const std::string& path,
                                     const beam::core::Iso11146Metrics& metrics,
                                     const ExportMeta& meta = {});

[[nodiscard]] bool exportMetricsCsv(const std::string& path,
                                    const beam::core::Iso11146Metrics& metrics,
                                    const ExportMeta& meta = {});

[[nodiscard]] bool exportPowerJson(const std::string& path,
                                   const beam::core::PowerMetrics& power,
                                   const beam::core::PowerCalibration& cal,
                                   const ExportMeta& meta = {});

[[nodiscard]] bool exportProfileCsv(const std::string& path,
                                    const beam::core::LineProfile& profile,
                                    const ExportMeta& meta = {});

[[nodiscard]] bool exportM2ReportMarkdown(const std::string& path,
                                          const beam::core::M2ScanResult& result,
                                          const ExportMeta& meta = {});

[[nodiscard]] bool exportM2ReportJson(const std::string& path,
                                      const beam::core::M2ScanResult& result,
                                      const ExportMeta& meta = {});

/// Dual-axis residual table: z, measured/fitted/Δ for X and Y.
[[nodiscard]] bool exportM2ResidualsCsv(const std::string& path,
                                        const beam::core::M2ScanResult& result,
                                        const ExportMeta& meta = {});

/// Standalone HTML analysis report (Beam Analyzer branding; printable).
[[nodiscard]] bool exportM2ReportHtml(const std::string& path,
                                      const beam::core::M2ScanResult& result,
                                      const ExportMeta& meta = {});

/// Single-frame analysis snapshot for HTML/Markdown report packages.
struct FrameAnalysisReport {
  ExportMeta meta;
  beam::core::Iso11146Metrics iso;
  beam::core::PowerMetrics power;
  beam::core::PowerCalibration calibration;
  beam::core::BeamWidthSuite widths;
  double pixelSizeX_um = 0.0;
  double pixelSizeY_um = 0.0;
  std::size_t frameWidth = 0;
  std::size_t frameHeight = 0;
  /// Relative paths (or absolute) to embed / link in the report.
  std::string annotatedImageRelPath;
  std::string profileHRelPath;
  std::string profileVRelPath;
};

[[nodiscard]] bool exportFrameAnalysisMarkdown(const std::string& path,
                                               const FrameAnalysisReport& report);

[[nodiscard]] bool exportFrameAnalysisHtml(const std::string& path,
                                           const FrameAnalysisReport& report);

/// Write Markdown + HTML (+ optional JSON metrics) into an output directory prefix.
/// Creates `<prefix>_report.md`, `<prefix>_report.html`, `<prefix>_metrics.json`.
[[nodiscard]] bool exportFrameAnalysisPackage(const std::string& pathPrefix,
                                              const FrameAnalysisReport& report);

}  // namespace beam::io
