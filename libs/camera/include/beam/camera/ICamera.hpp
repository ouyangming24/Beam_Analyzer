#pragma once

#include "beam/core/BeamFrame.hpp"

#include <memory>
#include <string>
#include <vector>

namespace beam::camera {

struct CameraInfo {
  std::string id;
  std::string name;
  std::string transport;  ///< e.g. "null", "simulated", "sdk:<vendor>"
};

struct CaptureResult {
  beam::core::BeamFrame frame;
  bool ok = false;
  std::string message;
};

/// Abstract camera port for Phase 3 SDK adapters.
class ICamera {
 public:
  virtual ~ICamera() = default;

  [[nodiscard]] virtual CameraInfo info() const = 0;
  [[nodiscard]] virtual bool open() = 0;
  virtual void close() = 0;
  [[nodiscard]] virtual bool isOpen() const = 0;

  /// Grab one frame. Implementations may block briefly.
  [[nodiscard]] virtual CaptureResult grab() = 0;
};

[[nodiscard]] std::unique_ptr<ICamera> createNullCamera();
[[nodiscard]] std::unique_ptr<ICamera> createSimulatedCamera(
    std::size_t width = 128,
    std::size_t height = 128,
    double pixelSize_um = 5.0);

}  // namespace beam::camera
