#pragma once

#include "beam/camera/ICamera.hpp"

#include <memory>
#include <string>
#include <vector>

namespace beam::camera {

/// GenICam / GigE Vision style path — GenTL consumer interface stub.
///
/// Design notes (no proprietary SDK required at build time):
/// - Market cameras often expose a GenTL `.cti` producer (GigE Vision, USB3 Vision).
/// - A full consumer would: enumerate CTI modules → open IF / DEV / DS →
///   map GenICam features (ExposureTime, Gain, TriggerMode) → queue buffers.
/// - This stub documents those hooks and returns unavailable until a CTI path
///   is configured via GenTLConsumerConfig.
///
/// Optional soft dependency: set BEAM_GENTL_CTI_PATH or call configureGenTL().
struct GenTLConsumerConfig {
  std::string ctiPath;           ///< Path to vendor .cti (empty = unavailable)
  std::string preferredIface;    ///< Optional IF filter
  bool softLoadAttempted = false;
  bool producerLoaded = false;   ///< Always false without real GenTL loader
  std::string lastError;
};

/// Global stub config for the GenTL backend (process-wide).
[[nodiscard]] GenTLConsumerConfig& genTLConfig();

/// Documented hook: attempt to "load" a GenTL producer. Without a real GenTL
/// library this only validates the path string and marks producerLoaded=false.
[[nodiscard]] bool configureGenTL(const GenTLConsumerConfig& config);

[[nodiscard]] std::vector<CameraInfo> discoverGenTLDevices();
[[nodiscard]] std::unique_ptr<ICamera> createGenTLCamera(const CameraInfo& device);

/// Backend id constant.
inline constexpr const char* kGenTLBackendId = "gentl";

}  // namespace beam::camera
