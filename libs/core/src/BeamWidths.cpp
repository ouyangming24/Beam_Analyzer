#include "beam/core/BeamWidths.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace beam::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

void fillCentroidPx(const BeamFrame& frame,
                    const Iso11146Metrics* hint,
                    double& cxPx,
                    double& cyPx) {
  if (hint && hint->valid && frame.pixelSizeX_um > 0.0 && frame.pixelSizeY_um > 0.0) {
    cxPx = hint->centroidX_um / frame.pixelSizeX_um;
    cyPx = hint->centroidY_um / frame.pixelSizeY_um;
    return;
  }
  const auto m = computeIso11146(frame);
  if (m.valid && frame.pixelSizeX_um > 0.0 && frame.pixelSizeY_um > 0.0) {
    cxPx = m.centroidX_um / frame.pixelSizeX_um;
    cyPx = m.centroidY_um / frame.pixelSizeY_um;
  } else {
    cxPx = 0.5 * static_cast<double>(frame.width);
    cyPx = 0.5 * static_cast<double>(frame.height);
  }
}

double interpolateCrossing(const std::vector<double>& cum,
                           double target) {
  if (cum.empty()) {
    return 0.0;
  }
  if (target <= cum.front()) {
    return 0.0;
  }
  if (target >= cum.back()) {
    return static_cast<double>(cum.size() - 1);
  }
  for (std::size_t i = 1; i < cum.size(); ++i) {
    if (cum[i] >= target) {
      const double y0 = cum[i - 1];
      const double y1 = cum[i];
      const double t = (y1 > y0) ? (target - y0) / (y1 - y0) : 0.0;
      return static_cast<double>(i - 1) + t;
    }
  }
  return static_cast<double>(cum.size() - 1);
}

AxisWidthResult knifeFromProjection(const std::vector<double>& proj,
                                    double pixel_um,
                                    double clipLow,
                                    double clipHigh,
                                    double multiplier) {
  AxisWidthResult out;
  if (proj.empty() || pixel_um <= 0.0) {
    return out;
  }
  double total = 0.0;
  for (double v : proj) {
    total += std::max(0.0, v);
  }
  if (!(total > 0.0) || !std::isfinite(total)) {
    return out;
  }
  std::vector<double> cum(proj.size(), 0.0);
  double run = 0.0;
  for (std::size_t i = 0; i < proj.size(); ++i) {
    run += std::max(0.0, proj[i]);
    cum[i] = run;
  }
  const double xLo = interpolateCrossing(cum, clipLow * total);
  const double xHi = interpolateCrossing(cum, clipHigh * total);
  const double span = std::max(0.0, xHi - xLo);
  out.widthX_um = multiplier * span * pixel_um;
  out.valid = std::isfinite(out.widthX_um) && out.widthX_um > 0.0;
  return out;
}

int spanAboveThreshold(const BeamFrame& frame, int row, int col, bool horizontal, double thr) {
  if (frame.empty()) {
    return 0;
  }
  int first = -1;
  int last = -1;
  if (horizontal) {
    if (row < 0 || static_cast<std::size_t>(row) >= frame.height) {
      return 0;
    }
    for (std::size_t x = 0; x < frame.width; ++x) {
      if (frame.intensity[static_cast<std::size_t>(row) * frame.width + x] >= thr) {
        if (first < 0) {
          first = static_cast<int>(x);
        }
        last = static_cast<int>(x);
      }
    }
  } else {
    if (col < 0 || static_cast<std::size_t>(col) >= frame.width) {
      return 0;
    }
    for (std::size_t y = 0; y < frame.height; ++y) {
      if (frame.intensity[y * frame.width + static_cast<std::size_t>(col)] >= thr) {
        if (first < 0) {
          first = static_cast<int>(y);
        }
        last = static_cast<int>(y);
      }
    }
  }
  if (first < 0) {
    return 0;
  }
  return last - first + 1;
}

double totalPower(const BeamFrame& frame) {
  double s = 0.0;
  for (double v : frame.intensity) {
    if (v > 0.0) {
      s += v;
    }
  }
  return s;
}

double powerInRadius(const BeamFrame& frame, double cxPx, double cyPx, double rPx) {
  const double r2 = rPx * rPx;
  double s = 0.0;
  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double dx = (static_cast<double>(x) + 0.5) - cxPx;
      const double dy = (static_cast<double>(y) + 0.5) - cyPx;
      if (dx * dx + dy * dy <= r2) {
        const double v = frame.intensity[y * frame.width + x];
        if (v > 0.0) {
          s += v;
        }
      }
    }
  }
  return s;
}

}  // namespace

KnifeEdgeOptions resolveKnifeEdge(const KnifeEdgeOptions& in) {
  KnifeEdgeOptions o = in;
  switch (in.preset) {
    case KnifeEdgePreset::Clip1090:
      o.clipLow = 0.10;
      o.clipHigh = 0.90;
      o.multiplier = 1.561;
      break;
    case KnifeEdgePreset::Clip1684:
      o.clipLow = 0.16;
      o.clipHigh = 0.84;
      o.multiplier = 2.0;
      break;
    case KnifeEdgePreset::Programmable:
      break;
  }
  return o;
}

AxisWidthResult computeKnifeEdgeWidths(const BeamFrame& frame, const KnifeEdgeOptions& optIn) {
  AxisWidthResult out;
  if (frame.empty()) {
    return out;
  }
  const auto opt = resolveKnifeEdge(optIn);
  std::vector<double> projX(frame.width, 0.0);
  std::vector<double> projY(frame.height, 0.0);
  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double v = std::max(0.0, frame.intensity[y * frame.width + x]);
      projX[x] += v;
      projY[y] += v;
    }
  }
  const auto wx = knifeFromProjection(projX, frame.pixelSizeX_um, opt.clipLow, opt.clipHigh,
                                      opt.multiplier);
  const auto wy = knifeFromProjection(projY, frame.pixelSizeY_um, opt.clipLow, opt.clipHigh,
                                      opt.multiplier);
  out.widthX_um = wx.widthX_um;
  out.widthY_um = wy.widthX_um;
  out.diameter_um = std::sqrt(out.widthX_um * out.widthY_um);
  out.valid = wx.valid && wy.valid;
  return out;
}

AxisWidthResult computePercentPeakWidths(const BeamFrame& frame,
                                         double clipOfPeak,
                                         const Iso11146Metrics* centroidHint) {
  AxisWidthResult out;
  if (frame.empty() || !(clipOfPeak > 0.0) || clipOfPeak > 1.0) {
    return out;
  }
  double peak = 0.0;
  for (double v : frame.intensity) {
    peak = std::max(peak, v);
  }
  if (!(peak > 0.0)) {
    return out;
  }
  double cx = 0.0;
  double cy = 0.0;
  fillCentroidPx(frame, centroidHint, cx, cy);
  const int col = static_cast<int>(std::lround(cx - 0.5));
  const int row = static_cast<int>(std::lround(cy - 0.5));
  const double thr = clipOfPeak * peak;
  const int nx = spanAboveThreshold(frame, row, col, true, thr);
  const int ny = spanAboveThreshold(frame, row, col, false, thr);
  out.widthX_um = static_cast<double>(nx) * frame.pixelSizeX_um;
  out.widthY_um = static_cast<double>(ny) * frame.pixelSizeY_um;
  out.diameter_um = std::sqrt(out.widthX_um * out.widthY_um);
  out.valid = nx > 0 && ny > 0;
  return out;
}

AxisWidthResult computePercentEnergyWidths(const BeamFrame& frame,
                                           double enclosedFraction,
                                           const Iso11146Metrics* centroidHint) {
  AxisWidthResult out;
  if (frame.empty() || !(enclosedFraction > 0.0) || enclosedFraction > 1.0) {
    return out;
  }
  const double total = totalPower(frame);
  if (!(total > 0.0)) {
    return out;
  }
  std::vector<double> vals = frame.intensity;
  std::sort(vals.begin(), vals.end(), std::greater<double>());
  double run = 0.0;
  double clipLevel = 0.0;
  for (double v : vals) {
    if (v <= 0.0) {
      break;
    }
    run += v;
    clipLevel = v;
    if (run >= enclosedFraction * total) {
      break;
    }
  }
  double cx = 0.0;
  double cy = 0.0;
  fillCentroidPx(frame, centroidHint, cx, cy);
  const int col = static_cast<int>(std::lround(cx - 0.5));
  const int row = static_cast<int>(std::lround(cy - 0.5));
  const int nx = spanAboveThreshold(frame, row, col, true, clipLevel);
  const int ny = spanAboveThreshold(frame, row, col, false, clipLevel);
  out.widthX_um = static_cast<double>(nx) * frame.pixelSizeX_um;
  out.widthY_um = static_cast<double>(ny) * frame.pixelSizeY_um;
  // Equivalent circular diameter from area of pixels above clip.
  std::size_t nAbove = 0;
  for (double v : frame.intensity) {
    if (v >= clipLevel) {
      ++nAbove;
    }
  }
  const double area_um2 =
      static_cast<double>(nAbove) * frame.pixelSizeX_um * frame.pixelSizeY_um;
  out.diameter_um = 2.0 * std::sqrt(area_um2 / kPi);
  out.valid = nx > 0 && ny > 0;
  return out;
}

AxisWidthResult computeEncircledPowerDiameter(const BeamFrame& frame,
                                             double enclosedFraction,
                                             const Iso11146Metrics* centroidHint) {
  AxisWidthResult out;
  if (frame.empty() || !(enclosedFraction > 0.0) || enclosedFraction > 1.0) {
    return out;
  }
  const double total = totalPower(frame);
  if (!(total > 0.0)) {
    return out;
  }
  double cx = 0.0;
  double cy = 0.0;
  fillCentroidPx(frame, centroidHint, cx, cy);
  const double target = enclosedFraction * total;
  const double maxR =
      std::hypot(static_cast<double>(frame.width), static_cast<double>(frame.height));
  double lo = 0.0;
  double hi = maxR;
  for (int iter = 0; iter < 48; ++iter) {
    const double mid = 0.5 * (lo + hi);
    if (powerInRadius(frame, cx, cy, mid) >= target) {
      hi = mid;
    } else {
      lo = mid;
    }
  }
  const double rPx = hi;
  // Convert radius in pixel units to µm using geometric mean pitch.
  const double pitch = std::sqrt(frame.pixelSizeX_um * frame.pixelSizeY_um);
  out.diameter_um = 2.0 * rPx * pitch;
  out.widthX_um = 2.0 * rPx * frame.pixelSizeX_um;
  out.widthY_um = 2.0 * rPx * frame.pixelSizeY_um;
  out.valid = std::isfinite(out.diameter_um) && out.diameter_um > 0.0;
  return out;
}

BeamWidthSuite computeBeamWidthSuite(const BeamFrame& frame, const Iso11146Metrics* iso) {
  BeamWidthSuite s;
  if (frame.empty()) {
    return s;
  }
  Iso11146Metrics local;
  const Iso11146Metrics* hint = iso;
  if (!hint || !hint->valid) {
    local = computeIso11146(frame);
    hint = &local;
  }
  if (hint->valid) {
    s.d4sigma.widthX_um = hint->diameterX_um;
    s.d4sigma.widthY_um = hint->diameterY_um;
    s.d4sigma.diameter_um = std::sqrt(hint->diameterX_um * hint->diameterY_um);
    s.d4sigma.valid = true;
  }
  KnifeEdgeOptions k10;
  k10.preset = KnifeEdgePreset::Clip1090;
  KnifeEdgeOptions k16;
  k16.preset = KnifeEdgePreset::Clip1684;
  s.knife1090 = computeKnifeEdgeWidths(frame, k10);
  s.knife1684 = computeKnifeEdgeWidths(frame, k16);
  s.percentPeak = computePercentPeakWidths(frame, 0.135, hint);
  s.percentEnergy = computePercentEnergyWidths(frame, 0.865, hint);
  s.encircled = computeEncircledPowerDiameter(frame, 0.865, hint);
  s.valid = s.d4sigma.valid || s.knife1090.valid || s.percentPeak.valid;
  return s;
}

}  // namespace beam::core
