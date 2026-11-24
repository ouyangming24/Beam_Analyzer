#pragma once

#include "beam/core/BeamFrame.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace beam::camera {

/// Acquisition trigger mode (placeholders map to GenICam / UVC concepts).
enum class TriggerMode {
  FreeRun = 0,   ///< Continuous free-running acquisition
  Software = 1,  ///< Wait for softwareTrigger()
  Hardware = 2,  ///< External line (stub until SDK wires it)
};

[[nodiscard]] inline const char* toString(TriggerMode mode) {
  switch (mode) {
    case TriggerMode::FreeRun:
      return "FreeRun";
    case TriggerMode::Software:
      return "Software";
    case TriggerMode::Hardware:
      return "Hardware";
  }
  return "Unknown";
}

struct CameraInfo {
  std::string id;         ///< Stable registry / discovery id
  std::string name;       ///< Human-readable label
  std::string transport;  ///< e.g. "simulated", "gentl", "uvc", "opencv", "sdk:<vendor>"
  std::string backendId;  ///< Owning backend plugin id
  bool available = true;  ///< False when stubbed / SDK missing
};

/// Exposure / gain / trigger placeholders common to scientific cameras.
struct CameraControls {
  double exposure_us = 1000.0;  ///< Exposure time (µs)
  double gain_db = 0.0;         ///< Analog/digital gain (dB)
  TriggerMode trigger = TriggerMode::FreeRun;
  double targetFps = 30.0;      ///< Live stream target FPS (FreeRun)
};

struct CaptureResult {
  beam::core::BeamFrame frame;
  bool ok = false;
  std::string message;
  std::chrono::steady_clock::time_point timestamp{};
  std::uint64_t frameIndex = 0;
};

/// Abstract camera port — keep stable for vendor SDK plugins.
class ICamera {
 public:
  virtual ~ICamera() = default;

  [[nodiscard]] virtual CameraInfo info() const = 0;
  [[nodiscard]] virtual bool open() = 0;
  virtual void close() = 0;
  [[nodiscard]] virtual bool isOpen() const = 0;

  /// Apply control placeholders. Implementations may clamp or ignore unsupported fields.
  [[nodiscard]] virtual bool setControls(const CameraControls& controls) = 0;
  [[nodiscard]] virtual CameraControls controls() const = 0;

  /// Start continuous / triggered acquisition (live path).
  [[nodiscard]] virtual bool startAcquisition() = 0;
  virtual void stopAcquisition() = 0;
  [[nodiscard]] virtual bool isAcquiring() const = 0;

  /// Fire one software trigger when TriggerMode::Software.
  [[nodiscard]] virtual bool softwareTrigger() = 0;

  /// Grab one frame. May block briefly waiting for the next buffer.
  [[nodiscard]] virtual CaptureResult grab() = 0;
};

/// Convenience factories (also registered via CameraRegistry).
[[nodiscard]] std::unique_ptr<ICamera> createNullCamera();
[[nodiscard]] std::unique_ptr<ICamera> createSimulatedCamera(
    std::size_t width = 128,
    std::size_t height = 128,
    double pixelSize_um = 5.0);

}  // namespace beam::camera
