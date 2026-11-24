#pragma once

#include "beam/core/BeamFrame.hpp"

#include <optional>

namespace beam::core {

/// Inclusive pixel rectangle [x0, x1] × [y0, y1].
struct RectRoi {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;

  [[nodiscard]] bool empty() const noexcept {
    return x1 < x0 || y1 < y0;
  }

  [[nodiscard]] int width() const noexcept { return empty() ? 0 : (x1 - x0 + 1); }
  [[nodiscard]] int height() const noexcept { return empty() ? 0 : (y1 - y0 + 1); }

  [[nodiscard]] bool contains(int x, int y) const noexcept {
    return !empty() && x >= x0 && x <= x1 && y >= y0 && y <= y1;
  }

  /// Clamp ROI into frame bounds; returns nullopt if no overlap.
  [[nodiscard]] std::optional<RectRoi> clampedTo(std::size_t width,
                                                 std::size_t height) const;
};

enum class BackgroundMode {
  None,
  Constant,
  BorderMean,  ///< Mean of a border strip (default 2 px) outside analysis ROI
  RoiMean,     ///< Mean of a dedicated background ROI
};

struct BackgroundOptions {
  BackgroundMode mode = BackgroundMode::None;
  double constantValue = 0.0;
  int borderWidth_px = 2;
  RectRoi backgroundRoi{};
  bool clipNegative = true;  ///< After subtraction, clip I < 0 to 0
};

struct AnalysisPrep {
  RectRoi roi{};               ///< Empty → full frame (legacy crop path)
  bool useRoi = false;
  /// When true with a non-empty aoiBounds, zero outside AOI (no crop) so
  /// centroids stay in full-frame coordinates — preferred for shaped AOI.
  bool useAoi = false;
  RectRoi aoiBounds{};
  /// 0=Rect, 1=Circle, 2=Ellipse (mirrors AoiShape; kept as int to avoid
  /// include cycles between Roi.hpp and Aoi.hpp).
  int aoiShape = 0;
  BackgroundOptions background{};
};

/// Estimate a scalar background level from the frame and options.
[[nodiscard]] double estimateBackground(const BeamFrame& frame,
                                        const BackgroundOptions& opt,
                                        const RectRoi* analysisRoi = nullptr);

/// Subtract background; optionally clip negatives. Does not crop.
[[nodiscard]] BeamFrame subtractBackground(const BeamFrame& frame,
                                           double level,
                                           bool clipNegative = true);

/// Crop to ROI (clamped). Pixel sizes preserved. Empty ROI → copy of frame.
[[nodiscard]] BeamFrame cropToRoi(const BeamFrame& frame, const RectRoi& roi);

/// Full prep: optional AOI mask or crop + background subtraction for analysis.
[[nodiscard]] BeamFrame prepareAnalysisFrame(const BeamFrame& frame,
                                             const AnalysisPrep& prep);

}  // namespace beam::core
