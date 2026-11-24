#pragma once

#include "beam/core/BeamFrame.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace beam::core {

enum class CaptureMode {
  Continuous,  ///< Every grabbed frame is kept / displayed
  Single,      ///< Soft-trigger style: one frame per request
  AverageN     ///< Average the last N acquired frames before publish
};

/// Running frame buffer: history + optional dark-frame subtraction + N-frame average.
class FrameBuffer {
 public:
  explicit FrameBuffer(std::size_t maxHistory = 120);

  void setMaxHistory(std::size_t n);
  [[nodiscard]] std::size_t maxHistory() const noexcept { return maxHistory_; }
  [[nodiscard]] std::size_t size() const noexcept { return history_.size(); }
  [[nodiscard]] bool empty() const noexcept { return history_.empty(); }

  void clear();
  void push(BeamFrame frame);

  [[nodiscard]] const BeamFrame* at(std::size_t index) const;
  [[nodiscard]] const BeamFrame* latest() const;

  void setDarkFrame(BeamFrame dark);
  void clearDarkFrame();
  [[nodiscard]] bool hasDarkFrame() const noexcept { return dark_.has_value(); }
  [[nodiscard]] const BeamFrame* darkFrame() const;

  /// Subtract stored dark (clip negatives). Returns copy; does not mutate buffer.
  [[nodiscard]] BeamFrame applyDark(const BeamFrame& frame) const;

  /// Average the last `n` frames (or all if fewer). Empty if buffer empty.
  [[nodiscard]] BeamFrame averageLast(std::size_t n) const;

  /// Average an explicit list (same geometry required).
  [[nodiscard]] static BeamFrame averageFrames(const std::vector<BeamFrame>& frames);

 private:
  std::size_t maxHistory_ = 120;
  std::deque<BeamFrame> history_;
  std::optional<BeamFrame> dark_;
};

/// Pixel-wise subtract dark from frame; clip to ≥0 when clipNegative.
[[nodiscard]] BeamFrame subtractDarkFrame(const BeamFrame& frame,
                                          const BeamFrame& dark,
                                          bool clipNegative = true);

}  // namespace beam::core
