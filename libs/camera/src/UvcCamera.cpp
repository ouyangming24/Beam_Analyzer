#include "beam/camera/UvcCamera.hpp"

#if defined(BEAM_ENABLE_OPENCV) && BEAM_ENABLE_OPENCV
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#endif

#include <sstream>

namespace beam::camera {
namespace {

#if defined(BEAM_ENABLE_OPENCV) && BEAM_ENABLE_OPENCV

class OpenCvUvcCamera final : public ICamera {
 public:
  explicit OpenCvUvcCamera(CameraInfo info) : info_(std::move(info)) {
    info_.backendId = kUvcBackendId;
    info_.transport = "opencv";
    info_.available = true;
  }

  ~OpenCvUvcCamera() override { close(); }

  CameraInfo info() const override { return info_; }

  bool open() override {
    const int index = parseIndex(info_.id);
    if (!cap_.open(index)) {
      lastError_ = "OpenCV VideoCapture failed to open device index " +
                   std::to_string(index);
      open_ = false;
      return false;
    }
    open_ = true;
    applyControlsUnlocked();
    return true;
  }

  void close() override {
    acquiring_ = false;
    if (cap_.isOpened()) {
      cap_.release();
    }
    open_ = false;
  }

  bool isOpen() const override { return open_ && cap_.isOpened(); }

  bool setControls(const CameraControls& controls) override {
    controls_ = controls;
    if (isOpen()) {
      applyControlsUnlocked();
    }
    return true;
  }

  CameraControls controls() const override { return controls_; }

  bool startAcquisition() override {
    if (!isOpen()) {
      return false;
    }
    acquiring_ = true;
    return true;
  }

  void stopAcquisition() override { acquiring_ = false; }

  bool isAcquiring() const override { return acquiring_; }

  bool softwareTrigger() override {
    // UVC typically free-runs; software trigger is a no-op success when open.
    return isOpen() && controls_.trigger == TriggerMode::Software;
  }

  CaptureResult grab() override {
    CaptureResult r;
    if (!isOpen()) {
      r.message = lastError_.empty() ? "UVC/OpenCV camera is not open" : lastError_;
      return r;
    }
    if (controls_.trigger == TriggerMode::Software && !acquiring_) {
      r.message = "Software trigger mode: start acquisition then softwareTrigger";
      return r;
    }

    cv::Mat bgr;
    if (!cap_.read(bgr) || bgr.empty()) {
      r.message = "OpenCV VideoCapture read failed";
      return r;
    }
    cv::Mat gray;
    if (bgr.channels() == 1) {
      gray = bgr;
    } else {
      cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    }

    r.frame.resize(static_cast<std::size_t>(gray.cols),
                   static_cast<std::size_t>(gray.rows));
    r.frame.pixelSizeX_um = 5.0;
    r.frame.pixelSizeY_um = 5.0;
    for (int y = 0; y < gray.rows; ++y) {
      for (int x = 0; x < gray.cols; ++x) {
        r.frame.set(static_cast<std::size_t>(x), static_cast<std::size_t>(y),
                    static_cast<double>(gray.at<unsigned char>(y, x)));
      }
    }
    r.ok = true;
    r.message = "OK";
    r.timestamp = std::chrono::steady_clock::now();
    r.frameIndex = ++frameIndex_;
    return r;
  }

 private:
  static int parseIndex(const std::string& id) {
    // Expect "uvc:0"
    const auto pos = id.find_last_of(':');
    if (pos == std::string::npos) {
      return 0;
    }
    try {
      return std::stoi(id.substr(pos + 1));
    } catch (...) {
      return 0;
    }
  }

  void applyControlsUnlocked() {
    if (!cap_.isOpened()) {
      return;
    }
    // Best-effort UVC property mapping (driver-dependent).
    cap_.set(cv::CAP_PROP_EXPOSURE, controls_.exposure_us / 1000.0);
    cap_.set(cv::CAP_PROP_GAIN, controls_.gain_db);
    if (controls_.targetFps > 0.0) {
      cap_.set(cv::CAP_PROP_FPS, controls_.targetFps);
    }
  }

  CameraInfo info_;
  CameraControls controls_{};
  cv::VideoCapture cap_;
  bool open_ = false;
  bool acquiring_ = false;
  std::uint64_t frameIndex_ = 0;
  std::string lastError_;
};

#endif  // BEAM_ENABLE_OPENCV

class UvcCameraStub final : public ICamera {
 public:
  explicit UvcCameraStub(CameraInfo info) : info_(std::move(info)) {
    info_.backendId = kUvcBackendId;
    if (info_.transport.empty()) {
      info_.transport = "uvc";
    }
    info_.available = false;
  }

  CameraInfo info() const override { return info_; }

  bool open() override {
    lastError_ =
        "UVC stub: rebuild with -DBEAM_ENABLE_OPENCV=ON and OpenCV installed "
        "for VideoCapture, or use GenTL for USB3 Vision GenICam devices.";
    return false;
  }

  void close() override { open_ = false; }
  bool isOpen() const override { return open_; }

  bool setControls(const CameraControls& controls) override {
    controls_ = controls;
    return false;
  }

  CameraControls controls() const override { return controls_; }

  bool startAcquisition() override { return false; }
  void stopAcquisition() override {}
  bool isAcquiring() const override { return false; }
  bool softwareTrigger() override { return false; }

  CaptureResult grab() override {
    CaptureResult r;
    r.message = lastError_.empty() ? "UVC stub unavailable" : lastError_;
    return r;
  }

 private:
  CameraInfo info_;
  CameraControls controls_{};
  bool open_ = false;
  std::string lastError_;
};

}  // namespace

bool isOpenCvCaptureEnabled() {
#if defined(BEAM_ENABLE_OPENCV) && BEAM_ENABLE_OPENCV
  return true;
#else
  return false;
#endif
}

std::vector<CameraInfo> discoverUvcDevices() {
  std::vector<CameraInfo> devices;
#if defined(BEAM_ENABLE_OPENCV) && BEAM_ENABLE_OPENCV
  // Probe a few indices; avoid hanging CI by limiting attempts.
  for (int i = 0; i < 4; ++i) {
    cv::VideoCapture cap;
    if (!cap.open(i)) {
      continue;
    }
    cap.release();
    std::ostringstream id;
    id << "uvc:" << i;
    devices.push_back(CameraInfo{id.str(), "OpenCV / UVC device " + std::to_string(i),
                                 "opencv", kUvcBackendId, true});
  }
  if (devices.empty()) {
    devices.push_back(CameraInfo{"uvc:none", "OpenCV enabled — no device found",
                                 "opencv", kUvcBackendId, false});
  }
#else
  devices.push_back(CameraInfo{
      "uvc:stub",
      "UVC / OpenCV (disabled — stub)",
      "uvc",
      kUvcBackendId,
      false});
#endif
  return devices;
}

std::unique_ptr<ICamera> createUvcCamera(const CameraInfo& device) {
#if defined(BEAM_ENABLE_OPENCV) && BEAM_ENABLE_OPENCV
  if (device.available) {
    return std::make_unique<OpenCvUvcCamera>(device);
  }
#endif
  return std::make_unique<UvcCameraStub>(device);
}

}  // namespace beam::camera
