#include "beam/core/Stability.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

StabilityMetrics computeStability(const std::vector<StabilitySample>& samples,
                                  double focalLength_mm) {
  StabilityMetrics out;
  out.focalLength_mm = focalLength_mm;
  out.sampleCount = samples.size();
  if (samples.size() < 2) {
    return out;
  }

  double tMin = samples.front().t_s;
  double tMax = samples.front().t_s;
  double sumX = 0.0;
  double sumY = 0.0;
  double sumP = 0.0;
  for (const auto& s : samples) {
    tMin = std::min(tMin, s.t_s);
    tMax = std::max(tMax, s.t_s);
    sumX += s.centroidX_um;
    sumY += s.centroidY_um;
    sumP += s.power;
  }
  const double n = static_cast<double>(samples.size());
  out.duration_s = std::max(0.0, tMax - tMin);
  out.meanX_um = sumX / n;
  out.meanY_um = sumY / n;
  out.meanPower = sumP / n;

  double varX = 0.0;
  double varY = 0.0;
  double varP = 0.0;
  double rMax = 0.0;
  for (const auto& s : samples) {
    const double dx = s.centroidX_um - out.meanX_um;
    const double dy = s.centroidY_um - out.meanY_um;
    const double dp = s.power - out.meanPower;
    varX += dx * dx;
    varY += dy * dy;
    varP += dp * dp;
    rMax = std::max(rMax, std::hypot(dx, dy));
  }
  varX /= n;
  varY /= n;
  varP /= n;
  out.stdX_um = std::sqrt(varX);
  out.stdY_um = std::sqrt(varY);
  out.stdPower = std::sqrt(varP);
  out.deltaR_um = 2.0 * std::sqrt(varX + varY);
  out.radialPeakToPeak_um = 2.0 * rMax;
  out.powerStability =
      out.meanPower > 0.0 ? std::clamp(1.0 - out.stdPower / out.meanPower, 0.0, 1.0)
                          : 0.0;

  if (focalLength_mm > 0.0) {
    // σ_um / f_mm → mrad: (σ_um / 1000) / f_mm * 1000 = σ_um / f_mm
    out.pointingStdX_mrad = out.stdX_um / focalLength_mm;
    out.pointingStdY_mrad = out.stdY_um / focalLength_mm;
    out.pointingDeltaR_mrad = out.deltaR_um / focalLength_mm;
  }

  out.valid = true;
  return out;
}

}  // namespace beam::core
