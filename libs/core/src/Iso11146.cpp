#include "beam/core/Iso11146.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace beam::core {
namespace {

double estimateBorderNoiseSigma(const BeamFrame& frame, int borderPx = 2) {
  if (frame.empty()) {
    return 0.0;
  }
  const int bw = std::max(1, borderPx);
  const int w = static_cast<int>(frame.width);
  const int h = static_cast<int>(frame.height);
  double sum = 0.0;
  double sum2 = 0.0;
  std::size_t n = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const bool onBorder = (x < bw) || (x >= w - bw) || (y < bw) || (y >= h - bw);
      if (!onBorder) {
        continue;
      }
      const double v = frame.intensity[static_cast<std::size_t>(y) * frame.width +
                                       static_cast<std::size_t>(x)];
      sum += v;
      sum2 += v * v;
      ++n;
    }
  }
  if (n < 2) {
    return 0.0;
  }
  const double mean = sum / static_cast<double>(n);
  const double var = std::max(0.0, sum2 / static_cast<double>(n) - mean * mean);
  return std::sqrt(var);
}

double peakIntensity(const BeamFrame& frame) {
  double peak = 0.0;
  for (double v : frame.intensity) {
    peak = std::max(peak, v);
  }
  return peak;
}

Iso11146Metrics momentsFromPrepared(const BeamFrame& frame,
                                    bool clipNegative,
                                    bool refineCentroid,
                                    double noiseFloor,
                                    double noiseSigma,
                                    double backgroundLevel) {
  Iso11146Metrics out;
  out.appliedNoiseFloor = noiseFloor;
  out.estimatedNoiseSigma = noiseSigma;
  out.backgroundLevel = backgroundLevel;

  if (frame.empty() || frame.intensity.size() != frame.width * frame.height) {
    return out;
  }

  const double dx = frame.pixelSizeX_um;
  const double dy = frame.pixelSizeY_um;

  auto intensityAt = [&](std::size_t x, std::size_t y) {
    double I = frame.intensity[y * frame.width + x];
    if (clipNegative) {
      I = std::max(0.0, I);
    }
    if (I < noiseFloor) {
      I = 0.0;
    }
    return I;
  };

  double sum = 0.0;
  double sumX = 0.0;
  double sumY = 0.0;
  std::size_t used = 0;

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double I = intensityAt(x, y);
      if (I == 0.0) {
        continue;
      }
      const double X = (static_cast<double>(x) + 0.5) * dx;
      const double Y = (static_cast<double>(y) + 0.5) * dy;
      sum += I;
      sumX += I * X;
      sumY += I * Y;
      ++used;
    }
  }

  if (!(sum > 0.0) || !std::isfinite(sum)) {
    return out;
  }

  double cx = sumX / sum;
  double cy = sumY / sum;

  // Optional second pass after gating (same as first when gate already applied).
  if (refineCentroid) {
    sum = 0.0;
    sumX = 0.0;
    sumY = 0.0;
    used = 0;
    for (std::size_t y = 0; y < frame.height; ++y) {
      for (std::size_t x = 0; x < frame.width; ++x) {
        const double I = intensityAt(x, y);
        if (I == 0.0) {
          continue;
        }
        const double X = (static_cast<double>(x) + 0.5) * dx;
        const double Y = (static_cast<double>(y) + 0.5) * dy;
        sum += I;
        sumX += I * X;
        sumY += I * Y;
        ++used;
      }
    }
    if (!(sum > 0.0)) {
      return out;
    }
    cx = sumX / sum;
    cy = sumY / sum;
  }

  double sxx = 0.0;
  double syy = 0.0;
  double sxy = 0.0;

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      const double I = intensityAt(x, y);
      if (I == 0.0) {
        continue;
      }
      const double X = (static_cast<double>(x) + 0.5) * dx - cx;
      const double Y = (static_cast<double>(y) + 0.5) * dy - cy;
      sxx += I * X * X;
      syy += I * Y * Y;
      sxy += I * X * Y;
    }
  }

  sxx /= sum;
  syy /= sum;
  sxy /= sum;

  const double diamX = 4.0 * std::sqrt(std::max(0.0, sxx));
  const double diamY = 4.0 * std::sqrt(std::max(0.0, syy));

  const double trace = sxx + syy;
  const double det = sxx * syy - sxy * sxy;
  const double disc = std::sqrt(std::max(0.0, 0.25 * trace * trace - det));
  const double lambda1 = 0.5 * trace + disc;
  const double lambda2 = 0.5 * trace - disc;

  const double diamMajor = 4.0 * std::sqrt(std::max(0.0, lambda1));
  const double diamMinor = 4.0 * std::sqrt(std::max(0.0, lambda2));

  double orientation = 0.0;
  if (std::abs(sxy) > 1e-30 || std::abs(sxx - syy) > 1e-30) {
    orientation = 0.5 * std::atan2(2.0 * sxy, sxx - syy);
  }

  double ellipticity = 0.0;
  if (diamMajor > 0.0) {
    ellipticity = diamMinor / diamMajor;
  }

  out.totalPower = sum;
  out.centroidX_um = cx;
  out.centroidY_um = cy;
  out.diameterX_um = diamX;
  out.diameterY_um = diamY;
  out.diameterMajor_um = diamMajor;
  out.diameterMinor_um = diamMinor;
  out.orientation_rad = orientation;
  out.ellipticity = ellipticity;
  out.pixelsUsed = used;
  out.valid = true;
  return out;
}

}  // namespace

BeamFrame applyNoiseFloor(const BeamFrame& frame,
                          const NoiseFloorOptions& opt,
                          double* outThreshold,
                          double* outNoiseSigma) {
  BeamFrame out = frame;
  if (out.empty()) {
    if (outThreshold) {
      *outThreshold = 0.0;
    }
    if (outNoiseSigma) {
      *outNoiseSigma = 0.0;
    }
    return out;
  }

  double noiseSigma = 0.0;
  if (opt.knownNoiseSigma > 0.0) {
    noiseSigma = opt.knownNoiseSigma;
  } else if (opt.mode == NoiseFloorMode::SigmaAboveBg) {
    noiseSigma = estimateBorderNoiseSigma(out);
  }

  double threshold = 0.0;
  switch (opt.mode) {
    case NoiseFloorMode::None:
      threshold = 0.0;
      break;
    case NoiseFloorMode::Absolute:
      threshold = std::max(0.0, opt.absoluteLevel);
      break;
    case NoiseFloorMode::FractionOfPeak:
      threshold = std::max(0.0, opt.fractionOfPeak) * peakIntensity(out);
      break;
    case NoiseFloorMode::SigmaAboveBg:
      threshold = std::max(0.0, opt.noiseSigmaMult) * noiseSigma;
      break;
  }

  if (threshold > 0.0) {
    for (double& v : out.intensity) {
      if (v < threshold) {
        v = 0.0;
      }
    }
  }

  if (outThreshold) {
    *outThreshold = threshold;
  }
  if (outNoiseSigma) {
    *outNoiseSigma = noiseSigma;
  }
  return out;
}

Iso11146Metrics computeIso11146(const BeamFrame& frame, bool clipNegative) {
  return momentsFromPrepared(frame, clipNegative, false, 0.0, 0.0, 0.0);
}

Iso11146Metrics computeIso11146(const BeamFrame& frame, const MomentOptions& options) {
  if (frame.empty()) {
    return {};
  }

  BeamFrame prepared = prepareAnalysisFrame(frame, options.prep);
  double bgLevel = 0.0;
  if (options.prep.background.mode != BackgroundMode::None) {
    // Re-estimate for diagnostics (prep already subtracted).
    bgLevel = estimateBackground(frame, options.prep.background,
                                 options.prep.useRoi ? &options.prep.roi : nullptr);
  }

  double threshold = 0.0;
  double noiseSigma = 0.0;
  BeamFrame gated = applyNoiseFloor(prepared, options.noise, &threshold, &noiseSigma);

  return momentsFromPrepared(gated, options.clipNegative,
                             options.refineCentroidAfterGate, threshold, noiseSigma,
                             bgLevel);
}

BeamFrame makeGaussianFrame(std::size_t width,
                            std::size_t height,
                            double centerX_px,
                            double centerY_px,
                            double sigmaX_px,
                            double sigmaY_px,
                            double amplitude,
                            double pixelSizeX_um,
                            double pixelSizeY_um,
                            double background) {
  BeamFrame frame;
  frame.resize(width, height);
  frame.pixelSizeX_um = pixelSizeX_um;
  frame.pixelSizeY_um = pixelSizeY_um;

  const double inv2sx2 = 1.0 / (2.0 * sigmaX_px * sigmaX_px);
  const double inv2sy2 = 1.0 / (2.0 * sigmaY_px * sigmaY_px);

  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      const double dx = static_cast<double>(x) + 0.5 - centerX_px;
      const double dy = static_cast<double>(y) + 0.5 - centerY_px;
      const double g = amplitude * std::exp(-(dx * dx * inv2sx2 + dy * dy * inv2sy2));
      frame.intensity[y * width + x] = background + g;
    }
  }
  return frame;
}

BeamFrame addGaussianNoise(const BeamFrame& frame, double sigma, unsigned seed) {
  BeamFrame out = frame;
  if (out.empty() || !(sigma > 0.0)) {
    return out;
  }
  // Simple LCG + Box–Muller (no <random> dependency quirks across toolchains).
  std::uint32_t state = seed ? seed : 1u;
  auto nextU01 = [&]() {
    state = state * 1664525u + 1013904223u;
    return (state + 0.5) / 4294967296.0;
  };
  for (std::size_t i = 0; i < out.intensity.size(); i += 2) {
    const double u1 = std::max(1e-12, nextU01());
    const double u2 = nextU01();
    const double r = sigma * std::sqrt(-2.0 * std::log(u1));
    const double theta = 2.0 * 3.14159265358979323846 * u2;
    out.intensity[i] += r * std::cos(theta);
    if (i + 1 < out.intensity.size()) {
      out.intensity[i + 1] += r * std::sin(theta);
    }
  }
  return out;
}

}  // namespace beam::core
