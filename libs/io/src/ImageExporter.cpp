#include "beam/io/ImageExporter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace beam::io {
namespace {

void mapFalseColor(double t, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
  t = std::clamp(t, 0.0, 1.0);
  if (t < 0.25) {
    const double u = t / 0.25;
    r = 0;
    g = static_cast<std::uint8_t>(255 * u);
    b = 255;
  } else if (t < 0.5) {
    const double u = (t - 0.25) / 0.25;
    r = 0;
    g = 255;
    b = static_cast<std::uint8_t>(255 * (1.0 - u));
  } else if (t < 0.75) {
    const double u = (t - 0.5) / 0.25;
    r = static_cast<std::uint8_t>(255 * u);
    g = 255;
    b = 0;
  } else {
    const double u = (t - 0.75) / 0.25;
    r = 255;
    g = static_cast<std::uint8_t>(255 * (1.0 - u));
    b = 0;
  }
}

void setPixel(RgbImage& img, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  if (x < 0 || y < 0 ||
      x >= static_cast<int>(img.width) || y >= static_cast<int>(img.height)) {
    return;
  }
  const std::size_t i = (static_cast<std::size_t>(y) * img.width + static_cast<std::size_t>(x)) * 3;
  img.rgb[i + 0] = r;
  img.rgb[i + 1] = g;
  img.rgb[i + 2] = b;
}

void drawHLine(RgbImage& img, int x0, int x1, int y, std::uint8_t r, std::uint8_t g,
               std::uint8_t b) {
  if (x0 > x1) {
    std::swap(x0, x1);
  }
  for (int x = x0; x <= x1; ++x) {
    setPixel(img, x, y, r, g, b);
  }
}

void drawVLine(RgbImage& img, int x, int y0, int y1, std::uint8_t r, std::uint8_t g,
               std::uint8_t b) {
  if (y0 > y1) {
    std::swap(y0, y1);
  }
  for (int y = y0; y <= y1; ++y) {
    setPixel(img, x, y, r, g, b);
  }
}

void drawRect(RgbImage& img, int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g,
              std::uint8_t b) {
  drawHLine(img, x0, x1, y0, r, g, b);
  drawHLine(img, x0, x1, y1, r, g, b);
  drawVLine(img, x0, y0, y1, r, g, b);
  drawVLine(img, x1, y0, y1, r, g, b);
}

void drawCross(RgbImage& img, int cx, int cy, int arm, std::uint8_t r, std::uint8_t g,
               std::uint8_t b) {
  drawHLine(img, cx - arm, cx + arm, cy, r, g, b);
  drawVLine(img, cx, cy - arm, cy + arm, r, g, b);
}

// Tiny 5x7 glyphs for ASCII digits / basic overlay (subset).
bool glyphBit(char ch, int gx, int gy) {
  // Very small font table for 0-9 . - space and A-Z subset used in overlays.
  static const char* digits[10] = {
      "01110"
      "10001"
      "10001"
      "10001"
      "10001"
      "10001"
      "01110",
      "00100"
      "01100"
      "00100"
      "00100"
      "00100"
      "00100"
      "01110",
      "01110"
      "10001"
      "00001"
      "00110"
      "01000"
      "10000"
      "11111",
      "01110"
      "10001"
      "00001"
      "00110"
      "00001"
      "10001"
      "01110",
      "00010"
      "00110"
      "01010"
      "10010"
      "11111"
      "00010"
      "00010",
      "11111"
      "10000"
      "11110"
      "00001"
      "00001"
      "10001"
      "01110",
      "01110"
      "10000"
      "11110"
      "10001"
      "10001"
      "10001"
      "01110",
      "11111"
      "00001"
      "00010"
      "00100"
      "01000"
      "01000"
      "01000",
      "01110"
      "10001"
      "10001"
      "01110"
      "10001"
      "10001"
      "01110",
      "01110"
      "10001"
      "10001"
      "01111"
      "00001"
      "00001"
      "01110",
  };
  if (ch >= '0' && ch <= '9') {
    return digits[ch - '0'][gy * 5 + gx] == '1';
  }
  if (ch == '.') {
    return gy == 6 && gx == 2;
  }
  if (ch == '-') {
    return gy == 3 && gx >= 1 && gx <= 3;
  }
  if (ch == ' ') {
    return false;
  }
  if (ch == ':') {
    return (gy == 2 || gy == 4) && gx == 2;
  }
  // Fallback block for letters — filled left column as marker
  return gx == 0 && gy >= 1 && gy <= 5;
}

void drawChar(RgbImage& img, int x, int y, char ch, std::uint8_t r, std::uint8_t g,
              std::uint8_t b) {
  for (int gy = 0; gy < 7; ++gy) {
    for (int gx = 0; gx < 5; ++gx) {
      if (glyphBit(ch, gx, gy)) {
        setPixel(img, x + gx, y + gy, r, g, b);
      }
    }
  }
}

void drawText(RgbImage& img, int x, int y, const std::string& text, std::uint8_t r,
              std::uint8_t g, std::uint8_t b) {
  int cx = x;
  for (char ch : text) {
    drawChar(img, cx, y, ch, r, g, b);
    cx += 6;
  }
}

#pragma pack(push, 1)
struct BmpFileHeader {
  std::uint16_t bfType = 0x4D42;
  std::uint32_t bfSize = 0;
  std::uint16_t bfReserved1 = 0;
  std::uint16_t bfReserved2 = 0;
  std::uint32_t bfOffBits = 54;
};

struct BmpInfoHeader {
  std::uint32_t biSize = 40;
  std::int32_t biWidth = 0;
  std::int32_t biHeight = 0;
  std::uint16_t biPlanes = 1;
  std::uint16_t biBitCount = 24;
  std::uint32_t biCompression = 0;
  std::uint32_t biSizeImage = 0;
  std::int32_t biXPelsPerMeter = 0;
  std::int32_t biYPelsPerMeter = 0;
  std::uint32_t biClrUsed = 0;
  std::uint32_t biClrImportant = 0;
};
#pragma pack(pop)

}  // namespace

RgbImage renderFrameRgb(const beam::core::BeamFrame& frame, ExportColorMap map) {
  RgbImage img;
  if (frame.empty()) {
    return img;
  }
  img.width = frame.width;
  img.height = frame.height;
  img.rgb.assign(frame.width * frame.height * 3, 0);

  double mn = frame.intensity.front();
  double mx = frame.intensity.front();
  for (double v : frame.intensity) {
    mn = std::min(mn, v);
    mx = std::max(mx, v);
  }
  const double span = (mx > mn) ? (mx - mn) : 1.0;

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double t = (frame.intensity[y * frame.width + x] - mn) / span;
      std::uint8_t r = 0;
      std::uint8_t g = 0;
      std::uint8_t b = 0;
      if (map == ExportColorMap::Grayscale) {
        const auto v = static_cast<std::uint8_t>(
            std::lround(255.0 * std::clamp(t, 0.0, 1.0)));
        r = g = b = v;
      } else {
        mapFalseColor(t, r, g, b);
      }
      const std::size_t i = (y * frame.width + x) * 3;
      img.rgb[i + 0] = r;
      img.rgb[i + 1] = g;
      img.rgb[i + 2] = b;
    }
  }
  return img;
}

RgbImage renderAnnotatedRgb(const beam::core::BeamFrame& frame,
                            const beam::core::Iso11146Metrics* metrics,
                            const AnnotatedExportOptions& options) {
  RgbImage body = renderFrameRgb(frame, options.colorMap);
  if (body.rgb.empty()) {
    return body;
  }

  const int headerLines =
      options.drawMetricsOverlay
          ? static_cast<int>(std::max<std::size_t>(1, options.overlayLines.size()))
          : 0;
  const int headerH = headerLines > 0 ? (headerLines * 10 + 8) : 0;

  RgbImage out;
  out.width = body.width;
  out.height = body.height + static_cast<std::size_t>(headerH);
  out.rgb.assign(out.width * out.height * 3, 20);

  // Copy body below header
  for (std::size_t y = 0; y < body.height; ++y) {
    for (std::size_t x = 0; x < body.width; ++x) {
      const std::size_t si = (y * body.width + x) * 3;
      const std::size_t di =
          ((y + static_cast<std::size_t>(headerH)) * out.width + x) * 3;
      out.rgb[di + 0] = body.rgb[si + 0];
      out.rgb[di + 1] = body.rgb[si + 1];
      out.rgb[di + 2] = body.rgb[si + 2];
    }
  }

  auto mapY = [headerH](int y) { return y + headerH; };

  if (options.drawCrosshair && options.crossX >= 0 && options.crossY >= 0) {
    drawCross(out, options.crossX, mapY(options.crossY), 8, 255, 255, 255);
  }
  if (options.drawRoi && options.hasRoi && !options.roi.empty()) {
    drawRect(out, options.roi.x0, mapY(options.roi.y0), options.roi.x1, mapY(options.roi.y1),
             0, 255, 180);
  }
  if (options.drawCentroid && metrics && metrics->valid && frame.pixelSizeX_um > 0 &&
      frame.pixelSizeY_um > 0) {
    const int cx = static_cast<int>(std::lround(metrics->centroidX_um / frame.pixelSizeX_um));
    const int cy = static_cast<int>(std::lround(metrics->centroidY_um / frame.pixelSizeY_um));
    drawCross(out, cx, mapY(cy), 6, 255, 64, 64);
  }

  if (headerH > 0) {
    int ty = 4;
    auto lines = options.overlayLines;
    if (lines.empty() && metrics && metrics->valid) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "Cx:%.2f um  Cy:%.2f um", metrics->centroidX_um,
                    metrics->centroidY_um);
      lines.emplace_back(buf);
      std::snprintf(buf, sizeof(buf), "Dx:%.2f um  Dy:%.2f um", metrics->diameterX_um,
                    metrics->diameterY_um);
      lines.emplace_back(buf);
    }
    for (const auto& line : lines) {
      drawText(out, 4, ty, line, 230, 230, 230);
      ty += 10;
    }
  }
  return out;
}

bool saveRgbBmp(const std::string& path, const RgbImage& image) {
  if (image.width == 0 || image.height == 0 ||
      image.rgb.size() < image.width * image.height * 3) {
    return false;
  }

  const int width = static_cast<int>(image.width);
  const int height = static_cast<int>(image.height);
  const int rowBytes = ((width * 24 + 31) / 32) * 4;
  const std::uint32_t imageSize = static_cast<std::uint32_t>(rowBytes * height);

  BmpFileHeader fh;
  fh.bfSize = 54 + imageSize;
  fh.bfOffBits = 54;

  BmpInfoHeader ih;
  ih.biWidth = width;
  ih.biHeight = height;  // bottom-up
  ih.biSizeImage = imageSize;

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
  out.write(reinterpret_cast<const char*>(&ih), sizeof(ih));

  std::vector<std::uint8_t> row(static_cast<std::size_t>(rowBytes), 0);
  for (int y = height - 1; y >= 0; --y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t i = (static_cast<std::size_t>(y) * image.width +
                             static_cast<std::size_t>(x)) *
                            3;
      row[static_cast<std::size_t>(x * 3 + 0)] = image.rgb[i + 2];  // B
      row[static_cast<std::size_t>(x * 3 + 1)] = image.rgb[i + 1];  // G
      row[static_cast<std::size_t>(x * 3 + 2)] = image.rgb[i + 0];  // R
    }
    out.write(reinterpret_cast<const char*>(row.data()), rowBytes);
  }
  return static_cast<bool>(out);
}

bool exportAnnotatedBmp(const std::string& path, const beam::core::BeamFrame& frame,
                        const beam::core::Iso11146Metrics* metrics,
                        const AnnotatedExportOptions& options) {
  const RgbImage img = renderAnnotatedRgb(frame, metrics, options);
  return saveRgbBmp(path, img);
}

}  // namespace beam::io
