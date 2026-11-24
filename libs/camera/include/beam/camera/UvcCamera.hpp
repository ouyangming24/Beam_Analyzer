#pragma once

#include "beam/camera/ICamera.hpp"

#include <memory>
#include <string>
#include <vector>

namespace beam::camera {

/// USB3 Vision / UVC-style capture path.
///
/// - Default build: stub that reports devices as unavailable (no OpenCV / V4L).
/// - Optional: compile with -DBEAM_ENABLE_OPENCV=ON and OpenCV found →
///   OpenCV VideoCapture backend (UVC webcams / some industrial UVC bridges).
/// - True USB3 Vision (GenICam over USB3) should use the GenTL backend + vendor CTI.
///
/// Windows notes: DirectShow / Media Foundation indexes often appear as OpenCV
/// device indices 0..N. Vendor USB3 Vision still needs the maker's GenTL .cti.

inline constexpr const char* kUvcBackendId = "uvc";

[[nodiscard]] bool isOpenCvCaptureEnabled();

[[nodiscard]] std::vector<CameraInfo> discoverUvcDevices();
[[nodiscard]] std::unique_ptr<ICamera> createUvcCamera(const CameraInfo& device);

}  // namespace beam::camera
