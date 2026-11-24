#pragma once

#include "beam/core/BeamFrame.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace beam::io {

struct ImageImportOptions {
  double pixelSizeX_um = 5.0;
  double pixelSizeY_um = 5.0;
  /// When true, RGB images use luminance 0.299R+0.587G+0.114B.
  bool rgbToLuminance = true;
};

struct ImageImportResult {
  beam::core::BeamFrame frame;
  bool ok = false;
  std::string message;
  std::string format;  ///< e.g. "bmp", "png", "raster"
};

/// Build a frame from row-major 8-bit grayscale samples.
[[nodiscard]] ImageImportResult frameFromGray8(std::size_t width,
                                               std::size_t height,
                                               const std::uint8_t* data,
                                               const ImageImportOptions& options = {});

/// Build a frame from row-major 16-bit grayscale samples (host endian).
[[nodiscard]] ImageImportResult frameFromGray16(std::size_t width,
                                                std::size_t height,
                                                const std::uint16_t* data,
                                                const ImageImportOptions& options = {});

/// Build a frame from packed 24-bit RGB (R,G,B per pixel, row-major).
[[nodiscard]] ImageImportResult frameFromRgb24(std::size_t width,
                                               std::size_t height,
                                               const std::uint8_t* rgb,
                                               const ImageImportOptions& options = {});

/// Load a Windows BMP (8-bit grayscale or 24-bit RGB). Portable, no Qt.
[[nodiscard]] ImageImportResult loadFrameBmp(const std::string& path,
                                             const ImageImportOptions& options = {});

/// Dispatch by file extension: .bmp → BMP loader; .csv → CSV importer.
/// Other raster formats (PNG/JPEG/TIFF) are opened in the Qt UI layer.
[[nodiscard]] ImageImportResult loadFrameByExtension(const std::string& path,
                                                     const ImageImportOptions& options = {});

}  // namespace beam::io
