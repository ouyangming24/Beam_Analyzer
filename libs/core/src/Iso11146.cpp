#include "beam/core/Iso11146.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

Iso11146Metrics computeIso11146(const BeamFrame& frame, bool clipNegative) {
  Iso11146Metrics out;
  if (frame.empty() || frame.intensity.size() != frame.width * frame.height) {
    return out;
  }

  const double dx = frame.pixelSizeX_um;
  const double dy = frame.pixelSizeY_um;

  double sum = 0.0;
  double sumX = 0.0;
  double sumY = 0.0;

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      double I = frame.intensity[y * frame.width + x];
      if (clipNegative) {
        I = std::max(0.0, I);
      }
      if (I == 0.0) {
        continue;
      }
      const double X = (static_cast<double>(x) + 0.5) * dx;
      const double Y = (static_cast<double>(y) + 0.5) * dy;
      sum += I;
      sumX += I * X;
      sumY += I * Y;
    }
  }

  if (!(sum > 0.0) || !std::isfinite(sum)) {
    return out;
  }

  const double cx = sumX / sum;
  const double cy = sumY / sum;

  double sxx = 0.0;
  double syy = 0.0;
  double sxy = 0.0;

  for (std::size_t y = 0; y < frame.height; ++y) {
    for (std::size_t x = 0; x < frame.width; ++x) {
      double I = frame.intensity[y * frame.width + x];
      if (clipNegative) {
        I = std::max(0.0, I);
      }
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

  // Lab-frame diameters dσ = 4σ
  const double diamX = 4.0 * std::sqrt(std::max(0.0, sxx));
  const double diamY = 4.0 * std::sqrt(std::max(0.0, syy));

  // Principal axes via 2x2 covariance eigen-decomposition
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
  out.valid = true;
  return out;
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

}  // namespace beam::core
