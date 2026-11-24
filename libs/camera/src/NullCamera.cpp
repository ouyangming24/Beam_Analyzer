#include "beam/camera/ICamera.hpp"

namespace beam::camera {
namespace {

class NullCamera final : public ICamera {
 public:
  CameraInfo info() const override {
    return {"null", "Null Camera", "null"};
  }

  bool open() override {
    open_ = true;
    return true;
  }

  void close() override { open_ = false; }

  bool isOpen() const override { return open_; }

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
};

}  // namespace

std::unique_ptr<ICamera> createNullCamera() {
  return std::make_unique<NullCamera>();
}

}  // namespace beam::camera
