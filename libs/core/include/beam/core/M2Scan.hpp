#pragma once

#include "beam/core/M2Fit.hpp"

#include <string>
#include <vector>

namespace beam::core {

/// One axial station in an M² multi-point scan (lab z + measured diameters).
struct M2ScanPoint {
  double z_mm = 0.0;
  double diameterX_um = 0.0;
  double diameterY_um = 0.0;
  std::string sourceLabel;  ///< e.g. CSV path, "coarse", "fine", "synthetic"
  bool valid = false;
};

struct M2ScanResult {
  double wavelength_nm = 1064.0;
  std::vector<M2ScanPoint> points;
  M2FitResult fitX;
  M2FitResult fitY;
  /// Combined isotropic diameter = sqrt((Dx²+Dy²)/2) fit (optional convenience).
  M2FitResult fitCombined;
  bool multiAxis = true;
  bool valid = false;
  std::string message;
};

/// Fit X, Y, and combined diameters from a multi-point scan.
[[nodiscard]] M2ScanResult analyzeM2Scan(const std::vector<M2ScanPoint>& points,
                                         double wavelength_nm,
                                         const M2FitOptions& fitOptions = {});

/// Fit a single axis only (X if useY=false, else Y).
[[nodiscard]] M2ScanResult analyzeM2ScanSingleAxis(const std::vector<M2ScanPoint>& points,
                                                   double wavelength_nm,
                                                   bool useY,
                                                   const M2FitOptions& fitOptions = {});

/// Plain-text / Markdown-ish report for export.
[[nodiscard]] std::string formatM2Report(const M2ScanResult& result,
                                         const std::string& title = "M² Scan Report");

/// Human-readable quality flag names.
[[nodiscard]] std::string describeQualityFlags(unsigned flags);

}  // namespace beam::core
