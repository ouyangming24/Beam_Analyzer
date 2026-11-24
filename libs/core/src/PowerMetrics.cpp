#include "beam/core/PowerMetrics.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

PowerMetrics computePowerMetrics(const BeamFrame& frame, const PowerCalibration& cal) {
  PowerMetrics out;
  if (frame.empty() || frame.intensity.size() != frame.width * frame.height) {
    return out;
  }

  double sum = 0.0;
  double peak = frame.intensity.front();
  for (double v : frame.intensity) {
    sum += v;
    peak = std::max(peak, v);
  }

  out.sumCounts = sum;
  out.peakCounts = peak;
  out.meanCounts = sum / static_cast<double>(frame.intensity.size());
  out.calibrated = cal.isCalibrated();
  if (out.calibrated) {
    out.power_W = sum * cal.countsToWatts;
    const double exposure = (cal.exposure_s > 0.0 && std::isfinite(cal.exposure_s))
                                ? cal.exposure_s
                                : 1.0;
    out.energy_J = out.power_W * exposure;
  }
  out.valid = std::isfinite(sum);
  return out;
}

}  // namespace beam::core
