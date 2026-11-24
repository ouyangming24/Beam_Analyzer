#include "beam/core/MotorScan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

namespace beam::core {
namespace {

std::vector<double> uniqueSortedMerge(const std::vector<double>& a,
                                      const std::vector<double>& b,
                                      double tol,
                                      bool skipDup) {
  std::vector<double> out = a;
  for (double z : b) {
    if (skipDup) {
      const bool dup = std::any_of(out.begin(), out.end(), [&](double e) {
        return std::abs(e - z) <= tol;
      });
      if (dup) {
        continue;
      }
    }
    out.push_back(z);
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

std::vector<double> linspaceInclusive(double start_mm, double end_mm, double step_mm) {
  std::vector<double> out;
  if (!(std::abs(step_mm) > 0.0) || !std::isfinite(start_mm) || !std::isfinite(end_mm) ||
      !std::isfinite(step_mm)) {
    return out;
  }
  const double span = end_mm - start_mm;
  if (span == 0.0) {
    out.push_back(start_mm);
    return out;
  }
  const double step = std::copysign(std::abs(step_mm), span);
  const int n = static_cast<int>(std::floor(std::abs(span) / std::abs(step) + 1e-12)) + 1;
  out.reserve(static_cast<std::size_t>(std::max(1, n)));
  for (int i = 0; i < n; ++i) {
    out.push_back(start_mm + step * static_cast<double>(i));
  }
  // Ensure endpoint included if close
  if (out.empty() || std::abs(out.back() - end_mm) > 1e-9 * std::max(1.0, std::abs(end_mm))) {
    if (out.empty() || std::abs(out.back() - end_mm) > std::abs(step) * 0.25) {
      out.push_back(end_mm);
    } else {
      out.back() = end_mm;
    }
  }
  return out;
}

std::vector<double> planCoarsePositions(const CoarseScanConfig& cfg) {
  return linspaceInclusive(cfg.zStart_mm, cfg.zEnd_mm, cfg.step_mm);
}

MotorScanPlan planMotorScan(const CoarseScanConfig& coarse,
                            const FineScanConfig& fine,
                            double estimatedWaistZ_mm,
                            bool includeFine) {
  MotorScanPlan plan;
  plan.coarsePositions_mm = planCoarsePositions(coarse);
  plan.estimatedWaistZ_mm = estimatedWaistZ_mm;
  plan.allPositions_mm = plan.coarsePositions_mm;

  if (!includeFine) {
    plan.message = "Coarse-only plan";
    return plan;
  }
  if (!(fine.step_mm > 0.0) || !(fine.halfRange_mm > 0.0)) {
    plan.message = "Invalid fine scan config";
    return plan;
  }

  plan.finePositions_mm = linspaceInclusive(estimatedWaistZ_mm - fine.halfRange_mm,
                                            estimatedWaistZ_mm + fine.halfRange_mm,
                                            fine.step_mm);
  plan.finePlanned = !plan.finePositions_mm.empty();
  plan.allPositions_mm =
      uniqueSortedMerge(plan.coarsePositions_mm, plan.finePositions_mm,
                        fine.duplicateTol_mm, fine.skipDuplicates);
  plan.message = plan.finePlanned ? "Coarse+fine plan" : "Fine plan empty";
  return plan;
}

SimulatedStage::SimulatedStage(double start_mm) : z_mm_(start_mm) {}

bool SimulatedStage::moveAbsolute_mm(double z_mm) {
  if (!std::isfinite(z_mm)) {
    return false;
  }
  z_mm_ = z_mm;
  return true;
}

double SimulatedStage::position_mm() const {
  return z_mm_;
}

std::string SimulatedStage::name() const {
  return "SimulatedStage";
}

AcquireAtZFn makeSyntheticAcquire(const SyntheticBeamModel& model) {
  return [model](double z_mm, double& dx, double& dy, std::string& label) {
    dx = hyperbolicDiameter_um(z_mm, model.waistDiameterX_um, model.waistZ_mm, model.m2X,
                               model.wavelength_nm);
    dy = hyperbolicDiameter_um(z_mm, model.waistDiameterY_um, model.waistZ_mm, model.m2Y,
                               model.wavelength_nm);
    if (model.diameterNoise_um > 0.0) {
      // Deterministic LCG noise from z + seed
      auto hash = [&](double z, unsigned salt) {
        std::uint32_t s = model.seed ^ salt;
        const auto bits = static_cast<std::uint32_t>(
            std::llround(z * 1000.0) & 0xffffffffll);
        s ^= bits + 0x9e3779b9u + (s << 6) + (s >> 2);
        s = s * 1664525u + 1013904223u;
        const double u = (s + 0.5) / 4294967296.0;
        return (u * 2.0 - 1.0) * model.diameterNoise_um;
      };
      dx = std::max(1e-6, dx + hash(z_mm, 1u));
      dy = std::max(1e-6, dy + hash(z_mm, 2u));
    }
    label = "synthetic";
    return true;
  };
}

ScanSessionResult runScanSession(IStage& stage,
                                 const AcquireAtZFn& acquire,
                                 const ScanSessionConfig& config,
                                 const ScanProgressFn& onProgress) {
  ScanSessionResult out;
  if (!acquire) {
    out.message = "Acquire callback is empty";
    return out;
  }

  // --- Coarse ---
  out.plan = planMotorScan(config.coarse, config.fine, 0.0, false);
  out.points.clear();
  out.points.reserve(out.plan.coarsePositions_mm.size() + 32);

  const auto runPositions = [&](const std::vector<double>& zs, ScanPhase phase) {
    for (std::size_t i = 0; i < zs.size(); ++i) {
      const double z = zs[i];
      if (onProgress) {
        onProgress(ScanSessionProgress{phase, i, zs.size(), z});
      }
      if (!stage.moveAbsolute_mm(z)) {
        continue;
      }
      double dx = 0.0;
      double dy = 0.0;
      std::string label;
      if (!acquire(stage.position_mm(), dx, dy, label)) {
        M2ScanPoint bad;
        bad.z_mm = z;
        bad.valid = false;
        bad.sourceLabel = label.empty() ? "acquire-failed" : label;
        out.points.push_back(bad);
        continue;
      }
      M2ScanPoint p;
      p.z_mm = stage.position_mm();
      p.diameterX_um = dx;
      p.diameterY_um = dy;
      p.sourceLabel = label;
      p.valid = dx > 0.0 && dy > 0.0;
      out.points.push_back(p);
    }
  };

  runPositions(out.plan.coarsePositions_mm, ScanPhase::Coarse);
  out.coarseOnly = analyzeM2Scan(out.points, config.wavelength_nm, config.fitOptions);

  double waistGuess = 0.0;
  if (config.autoCenterFineOnWaist) {
    if (out.coarseOnly.fitX.valid) {
      waistGuess = out.coarseOnly.fitX.waistZ_mm;
    } else if (out.coarseOnly.fitY.valid) {
      waistGuess = out.coarseOnly.fitY.waistZ_mm;
    } else if (out.coarseOnly.fitCombined.valid) {
      waistGuess = out.coarseOnly.fitCombined.waistZ_mm;
    } else {
      // Fallback: z of minimum geometric-mean diameter
      double best = 0.0;
      double bestD = std::numeric_limits<double>::infinity();
      for (const auto& p : out.points) {
        if (!p.valid) {
          continue;
        }
        const double d = std::sqrt(p.diameterX_um * p.diameterY_um);
        if (d < bestD) {
          bestD = d;
          best = p.z_mm;
        }
      }
      waistGuess = best;
    }
  }

  if (config.runFineAfterCoarse) {
    out.plan = planMotorScan(config.coarse, config.fine, waistGuess, true);
    // Acquire only fine positions not already measured
    std::vector<double> fineOnly;
    for (double z : out.plan.finePositions_mm) {
      const bool have = std::any_of(out.points.begin(), out.points.end(), [&](const M2ScanPoint& p) {
        return p.valid && std::abs(p.z_mm - z) <= config.fine.duplicateTol_mm;
      });
      if (!have) {
        fineOnly.push_back(z);
      }
    }
    runPositions(fineOnly, ScanPhase::Fine);
  } else {
    out.plan.estimatedWaistZ_mm = waistGuess;
  }

  out.full = analyzeM2Scan(out.points, config.wavelength_nm, config.fitOptions);
  out.valid = out.full.valid;
  out.message = out.valid ? "Scan session complete" : out.full.message;
  return out;
}

std::string formatDzCurveCsv(const M2ScanResult& result) {
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(8);
  ss << "z_mm,diameter_x_um,diameter_y_um,diameter_combined_um,fitted_x_um,fitted_y_um,phase\n";
  for (const auto& p : result.points) {
    if (!p.valid) {
      continue;
    }
    const double dComb =
        std::sqrt(0.5 * (p.diameterX_um * p.diameterX_um + p.diameterY_um * p.diameterY_um));
    double fx = 0.0;
    double fy = 0.0;
    if (result.fitX.valid) {
      fx = hyperbolicDiameter_um(p.z_mm, result.fitX.waistDiameter_um, result.fitX.waistZ_mm,
                                 result.fitX.m2, result.wavelength_nm);
    }
    if (result.fitY.valid) {
      fy = hyperbolicDiameter_um(p.z_mm, result.fitY.waistDiameter_um, result.fitY.waistZ_mm,
                                 result.fitY.m2, result.wavelength_nm);
    }
    ss << p.z_mm << "," << p.diameterX_um << "," << p.diameterY_um << "," << dComb << ","
       << fx << "," << fy << "," << p.sourceLabel << "\n";
  }
  return ss.str();
}

std::string formatDzAsciiPlot(const M2ScanResult& result, int width, int height) {
  width = std::max(20, width);
  height = std::max(8, height);

  std::vector<double> zs;
  std::vector<double> ds;
  for (const auto& p : result.points) {
    if (!p.valid) {
      continue;
    }
    zs.push_back(p.z_mm);
    ds.push_back(
        std::sqrt(0.5 * (p.diameterX_um * p.diameterX_um + p.diameterY_um * p.diameterY_um)));
  }
  std::ostringstream ss;
  if (zs.size() < 2) {
    ss << "(need ≥2 valid points for ASCII D(z) plot)\n";
    return ss.str();
  }

  const double zMin = *std::min_element(zs.begin(), zs.end());
  const double zMax = *std::max_element(zs.begin(), zs.end());
  const double dMin = *std::min_element(ds.begin(), ds.end());
  const double dMax = *std::max_element(ds.begin(), ds.end());
  const double zSpan = std::max(1e-9, zMax - zMin);
  const double dSpan = std::max(1e-9, dMax - dMin);

  std::vector<std::string> grid(static_cast<std::size_t>(height),
                                std::string(static_cast<std::size_t>(width), ' '));

  auto plot = [&](double z, double d, char ch) {
    const int x = static_cast<int>(std::lround((z - zMin) / zSpan * (width - 1)));
    const int y =
        static_cast<int>(std::lround((1.0 - (d - dMin) / dSpan) * (height - 1)));
    if (x >= 0 && x < width && y >= 0 && y < height) {
      grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = ch;
    }
  };

  for (std::size_t i = 0; i < zs.size(); ++i) {
    plot(zs[i], ds[i], '*');
  }

  // Overlay combined fit curve
  if (result.fitCombined.valid) {
    for (int x = 0; x < width; ++x) {
      const double z = zMin + zSpan * static_cast<double>(x) / (width - 1);
      const double d = hyperbolicDiameter_um(z, result.fitCombined.waistDiameter_um,
                                             result.fitCombined.waistZ_mm,
                                             result.fitCombined.m2, result.wavelength_nm);
      plot(z, d, '.');
    }
    // Re-plot measured so they stay visible
    for (std::size_t i = 0; i < zs.size(); ++i) {
      plot(zs[i], ds[i], '*');
    }
  }

  ss << "D(z) ASCII  z:[" << zMin << "," << zMax << "] mm  D:[" << dMin << "," << dMax
     << "] µm  (* measured, . fit)\n";
  for (const auto& row : grid) {
    ss << '|' << row << "|\n";
  }
  return ss.str();
}

}  // namespace beam::core
