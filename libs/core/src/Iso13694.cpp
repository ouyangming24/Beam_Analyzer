#include "beam/core/Iso13694.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace beam::core {

Iso13694Metrics computeIso13694(const BeamFrame& frame, double eta) {
  Iso13694Metrics out;
  out.eta = std::clamp(eta, 1e-6, 1.0);

  if (frame.empty() || frame.intensity.size() != frame.width * frame.height) {
    return out;
  }

  double peak = 0.0;
  for (double v : frame.intensity) {
    if (std::isfinite(v)) {
      peak = std::max(peak, v);
    }
  }
  out.peak = peak;
  if (!(peak > 0.0)) {
    return out;
  }

  const double thresh = out.eta * peak;
  double sum = 0.0;
  double sum2 = 0.0;
  double minV = peak;
  std::size_t n = 0;

  for (double v : frame.intensity) {
    if (!std::isfinite(v) || v < thresh) {
      continue;
    }
    sum += v;
    sum2 += v * v;
    minV = std::min(minV, v);
    ++n;
  }

  if (n == 0) {
    return out;
  }

  const double mean = sum / static_cast<double>(n);
  const double var =
      std::max(0.0, sum2 / static_cast<double>(n) - mean * mean);
  const double sigma = std::sqrt(var);

  out.pixelsEffective = n;
  out.averageOnEffective = mean;
  out.minOnEffective = minV;
  out.rmsOnEffective = std::sqrt(sum2 / static_cast<double>(n));
  out.effectiveArea_um2 =
      static_cast<double>(n) * frame.pixelSizeX_um * frame.pixelSizeY_um;

  out.flatnessFactor = mean > 0.0 ? (peak / mean) : 0.0;
  out.flatnessNormalized = peak > 0.0 ? (mean / peak) : 0.0;
  out.uniformity = mean > 0.0 ? std::clamp(1.0 - sigma / mean, 0.0, 1.0) : 0.0;
  const double denom = peak + minV;
  out.uniformityMinMax =
      denom > 0.0 ? std::clamp(1.0 - (peak - minV) / denom, 0.0, 1.0) : 0.0;
  out.valid = true;
  return out;
}

}  // namespace beam::core
