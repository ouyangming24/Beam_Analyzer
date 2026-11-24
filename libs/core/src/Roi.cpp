#include "beam/core/Roi.hpp"
#include "beam/core/Aoi.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

std::optional<RectRoi> RectRoi::clampedTo(std::size_t width,
                                          std::size_t height) const {
  if (width == 0 || height == 0 || empty()) {
    return std::nullopt;
  }
  RectRoi c;
  c.x0 = std::max(0, x0);
  c.y0 = std::max(0, y0);
  c.x1 = std::min(static_cast<int>(width) - 1, x1);
  c.y1 = std::min(static_cast<int>(height) - 1, y1);
  if (c.empty()) {
    return std::nullopt;
  }
  return c;
}

double estimateBackground(const BeamFrame& frame,
                          const BackgroundOptions& opt,
                          const RectRoi* analysisRoi) {
  if (frame.empty()) {
    return 0.0;
  }

  switch (opt.mode) {
    case BackgroundMode::None:
      return 0.0;
    case BackgroundMode::Constant:
      return opt.constantValue;
    case BackgroundMode::RoiMean: {
      const auto clamped = opt.backgroundRoi.clampedTo(frame.width, frame.height);
      if (!clamped) {
        return 0.0;
      }
      double sum = 0.0;
      std::size_t n = 0;
      for (int y = clamped->y0; y <= clamped->y1; ++y) {
        for (int x = clamped->x0; x <= clamped->x1; ++x) {
          sum += frame.intensity[static_cast<std::size_t>(y) * frame.width +
                                 static_cast<std::size_t>(x)];
          ++n;
        }
      }
      return n > 0 ? sum / static_cast<double>(n) : 0.0;
    }
    case BackgroundMode::BorderMean: {
      RectRoi roi;
      if (analysisRoi && !analysisRoi->empty()) {
        const auto c = analysisRoi->clampedTo(frame.width, frame.height);
        if (c) {
          roi = *c;
        } else {
          roi = {0, 0, static_cast<int>(frame.width) - 1,
                 static_cast<int>(frame.height) - 1};
        }
      } else {
        roi = {0, 0, static_cast<int>(frame.width) - 1,
               static_cast<int>(frame.height) - 1};
      }
      const int bw = std::max(1, opt.borderWidth_px);
      double sum = 0.0;
      std::size_t n = 0;
      for (int y = roi.y0; y <= roi.y1; ++y) {
        for (int x = roi.x0; x <= roi.x1; ++x) {
          const bool onBorder = (x < roi.x0 + bw) || (x > roi.x1 - bw) ||
                                (y < roi.y0 + bw) || (y > roi.y1 - bw);
          if (!onBorder) {
            continue;
          }
          sum += frame.intensity[static_cast<std::size_t>(y) * frame.width +
                                 static_cast<std::size_t>(x)];
          ++n;
        }
      }
      return n > 0 ? sum / static_cast<double>(n) : 0.0;
    }
  }
  return 0.0;
}

BeamFrame subtractBackground(const BeamFrame& frame, double level, bool clipNegative) {
  BeamFrame out = frame;
  if (out.empty()) {
    return out;
  }
  for (double& v : out.intensity) {
    v -= level;
    if (clipNegative) {
      v = std::max(0.0, v);
    }
  }
  return out;
}

BeamFrame cropToRoi(const BeamFrame& frame, const RectRoi& roi) {
  if (frame.empty()) {
    return {};
  }
  const auto clamped = roi.clampedTo(frame.width, frame.height);
  if (!clamped) {
    return frame;
  }

  BeamFrame out;
  out.resize(static_cast<std::size_t>(clamped->width()),
             static_cast<std::size_t>(clamped->height()));
  out.pixelSizeX_um = frame.pixelSizeX_um;
  out.pixelSizeY_um = frame.pixelSizeY_um;

  for (int y = clamped->y0; y <= clamped->y1; ++y) {
    for (int x = clamped->x0; x <= clamped->x1; ++x) {
      const auto dx = static_cast<std::size_t>(x - clamped->x0);
      const auto dy = static_cast<std::size_t>(y - clamped->y0);
      out.intensity[dy * out.width + dx] =
          frame.intensity[static_cast<std::size_t>(y) * frame.width +
                          static_cast<std::size_t>(x)];
    }
  }
  return out;
}

BeamFrame prepareAnalysisFrame(const BeamFrame& frame, const AnalysisPrep& prep) {
  if (frame.empty()) {
    return {};
  }

  // Preferred path: shaped AOI mask keeps full-frame coordinates.
  if (prep.useAoi && !prep.aoiBounds.empty()) {
    AoiRegion aoi;
    aoi.shape = static_cast<AoiShape>(prep.aoiShape);
    aoi.bounds = prep.aoiBounds;
    BeamFrame work = frame;
    const RectRoi* roiPtr = &prep.aoiBounds;
    if (prep.background.mode != BackgroundMode::None) {
      const double level = estimateBackground(frame, prep.background, roiPtr);
      work = subtractBackground(frame, level, prep.background.clipNegative);
    }
    return applyAoiMask(work, aoi);
  }

  BeamFrame work = frame;
  const RectRoi* roiPtr = nullptr;
  RectRoi activeRoi{};

  if (prep.useRoi && !prep.roi.empty()) {
    activeRoi = prep.roi;
    roiPtr = &activeRoi;
    // Subtract on full frame first so border/ROI-mean uses original coords,
    // then crop — except Constant/None which can subtract after crop equally.
    if (prep.background.mode == BackgroundMode::BorderMean ||
        prep.background.mode == BackgroundMode::RoiMean) {
      const double level = estimateBackground(frame, prep.background, roiPtr);
      work = subtractBackground(frame, level, prep.background.clipNegative);
      work = cropToRoi(work, prep.roi);
      return work;
    }
    work = cropToRoi(frame, prep.roi);
  }

  if (prep.background.mode != BackgroundMode::None) {
    const double level = estimateBackground(work, prep.background, nullptr);
    work = subtractBackground(work, level, prep.background.clipNegative);
  }
  return work;
}

}  // namespace beam::core
