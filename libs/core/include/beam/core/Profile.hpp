#pragma once

#include "beam/core/BeamFrame.hpp"

#include <vector>

namespace beam::core {

enum class ProfileAxis {
  Horizontal,  ///< Along X at fixed row
  Vertical,    ///< Along Y at fixed column
  Arbitrary,   ///< Bresenham line between two pixels
};

struct LineProfile {
  ProfileAxis axis = ProfileAxis::Horizontal;
  std::vector<double> position_um;  ///< Distance along profile from start
  std::vector<double> intensity;
  int row_or_col = 0;  ///< Fixed row (H) or column (V); unused for Arbitrary
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  bool valid = false;
};

/// Horizontal line at row (pixel). Positions in µm from left edge.
[[nodiscard]] LineProfile extractHorizontalProfile(const BeamFrame& frame, int row);

/// Vertical line at column (pixel). Positions in µm from top edge.
[[nodiscard]] LineProfile extractVerticalProfile(const BeamFrame& frame, int col);

/// Arbitrary line via Bresenham sampling.
[[nodiscard]] LineProfile extractLineProfile(const BeamFrame& frame,
                                             int x0,
                                             int y0,
                                             int x1,
                                             int y1);

/// Crosshair pair through a pixel (horizontal + vertical).
struct CrosshairProfiles {
  LineProfile horizontal;
  LineProfile vertical;
};

[[nodiscard]] CrosshairProfiles extractCrosshair(const BeamFrame& frame,
                                                 int x,
                                                 int y);

}  // namespace beam::core
