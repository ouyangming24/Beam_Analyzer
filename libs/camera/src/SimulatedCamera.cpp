#include "beam/camera/ICamera.hpp"

#include "beam/core/Iso11146.hpp"

#include <cmath>
#include <random>

namespace beam::camera {
namespace {

class SimulatedCamera final : public ICamera {
 public:
  SimulatedCamera(std::size_t width, std::size_t height, double pixelSize_um)
      : width_(width), height_(height), pixelSize_um_(pixelSize_um) {}

  CameraInfo info() const override {
    return {"sim-gaussian", "Simulated Gaussian Camera", "simulated"};
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
      r.message = "SimulatedCamera is not open";
      return r;
    }

    std::uniform_real_distribution<double> jitter(-0.5, 0.5);
    const double cx = 0.5 * static_cast<double>(width_) + jitter(rng_);
    const double cy = 0.5 * static_cast<double>(height_) + jitter(rng_);
    const double sx = 0.12 * static_cast<double>(width_);
    const double sy = 0.10 * static_cast<double>(height_);

    r.frame = beam::core::makeGaussianFrame(
        width_, height_, cx, cy, sx, sy, 1.0, pixelSize_um_, pixelSize_um_, 0.01);
    r.ok = true;
    r.message = "OK";
    return r;
  }

 private:
  std::size_t width_;
  std::size_t height_;
  double pixelSize_um_;
  bool open_ = false;
  std::mt19937 rng_{42};
};

}  // namespace

std::unique_ptr<ICamera> createSimulatedCamera(std::size_t width,
                                               std::size_t height,
                                               double pixelSize_um) {
  return std::make_unique<SimulatedCamera>(width, height, pixelSize_um);
}

}  // namespace beam::camera
