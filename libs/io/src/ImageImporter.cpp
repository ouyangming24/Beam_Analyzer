#include "beam/io/ImageImporter.hpp"

#include "beam/io/CsvImporter.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>

namespace beam::io {
namespace {

std::string lowerExt(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  const auto base = (slash == std::string::npos) ? path : path.substr(slash + 1);
  const auto dot = base.find_last_of('.');
  if (dot == std::string::npos) {
    return {};
  }
  std::string ext = base.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

#pragma pack(push, 1)
struct BmpFileHeader {
  std::uint16_t bfType = 0;
  std::uint32_t bfSize = 0;
  std::uint16_t bfReserved1 = 0;
  std::uint16_t bfReserved2 = 0;
  std::uint32_t bfOffBits = 0;
};

struct BmpInfoHeader {
  std::uint32_t biSize = 0;
  std::int32_t biWidth = 0;
  std::int32_t biHeight = 0;
  std::uint16_t biPlanes = 0;
  std::uint16_t biBitCount = 0;
  std::uint32_t biCompression = 0;
  std::uint32_t biSizeImage = 0;
  std::int32_t biXPelsPerMeter = 0;
  std::int32_t biYPelsPerMeter = 0;
  std::uint32_t biClrUsed = 0;
  std::uint32_t biClrImportant = 0;
};
#pragma pack(pop)

}  // namespace

ImageImportResult frameFromGray8(std::size_t width, std::size_t height,
                                 const std::uint8_t* data,
                                 const ImageImportOptions& options) {
  ImageImportResult result;
  if (width == 0 || height == 0 || data == nullptr) {
    result.message = "Invalid gray8 raster";
    return result;
  }
  result.frame.resize(width, height);
  result.frame.pixelSizeX_um = options.pixelSizeX_um;
  result.frame.pixelSizeY_um = options.pixelSizeY_um;
  for (std::size_t i = 0; i < width * height; ++i) {
    result.frame.intensity[i] = static_cast<double>(data[i]);
  }
  result.ok = true;
  result.format = "gray8";
  result.message = "ok";
  return result;
}

ImageImportResult frameFromGray16(std::size_t width, std::size_t height,
                                  const std::uint16_t* data,
                                  const ImageImportOptions& options) {
  ImageImportResult result;
  if (width == 0 || height == 0 || data == nullptr) {
    result.message = "Invalid gray16 raster";
    return result;
  }
  result.frame.resize(width, height);
  result.frame.pixelSizeX_um = options.pixelSizeX_um;
  result.frame.pixelSizeY_um = options.pixelSizeY_um;
  for (std::size_t i = 0; i < width * height; ++i) {
    result.frame.intensity[i] = static_cast<double>(data[i]);
  }
  result.ok = true;
  result.format = "gray16";
  result.message = "ok";
  return result;
}

ImageImportResult frameFromRgb24(std::size_t width, std::size_t height,
                                 const std::uint8_t* rgb,
                                 const ImageImportOptions& options) {
  ImageImportResult result;
  if (width == 0 || height == 0 || rgb == nullptr) {
    result.message = "Invalid rgb24 raster";
    return result;
  }
  result.frame.resize(width, height);
  result.frame.pixelSizeX_um = options.pixelSizeX_um;
  result.frame.pixelSizeY_um = options.pixelSizeY_um;
  for (std::size_t i = 0; i < width * height; ++i) {
    const std::uint8_t r = rgb[i * 3 + 0];
    const std::uint8_t g = rgb[i * 3 + 1];
    const std::uint8_t b = rgb[i * 3 + 2];
    if (options.rgbToLuminance) {
      result.frame.intensity[i] = 0.299 * r + 0.587 * g + 0.114 * b;
    } else {
      result.frame.intensity[i] = static_cast<double>((static_cast<int>(r) + g + b) / 3);
    }
  }
  result.ok = true;
  result.format = "rgb24";
  result.message = "ok";
  return result;
}

ImageImportResult loadFrameBmp(const std::string& path, const ImageImportOptions& options) {
  ImageImportResult result;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    result.message = "Cannot open BMP: " + path;
    return result;
  }

  BmpFileHeader fh{};
  BmpInfoHeader ih{};
  in.read(reinterpret_cast<char*>(&fh), sizeof(fh));
  in.read(reinterpret_cast<char*>(&ih), sizeof(ih));
  if (!in || fh.bfType != 0x4D42) {
    result.message = "Not a BMP file: " + path;
    return result;
  }
  if (ih.biCompression != 0) {
    result.message = "Compressed BMP not supported";
    return result;
  }
  if (ih.biBitCount != 8 && ih.biBitCount != 24) {
    result.message = "Only 8-bit and 24-bit BMP supported";
    return result;
  }

  const int width = ih.biWidth;
  const int absHeight = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
  const bool bottomUp = ih.biHeight > 0;
  if (width <= 0 || absHeight <= 0) {
    result.message = "Invalid BMP dimensions";
    return result;
  }

  in.seekg(fh.bfOffBits, std::ios::beg);
  if (!in) {
    result.message = "BMP pixel offset seek failed";
    return result;
  }

  const int rowBytes =
      ((width * static_cast<int>(ih.biBitCount) + 31) / 32) * 4;
  std::vector<std::uint8_t> row(static_cast<std::size_t>(rowBytes));

  if (ih.biBitCount == 8) {
    // Skip palette — already at bfOffBits
    std::vector<std::uint8_t> gray(static_cast<std::size_t>(width * absHeight));
    for (int y = 0; y < absHeight; ++y) {
      in.read(reinterpret_cast<char*>(row.data()), rowBytes);
      if (!in) {
        result.message = "BMP truncated (8-bit)";
        return result;
      }
      const int dstY = bottomUp ? (absHeight - 1 - y) : y;
      std::memcpy(gray.data() + static_cast<std::size_t>(dstY * width), row.data(),
                  static_cast<std::size_t>(width));
    }
    result = frameFromGray8(static_cast<std::size_t>(width),
                            static_cast<std::size_t>(absHeight), gray.data(), options);
    result.format = "bmp";
    return result;
  }

  // 24-bit BGR
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width * absHeight * 3));
  for (int y = 0; y < absHeight; ++y) {
    in.read(reinterpret_cast<char*>(row.data()), rowBytes);
    if (!in) {
      result.message = "BMP truncated (24-bit)";
      return result;
    }
    const int dstY = bottomUp ? (absHeight - 1 - y) : y;
    for (int x = 0; x < width; ++x) {
      const std::uint8_t b = row[static_cast<std::size_t>(x * 3 + 0)];
      const std::uint8_t g = row[static_cast<std::size_t>(x * 3 + 1)];
      const std::uint8_t r = row[static_cast<std::size_t>(x * 3 + 2)];
      const std::size_t i = static_cast<std::size_t>(dstY * width + x) * 3;
      rgb[i + 0] = r;
      rgb[i + 1] = g;
      rgb[i + 2] = b;
    }
  }
  result = frameFromRgb24(static_cast<std::size_t>(width),
                          static_cast<std::size_t>(absHeight), rgb.data(), options);
  result.format = "bmp";
  return result;
}

ImageImportResult loadFrameByExtension(const std::string& path,
                                       const ImageImportOptions& options) {
  const std::string ext = lowerExt(path);
  if (ext == "bmp") {
    return loadFrameBmp(path, options);
  }
  if (ext == "csv") {
    CsvImportOptions csvOpt;
    csvOpt.pixelSizeX_um = options.pixelSizeX_um;
    csvOpt.pixelSizeY_um = options.pixelSizeY_um;
    const auto csv = loadFrameCsv(path, csvOpt);
    ImageImportResult result;
    result.ok = csv.ok;
    result.message = csv.message;
    result.frame = csv.frame;
    result.format = "csv";
    return result;
  }
  ImageImportResult result;
  result.message =
      "Extension not handled in beam::io (use Qt UI for PNG/JPEG/TIFF): " + ext;
  return result;
}

}  // namespace beam::io
