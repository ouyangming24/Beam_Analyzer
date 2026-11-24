#include "beam/camera/CameraRegistry.hpp"

#include "beam/camera/GenTLCamera.hpp"
#include "beam/camera/UvcCamera.hpp"

#include <algorithm>

namespace beam::camera {
namespace {

bool gBuiltInsRegistered = false;

}  // namespace

CameraRegistry& CameraRegistry::instance() {
  static CameraRegistry registry;
  return registry;
}

void CameraRegistry::clear() {
  std::lock_guard lock(mutex_);
  entries_.clear();
  gBuiltInsRegistered = false;
}

void CameraRegistry::registerBackend(CameraBackendDescriptor desc,
                                     CameraDiscoverFn discover,
                                     CameraFactoryFn factory) {
  std::lock_guard lock(mutex_);
  const auto it = std::find_if(entries_.begin(), entries_.end(),
                               [&](const Entry& e) { return e.desc.id == desc.id; });
  if (it != entries_.end()) {
    it->desc = std::move(desc);
    it->discover = std::move(discover);
    it->factory = std::move(factory);
    return;
  }
  entries_.push_back(Entry{std::move(desc), std::move(discover), std::move(factory)});
}

std::vector<CameraBackendDescriptor> CameraRegistry::backends() const {
  std::lock_guard lock(mutex_);
  std::vector<CameraBackendDescriptor> out;
  out.reserve(entries_.size());
  for (const auto& e : entries_) {
    out.push_back(e.desc);
  }
  return out;
}

std::optional<CameraBackendDescriptor> CameraRegistry::findBackend(
    const std::string& backendId) const {
  std::lock_guard lock(mutex_);
  for (const auto& e : entries_) {
    if (e.desc.id == backendId) {
      return e.desc;
    }
  }
  return std::nullopt;
}

std::vector<CameraInfo> CameraRegistry::discoverAll() const {
  std::lock_guard lock(mutex_);
  std::vector<CameraInfo> out;
  for (const auto& e : entries_) {
    if (!e.discover) {
      continue;
    }
    auto devices = e.discover();
    for (auto& d : devices) {
      if (d.backendId.empty()) {
        d.backendId = e.desc.id;
      }
      if (d.transport.empty()) {
        d.transport = e.desc.transport;
      }
      out.push_back(std::move(d));
    }
  }
  return out;
}

std::unique_ptr<ICamera> CameraRegistry::create(const std::string& deviceId) const {
  std::lock_guard lock(mutex_);
  for (const auto& e : entries_) {
    if (!e.discover || !e.factory) {
      continue;
    }
    for (const auto& d : e.discover()) {
      if (d.id == deviceId) {
        CameraInfo info = d;
        if (info.backendId.empty()) {
          info.backendId = e.desc.id;
        }
        return e.factory(info);
      }
    }
  }
  return nullptr;
}

std::unique_ptr<ICamera> CameraRegistry::createFromBackend(
    const std::string& backendId,
    const std::string& deviceId) const {
  std::lock_guard lock(mutex_);
  for (const auto& e : entries_) {
    if (e.desc.id != backendId || !e.factory) {
      continue;
    }
    CameraInfo info;
    info.id = deviceId.empty() ? backendId : deviceId;
    info.name = e.desc.name;
    info.transport = e.desc.transport;
    info.backendId = e.desc.id;
    info.available = e.desc.available;
    if (e.discover) {
      const auto devices = e.discover();
      if (!devices.empty()) {
        if (!deviceId.empty()) {
          for (const auto& d : devices) {
            if (d.id == deviceId) {
              info = d;
              break;
            }
          }
        } else {
          info = devices.front();
        }
      }
    }
    return e.factory(info);
  }
  return nullptr;
}

void registerAllBuiltInBackends() {
  if (gBuiltInsRegistered) {
    return;
  }
  gBuiltInsRegistered = true;
  auto& reg = CameraRegistry::instance();

  reg.registerBackend(
      CameraBackendDescriptor{
          "null",
          "Null Camera",
          "null",
          true,
          "Placeholder that never produces frames."},
      []() {
        return std::vector<CameraInfo>{
            {"null", "Null Camera", "null", "null", true}};
      },
      [](const CameraInfo&) { return createNullCamera(); });

  reg.registerBackend(
      CameraBackendDescriptor{
          "simulated",
          "Simulated Gaussian",
          "simulated",
          true,
          "Software Gaussian beam with live FPS / trigger simulation."},
      []() {
        return std::vector<CameraInfo>{
            {"sim-gaussian", "Simulated Gaussian Camera", "simulated", "simulated",
             true}};
      },
      [](const CameraInfo&) { return createSimulatedCamera(); });

  reg.registerBackend(
      CameraBackendDescriptor{
          kGenTLBackendId,
          "GenICam / GigE Vision (GenTL)",
          "gentl",
          !genTLConfig().ctiPath.empty(),
          "Stub GenTL consumer. Point GenTLConsumerConfig::ctiPath at a vendor "
          ".cti and replace the loader hooks with a real GenTL consumer."},
      discoverGenTLDevices,
      createGenTLCamera);

  reg.registerBackend(
      CameraBackendDescriptor{
          kUvcBackendId,
          isOpenCvCaptureEnabled() ? "UVC / OpenCV VideoCapture"
                                   : "UVC / USB capture (stub)",
          isOpenCvCaptureEnabled() ? "opencv" : "uvc",
          isOpenCvCaptureEnabled(),
          "Enable with -DBEAM_ENABLE_OPENCV=ON. USB3 Vision GenICam devices "
          "should use the GenTL backend instead."},
      discoverUvcDevices,
      createUvcCamera);
}

}  // namespace beam::camera
