#pragma once

#include "beam/core/M2Fit.hpp"
#include "beam/core/M2Scan.hpp"
#include "beam/core/MomentOptions.hpp"

#include <functional>
#include <string>
#include <vector>

namespace beam::core {

enum class ScanPhase {
  Coarse,
  Fine,
  Manual,
};

/// User-configurable coarse linear z scan.
struct CoarseScanConfig {
  double zStart_mm = -50.0;
  double zEnd_mm = 50.0;
  double step_mm = 10.0;  ///< Absolute step; sign inferred from start→end
};

/// Fine scan centered on an estimated waist (typically from coarse fit).
struct FineScanConfig {
  double halfRange_mm = 10.0;  ///< ± around waistZ
  double step_mm = 1.0;
  /// If true, skip positions already visited in coarse (within tolerance).
  bool skipDuplicates = true;
  double duplicateTol_mm = 1e-3;
};

struct MotorScanPlan {
  std::vector<double> coarsePositions_mm;
  std::vector<double> finePositions_mm;
  /// Full ordered acquisition list (coarse then fine, duplicates pruned if requested).
  std::vector<double> allPositions_mm;
  double estimatedWaistZ_mm = 0.0;
  bool finePlanned = false;
  std::string message;
};

/// Generate evenly spaced positions from start to end inclusive (within step).
[[nodiscard]] std::vector<double> linspaceInclusive(double start_mm,
                                                    double end_mm,
                                                    double step_mm);

[[nodiscard]] std::vector<double> planCoarsePositions(const CoarseScanConfig& cfg);

/// Plan fine positions around waistZ; optionally merge into full list with coarse.
[[nodiscard]] MotorScanPlan planMotorScan(const CoarseScanConfig& coarse,
                                          const FineScanConfig& fine,
                                          double estimatedWaistZ_mm,
                                          bool includeFine);

/// Abstract z-stage (real motor or simulation).
class IStage {
 public:
  virtual ~IStage() = default;
  virtual bool moveAbsolute_mm(double z_mm) = 0;
  [[nodiscard]] virtual double position_mm() const = 0;
  [[nodiscard]] virtual std::string name() const = 0;
};

/// Simulated translation stage (instant moves; optional settle delay hook unused).
class SimulatedStage final : public IStage {
 public:
  explicit SimulatedStage(double start_mm = 0.0);
  bool moveAbsolute_mm(double z_mm) override;
  [[nodiscard]] double position_mm() const override;
  [[nodiscard]] std::string name() const override;

 private:
  double z_mm_ = 0.0;
};

/// Acquisition callback: given commanded z, return measured diameters (µm).
using AcquireAtZFn =
    std::function<bool(double z_mm, double& diameterX_um, double& diameterY_um,
                       std::string& label)>;

struct ScanSessionConfig {
  double wavelength_nm = 1064.0;
  CoarseScanConfig coarse{};
  FineScanConfig fine{};
  bool runFineAfterCoarse = true;
  /// If true, estimate waist from coarse X-fit (fallback Y/combined) before fine.
  bool autoCenterFineOnWaist = true;
  MomentOptions momentOptions{};  ///< Used by synthetic/helpers; callers may ignore
  M2FitOptions fitOptions{};
};

struct ScanSessionProgress {
  ScanPhase phase = ScanPhase::Coarse;
  std::size_t index = 0;
  std::size_t total = 0;
  double z_mm = 0.0;
};

using ScanProgressFn = std::function<void(const ScanSessionProgress&)>;

struct ScanSessionResult {
  MotorScanPlan plan;
  M2ScanResult coarseOnly;
  M2ScanResult full;  ///< After fine (or coarse-only if fine disabled)
  std::vector<M2ScanPoint> points;
  bool valid = false;
  std::string message;
};

/// Run a coarse→fine motor scan using the stage + acquire hook.
[[nodiscard]] ScanSessionResult runScanSession(IStage& stage,
                                               const AcquireAtZFn& acquire,
                                               const ScanSessionConfig& config,
                                               const ScanProgressFn& onProgress = {});

/// Synthetic TEM00 / non-ideal acquire hook for tests and CLI demos.
struct SyntheticBeamModel {
  double wavelength_nm = 1064.0;
  double waistDiameterX_um = 100.0;
  double waistDiameterY_um = 100.0;
  double waistZ_mm = 0.0;
  double m2X = 1.0;
  double m2Y = 1.0;
  double diameterNoise_um = 0.0;  ///< Additive Gaussian-ish noise amplitude
  unsigned seed = 42;
};

[[nodiscard]] AcquireAtZFn makeSyntheticAcquire(const SyntheticBeamModel& model);

/// ASCII / CSV helpers for D(z) plotting without a GUI.
[[nodiscard]] std::string formatDzCurveCsv(const M2ScanResult& result);
[[nodiscard]] std::string formatDzAsciiPlot(const M2ScanResult& result,
                                            int width = 72,
                                            int height = 18);

}  // namespace beam::core
