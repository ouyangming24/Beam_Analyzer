#include "beam/camera/GenTLCamera.hpp"

#include <utility>

namespace beam::camera {
namespace {

GenTLConsumerConfig gGenTLConfig{};

class GenTLCameraStub final : public ICamera {
 public:
  explicit GenTLCameraStub(CameraInfo info) : info_(std::move(info)) {
    info_.backendId = kGenTLBackendId;
    info_.transport = "gentl";
    info_.available = false;
  }

  CameraInfo info() const override { return info_; }

  bool open() override {
    // Hook point: GenTL_OpenTL / IFOpen / DevOpen / DSOpen
    lastError_ =
        "GenTL consumer stub: no CTI producer loaded. Set ctiPath and link a "
        "real GenTL loader to enable GigE / USB3 Vision devices.";
    open_ = false;
    return false;
  }

  void close() override {
    acquiring_ = false;
    open_ = false;
  }

  bool isOpen() const override { return open_; }

  bool setControls(const CameraControls& controls) override {
    // Hook point: GenApi node map ExposureTime / Gain / TriggerMode / TriggerSource
    controls_ = controls;
    return open_;
  }

  CameraControls controls() const override { return controls_; }

  bool startAcquisition() override {
    // Hook point: DSStartAcquisition + buffer announce/queue
    lastError_ = "GenTL stub cannot start acquisition";
    return false;
  }

  void stopAcquisition() override {
    // Hook point: DSStopAcquisition
    acquiring_ = false;
  }

  bool isAcquiring() const override { return acquiring_; }

  bool softwareTrigger() override {
    // Hook point: TriggerSoftware command feature
    return false;
  }

  CaptureResult grab() override {
    CaptureResult r;
    r.message = lastError_.empty()
                    ? "GenTL stub: grab unavailable without producer"
                    : lastError_;
    return r;
  }

 private:
  CameraInfo info_;
  CameraControls controls_{};
  bool open_ = false;
  bool acquiring_ = false;
  std::string lastError_;
};

}  // namespace

GenTLConsumerConfig& genTLConfig() {
  return gGenTLConfig;
}

bool configureGenTL(const GenTLConsumerConfig& config) {
  gGenTLConfig = config;
  gGenTLConfig.softLoadAttempted = true;
  gGenTLConfig.producerLoaded = false;
  if (gGenTLConfig.ctiPath.empty()) {
    gGenTLConfig.lastError = "ctiPath is empty";
    return false;
  }
  // Soft dependency hook: a future GenTL dynamic loader would dlopen/LoadLibrary
  // the CTI here. Without the library we only record the intended path.
  gGenTLConfig.lastError =
      "GenTL soft-load stub recorded ctiPath=\"" + gGenTLConfig.ctiPath +
      "\" but no GenTL producer library is linked in this build";
  return false;
}

std::vector<CameraInfo> discoverGenTLDevices() {
  std::vector<CameraInfo> devices;
  // Hook point: TLUpdateInterfaceList / IFUpdateDeviceList
  if (gGenTLConfig.ctiPath.empty()) {
    devices.push_back(CameraInfo{
        "gentl:unconfigured",
        "GenTL (no CTI configured)",
        "gentl",
        kGenTLBackendId,
        false});
    return devices;
  }
  devices.push_back(CameraInfo{
      "gentl:stub-device",
      "GenTL stub device (" + gGenTLConfig.ctiPath + ")",
      "gentl",
      kGenTLBackendId,
      false});
  return devices;
}

std::unique_ptr<ICamera> createGenTLCamera(const CameraInfo& device) {
  return std::make_unique<GenTLCameraStub>(device);
}

}  // namespace beam::camera
