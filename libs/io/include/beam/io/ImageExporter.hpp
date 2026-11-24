#pragma once

#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/Roi.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beam::io {

enum class ExportColorMap {
  Grayscale,
  FalseColor,  ///< Blue → cyan → green → yellow → red
};

struct AnnotatedExportOptions {
  ExportColorMap colorMap = ExportColorMap::FalseColor;
  bool drawCrosshair = true;
  bool drawRoi = true;
  bool drawCentroid = true;
  bool drawMetricsOverlay = true;
  int crossX = -1;
  int crossY = -1;
  beam::core::RectRoi roi{};
  bool hasRoi = false;
  /// Extra text lines drawn into a header strip (UTF-8 ASCII-safe preferred).
  std::vector<std::string> overlayLines;
};

struct RgbImage {
  std::size_t width = 0;
  std::size_t height = 0;
  std::vector<std::uint8_t> rgb;  ///< packed RGB24, size = width*height*3
};

/// Map a frame to RGB24 (no annotations).
[[nodiscard]] RgbImage renderFrameRgb(const beam::core::BeamFrame& frame,
                                      ExportColorMap map);

/// Render frame with optional annotations / metrics header strip.
[[nodiscard]] RgbImage renderAnnotatedRgb(const beam::core::BeamFrame& frame,
                                          const beam::core::Iso11146Metrics* metrics,
                                          const AnnotatedExportOptions& options);

/// Write a 24-bit Windows BMP. Returns false on I/O error.
[[nodiscard]] bool saveRgbBmp(const std::string& path, const RgbImage& image);

/// Convenience: false-color/annotated BMP export.
[[nodiscard]] bool exportAnnotatedBmp(const std::string& path,
                                      const beam::core::BeamFrame& frame,
                                      const beam::core::Iso11146Metrics* metrics,
                                      const AnnotatedExportOptions& options);

}  // namespace beam::io
