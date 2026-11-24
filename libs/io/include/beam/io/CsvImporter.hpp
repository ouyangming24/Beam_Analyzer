#pragma once

#include "beam/core/BeamFrame.hpp"

#include <string>

namespace beam::io {

struct CsvImportOptions {
  double pixelSizeX_um = 5.0;
  double pixelSizeY_um = 5.0;
  char delimiter = ',';
  bool hasHeader = false;
};

struct CsvImportResult {
  beam::core::BeamFrame frame;
  bool ok = false;
  std::string message;
};

/// Load a 2D intensity frame from CSV (rows = Y, columns = X).
/// Optional first-line header "# pixel_um=5" or "# pixel_um_x=5,pixel_um_y=5"
/// overrides options when present.
[[nodiscard]] CsvImportResult loadFrameCsv(const std::string& path,
                                           const CsvImportOptions& options = {});

}  // namespace beam::io
