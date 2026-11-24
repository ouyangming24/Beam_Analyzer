#include "beam/core/Iso11146.hpp"
#include "beam/io/ImageExporter.hpp"
#include "beam/io/ImageImporter.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #cond "\n"; \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

int main() {
  const auto tmpDir = fs::temp_directory_path() / "beam_analyzer_image_io";
  fs::create_directories(tmpDir);
  const std::string bmpPath = (tmpDir / "gaussian.bmp").string();
  const std::string annotatedPath = (tmpDir / "annotated.bmp").string();
  const std::string grayBmpPath = (tmpDir / "gray8.bmp").string();

  auto frame = beam::core::makeGaussianFrame(64, 48, 32.0, 24.0, 6.0, 4.0, 200.0, 5.0, 5.0);
  CHECK(!frame.empty());

  // Gray8 → BMP → load roundtrip
  std::vector<std::uint8_t> gray(frame.width * frame.height);
  double mx = 0;
  for (double v : frame.intensity) {
    mx = std::max(mx, v);
  }
  for (std::size_t i = 0; i < frame.intensity.size(); ++i) {
    gray[i] = static_cast<std::uint8_t>(
        std::lround(255.0 * frame.intensity[i] / (mx > 0 ? mx : 1.0)));
  }
  auto built = beam::io::frameFromGray8(frame.width, frame.height, gray.data());
  CHECK(built.ok);
  CHECK(built.frame.width == 64);
  CHECK(built.frame.height == 48);

  beam::io::AnnotatedExportOptions opt;
  opt.colorMap = beam::io::ExportColorMap::FalseColor;
  opt.drawCrosshair = true;
  opt.crossX = 32;
  opt.crossY = 24;
  opt.drawMetricsOverlay = true;
  opt.overlayLines = {"Cx test", "Dx Dy"};
  auto metrics = beam::core::computeIso11146(frame);
  CHECK(metrics.valid);

  CHECK(beam::io::exportAnnotatedBmp(annotatedPath, frame, &metrics, opt));
  CHECK(fs::exists(annotatedPath));
  CHECK(fs::file_size(annotatedPath) > 54);

  // False-color RGB BMP without annotations header still loads as luminance frame
  auto rgb = beam::io::renderFrameRgb(frame, beam::io::ExportColorMap::Grayscale);
  CHECK(rgb.width == 64);
  CHECK(rgb.height == 48);
  CHECK(beam::io::saveRgbBmp(bmpPath, rgb));

  beam::io::ImageImportOptions iopt;
  iopt.pixelSizeX_um = 5.0;
  iopt.pixelSizeY_um = 5.0;
  auto loaded = beam::io::loadFrameBmp(bmpPath, iopt);
  CHECK(loaded.ok);
  CHECK(loaded.format == "bmp");
  CHECK(loaded.frame.width == 64);
  CHECK(loaded.frame.height == 48);
  CHECK(std::abs(loaded.frame.pixelSizeX_um - 5.0) < 1e-9);

  auto reloadedMetrics = beam::core::computeIso11146(loaded.frame);
  CHECK(reloadedMetrics.valid);
  // Centroid should stay near center after grayscale roundtrip
  CHECK(std::abs(reloadedMetrics.centroidX_um / 5.0 - 32.0) < 3.0);
  CHECK(std::abs(reloadedMetrics.centroidY_um / 5.0 - 24.0) < 3.0);

  // Extension dispatcher
  auto byExt = beam::io::loadFrameByExtension(bmpPath, iopt);
  CHECK(byExt.ok);

  // frameFromRgb24 smoke
  std::vector<std::uint8_t> rgb24(4 * 4 * 3, 0);
  for (int i = 0; i < 16; ++i) {
    rgb24[static_cast<std::size_t>(i * 3 + 0)] = 10;
    rgb24[static_cast<std::size_t>(i * 3 + 1)] = 20;
    rgb24[static_cast<std::size_t>(i * 3 + 2)] = 30;
  }
  auto fromRgb = beam::io::frameFromRgb24(4, 4, rgb24.data());
  CHECK(fromRgb.ok);
  CHECK(fromRgb.frame.intensity[0] > 0.0);

  // Save 8-bit style via exporter then ensure annotated path works for empty metrics
  CHECK(beam::io::exportAnnotatedBmp(grayBmpPath, frame, nullptr, opt));
  CHECK(fs::exists(grayBmpPath));

  std::cout << (failures == 0 ? "image_io OK\n" : "image_io FAILED\n");
  return failures == 0 ? 0 : 1;
}
