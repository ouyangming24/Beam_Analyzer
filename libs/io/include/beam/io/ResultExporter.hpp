#pragma once

#include "beam/core/Iso11146.hpp"
#include "beam/core/M2Scan.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Profile.hpp"

#include <string>

namespace beam::io {

struct ExportMeta {
  std::string sourcePath;
  std::string note;
};

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

}  // namespace beam::io
