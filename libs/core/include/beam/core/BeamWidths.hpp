#pragma once

#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"

namespace beam::core {

/// Knife-edge clip preset (ISO / Siegman-style laboratory practice).
enum class KnifeEdgePreset {
  Clip1090,   ///< 10%/90%, multiplier 1.561 (Siegman et al.)
  Clip1684,   ///< 16%/84%, multiplier 2.0 (ISO 13694-related practice)
  Programmable
};

struct KnifeEdgeOptions {
  KnifeEdgePreset preset = KnifeEdgePreset::Clip1090;
  double clipLow = 0.10;    ///< Fraction of total projected power (0–1)
  double clipHigh = 0.90;
  double multiplier = 1.561;
};

/// Resolve preset → clip/multiplier (Programmable leaves user values).
[[nodiscard]] KnifeEdgeOptions resolveKnifeEdge(const KnifeEdgeOptions& in);

struct AxisWidthResult {
  double widthX_um = 0.0;
  double widthY_um = 0.0;
  double diameter_um = 0.0;  ///< Equivalent circular diameter when defined
  bool valid = false;
};

/// Simulated knife-edge widths from 1D projections (sum along orthogonal axis).
/// Width = multiplier * (x_high − x_low) where cumulative power hits clip levels.
[[nodiscard]] AxisWidthResult computeKnifeEdgeWidths(const BeamFrame& frame,
                                                     const KnifeEdgeOptions& opt = {});

/// Percent-of-peak clip along centroid row/column (legacy 1D).
/// Default clip=0.135 ≈ 1/e² intensity for TEM00; 0.5 → FWHM.
[[nodiscard]] AxisWidthResult computePercentPeakWidths(const BeamFrame& frame,
                                                       double clipOfPeak = 0.135,
                                                       const Iso11146Metrics* centroidHint = nullptr);

/// Percent-of-total energy: descending-energy clip level, then count pixels above
/// that level on the centroid row/column. Default 0.865 ≈ TEM00 second-moment energy.
[[nodiscard]] AxisWidthResult computePercentEnergyWidths(const BeamFrame& frame,
                                                         double enclosedFraction = 0.865,
                                                         const Iso11146Metrics* centroidHint = nullptr);

/// Encircled-power smallest aperture (EPSA / power-in-the-bucket): circular diameter
/// about the centroid enclosing `enclosedFraction` of total power.
[[nodiscard]] AxisWidthResult computeEncircledPowerDiameter(const BeamFrame& frame,
                                                           double enclosedFraction = 0.865,
                                                           const Iso11146Metrics* centroidHint = nullptr);

/// Aggregate of common commercial width tools for one prepared frame.
struct BeamWidthSuite {
  AxisWidthResult d4sigma;          ///< Filled from Iso11146Metrics when provided
  AxisWidthResult knife1090;
  AxisWidthResult knife1684;
  AxisWidthResult percentPeak;
  AxisWidthResult percentEnergy;
  AxisWidthResult encircled;
  bool valid = false;
};

[[nodiscard]] BeamWidthSuite computeBeamWidthSuite(const BeamFrame& frame,
                                                   const Iso11146Metrics* iso = nullptr);

}  // namespace beam::core
