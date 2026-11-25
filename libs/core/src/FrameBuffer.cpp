#include "beam/core/FrameBuffer.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {

FrameBuffer::FrameBuffer(std::size_t maxHistory) : maxHistory_(std::max<std::size_t>(1, maxHistory)) {}

void FrameBuffer::setMaxHistory(std::size_t n) {
  maxHistory_ = std::max<std::size_t>(1, n);
  while (history_.size() > maxHistory_) {
    history_.pop_front();
  }
}

void FrameBuffer::clear() {
  history_.clear();
}

void FrameBuffer::push(BeamFrame frame) {
  history_.push_back(std::move(frame));
  while (history_.size() > maxHistory_) {
    history_.pop_front();
  }
}

const BeamFrame* FrameBuffer::at(std::size_t index) const {
  if (index >= history_.size()) {
    return nullptr;
  }
  return &history_[index];
}

const BeamFrame* FrameBuffer::latest() const {
  if (history_.empty()) {
    return nullptr;
  }
  return &history_.back();
}

void FrameBuffer::setDarkFrame(BeamFrame dark) {
  dark_ = std::move(dark);
}

void FrameBuffer::clearDarkFrame() {
  dark_.reset();
}

const BeamFrame* FrameBuffer::darkFrame() const {
  return dark_ ? &(*dark_) : nullptr;
}

BeamFrame FrameBuffer::applyDark(const BeamFrame& frame) const {
  if (!dark_) {
    return frame;
  }
  return subtractDarkFrame(frame, *dark_, true);
}

BeamFrame FrameBuffer::averageLast(std::size_t n) const {
  if (history_.empty() || n == 0) {
    return {};
  }
  const std::size_t count = std::min(n, history_.size());
  std::vector<BeamFrame> slice;
  slice.reserve(count);
  const std::size_t start = history_.size() - count;
  for (std::size_t i = start; i < history_.size(); ++i) {
    slice.push_back(history_[i]);
  }
  return averageFrames(slice);
}

BeamFrame FrameBuffer::averageFrames(const std::vector<BeamFrame>& frames) {
  if (frames.empty() || frames.front().empty()) {
    return {};
  }
  const auto& ref = frames.front();
  BeamFrame out;
  out.resize(ref.width, ref.height);
  out.pixelSizeX_um = ref.pixelSizeX_um;
  out.pixelSizeY_um = ref.pixelSizeY_um;
  std::size_t used = 0;
  for (const auto& f : frames) {
    if (f.width != ref.width || f.height != ref.height || f.intensity.size() != ref.intensity.size()) {
      continue;
    }
    for (std::size_t i = 0; i < out.intensity.size(); ++i) {
      out.intensity[i] += f.intensity[i];
    }
    ++used;
  }
  if (used == 0) {
    return {};
  }
  const double inv = 1.0 / static_cast<double>(used);
  for (double& v : out.intensity) {
    v *= inv;
  }
  return out;
}

BeamFrame subtractDarkFrame(const BeamFrame& frame, const BeamFrame& dark, bool clipNegative) {
  BeamFrame out = frame;
  if (frame.empty() || dark.width != frame.width || dark.height != frame.height ||
      dark.intensity.size() != frame.intensity.size()) {
    return out;
  }
  for (std::size_t i = 0; i < out.intensity.size(); ++i) {
    double v = frame.intensity[i] - dark.intensity[i];
    if (clipNegative && v < 0.0) {
      v = 0.0;
    }
    out.intensity[i] = v;
  }
  return out;
}

}  // namespace beam::core
