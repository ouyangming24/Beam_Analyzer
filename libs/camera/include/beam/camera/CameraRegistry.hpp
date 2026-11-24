#pragma once

#include "beam/camera/ICamera.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace beam::camera {

/// One discoverable / creatable camera backend (built-in or future SDK plugin).
struct CameraBackendDescriptor {
  std::string id;     ///< Stable id, e.g. "simulated", "gentl", "uvc"
  std::string name;   ///< Display name
  std::string transport;
  bool available = true;  ///< False when optional deps / GenTL producers missing
  std::string notes;      ///< How to enable / plug a real SDK later
};

using CameraFactoryFn =
    std::function<std::unique_ptr<ICamera>(const CameraInfo& device)>;

using CameraDiscoverFn = std::function<std::vector<CameraInfo>()>;

/// Process-wide registry: discover devices and create ICamera instances.
///
/// Real vendor SDKs should:
/// 1. Implement ICamera
/// 2. Call CameraRegistry::instance().registerBackend(...) at load time
///    (static initializer, Qt plugin, or explicit registerAllBuiltInBackends())
/// 3. Keep proprietary headers/libs behind optional CMake options
class CameraRegistry {
 public:
  static CameraRegistry& instance();

  void clear();  ///< Test helper — wipe registrations

  void registerBackend(CameraBackendDescriptor desc,
                       CameraDiscoverFn discover,
                       CameraFactoryFn factory);

  [[nodiscard]] std::vector<CameraBackendDescriptor> backends() const;
  [[nodiscard]] std::optional<CameraBackendDescriptor> findBackend(
      const std::string& backendId) const;

  /// Union of all backends' discover() results.
  [[nodiscard]] std::vector<CameraInfo> discoverAll() const;

  /// Create by device id from a prior discoverAll() / discover() entry.
  [[nodiscard]] std::unique_ptr<ICamera> create(const std::string& deviceId) const;

  /// Create by backend id with a synthetic CameraInfo (id/name from args).
  [[nodiscard]] std::unique_ptr<ICamera> createFromBackend(
      const std::string& backendId,
      const std::string& deviceId = {}) const;

 private:
  CameraRegistry() = default;

  struct Entry {
    CameraBackendDescriptor desc;
    CameraDiscoverFn discover;
    CameraFactoryFn factory;
  };

  mutable std::mutex mutex_;
  std::vector<Entry> entries_;
};

/// Register Null / Simulated / GenTL stub / UVC-OpenCV backends (idempotent).
void registerAllBuiltInBackends();

}  // namespace beam::camera
