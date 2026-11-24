#include "FalseColor.hpp"

#include <algorithm>
#include <cmath>

namespace beam::ui {
namespace {

QRgb mapFalseColor(double t) {
  t = std::clamp(t, 0.0, 1.0);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t < 0.25) {
    const double u = t / 0.25;
    r = 0;
    g = static_cast<int>(255 * u);
    b = 255;
  } else if (t < 0.5) {
    const double u = (t - 0.25) / 0.25;
    r = 0;
    g = 255;
    b = static_cast<int>(255 * (1.0 - u));
  } else if (t < 0.75) {
    const double u = (t - 0.5) / 0.25;
    r = static_cast<int>(255 * u);
    g = 255;
    b = 0;
  } else {
    const double u = (t - 0.75) / 0.25;
    r = 255;
    g = static_cast<int>(255 * (1.0 - u));
    b = 0;
  }
  return qRgb(r, g, b);
}

double normalizeValue(double v, double mn, double mx, IntensityScale scale) {
  if (!(mx > mn)) {
    return 0.0;
  }
  if (scale == IntensityScale::Linear) {
    return (v - mn) / (mx - mn);
  }
  // Log display: shift so positives map; clip non-positive to floor.
  const double floor = std::max(1e-12, mx * 1e-6);
  const double vv = std::max(v, floor);
  const double lo = std::max(mn, floor);
  const double hi = std::max(mx, lo * 1.000001);
  const double t = (std::log10(vv) - std::log10(lo)) / (std::log10(hi) - std::log10(lo));
  return std::clamp(t, 0.0, 1.0);
}

}  // namespace

QImage frameToImage(const beam::core::BeamFrame& frame,
                    ColorMap map,
                    double* outMin,
                    double* outMax,
                    IntensityScale scale) {
  if (frame.empty()) {
    return {};
  }

  double mn = frame.intensity.front();
  double mx = frame.intensity.front();
  for (double v : frame.intensity) {
    mn = std::min(mn, v);
    mx = std::max(mx, v);
  }
  if (outMin) {
    *outMin = mn;
  }
  if (outMax) {
    *outMax = mx;
  }

  QImage img(static_cast<int>(frame.width), static_cast<int>(frame.height),
             QImage::Format_RGB32);

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double t =
          normalizeValue(frame.intensity[y * frame.width + x], mn, mx, scale);
      if (map == ColorMap::Grayscale) {
        const int g = static_cast<int>(std::lround(255.0 * std::clamp(t, 0.0, 1.0)));
        img.setPixel(static_cast<int>(x), static_cast<int>(y), qRgb(g, g, g));
      } else {
        img.setPixel(static_cast<int>(x), static_cast<int>(y), mapFalseColor(t));
      }
    }
  }
  return img;
}

}  // namespace beam::ui
