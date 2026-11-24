#include "beam/camera/ICamera.hpp"

namespace beam::camera {
namespace {

class NullCamera final : public ICamera {
 public:
  CameraInfo info() const override {
    return {"null", "Null Camera", "null", "null", true};
  }

  bool open() override {
    open_ = true;
    return true;
  }

  void close() override {
    acquiring_ = false;
    open_ = false;
  }

  bool isOpen() const override { return open_; }

  bool setControls(const CameraControls& controls) override {
    controls_ = controls;
    return true;
  }

  CameraControls controls() const override { return controls_; }

  bool startAcquisition() override {
    if (!open_) {
      return false;
    }
    acquiring_ = true;
    return true;
  }

  void stopAcquisition() override { acquiring_ = false; }

  bool isAcquiring() const override { return acquiring_; }

  bool softwareTrigger() override { return open_ && acquiring_; }

  CaptureResult grab() override {
    CaptureResult r;
    if (!open_) {
      r.message = "NullCamera is not open";
      return r;
    }
    r.message = "NullCamera produces no frames (hardware port placeholder)";
    return r;
  }

 private:
  bool open_ = false;
  bool acquiring_ = false;
  CameraControls controls_{};
};

}  // namespace

std::unique_ptr<ICamera> createNullCamera() {
  return std::make_unique<NullCamera>();
}

}  // namespace beam::camera
