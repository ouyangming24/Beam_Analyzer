#pragma once

#include "beam/core/Iso11146.hpp"

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

}  // namespace beam::io
