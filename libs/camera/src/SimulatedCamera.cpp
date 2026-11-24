#include "beam/camera/ICamera.hpp"

#include "beam/core/Iso11146.hpp"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace beam::camera {
namespace {

class SimulatedCamera final : public ICamera {
 public:
  SimulatedCamera(std::size_t width, std::size_t height, double pixelSize_um)
      : width_(width), height_(height), pixelSize_um_(pixelSize_um) {}

  ~SimulatedCamera() override {
    stopAcquisition();
    close();
  }

  CameraInfo info() const override {
    return {"sim-gaussian", "Simulated Gaussian Camera", "simulated", "simulated",
            true};
  }

  bool open() override {
    std::lock_guard lock(mutex_);
    open_ = true;
    return true;
  }

  void close() override {
    stopAcquisition();
    std::lock_guard lock(mutex_);
    open_ = false;
    pendingTrigger_ = false;
  }

  bool isOpen() const override {
    std::lock_guard lock(mutex_);
    return open_;
  }

  bool setControls(const CameraControls& controls) override {
    std::lock_guard lock(mutex_);
    controls_ = controls;
    controls_.exposure_us = std::max(1.0, controls_.exposure_us);
    controls_.targetFps = std::clamp(controls_.targetFps, 0.1, 240.0);
    cv_.notify_all();
    return true;
  }

  CameraControls controls() const override {
    std::lock_guard lock(mutex_);
    return controls_;
  }

  bool startAcquisition() override {
    std::lock_guard lock(mutex_);
    if (!open_) {
      return false;
    }
    if (acquiring_) {
      return true;
    }
    acquiring_ = true;
    stopWorker_ = false;
    worker_ = std::thread([this] { workerLoop(); });
    return true;
  }

  void stopAcquisition() override {
    {
      std::lock_guard lock(mutex_);
      if (!acquiring_) {
        return;
      }
      stopWorker_ = true;
      acquiring_ = false;
      pendingTrigger_ = false;
      cv_.notify_all();
    }
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  bool isAcquiring() const override {
    std::lock_guard lock(mutex_);
    return acquiring_;
  }

  bool softwareTrigger() override {
    std::lock_guard lock(mutex_);
    if (!open_ || !acquiring_) {
      return false;
    }
    if (controls_.trigger != TriggerMode::Software) {
      return false;
    }
    pendingTrigger_ = true;
    cv_.notify_all();
    return true;
  }

  CaptureResult grab() override {
    std::unique_lock lock(mutex_);
    if (!open_) {
      CaptureResult r;
      r.message = "SimulatedCamera is not open";
      return r;
    }

    // Live path: wait for a produced frame (or synthesize one-shot if idle).
    if (acquiring_) {
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
      while (!latest_.ok && !stopWorker_) {
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
          CaptureResult r;
          r.message = "SimulatedCamera grab timeout (check trigger mode)";
          return r;
        }
      }
      CaptureResult out = latest_;
      latest_ = CaptureResult{};
      return out;
    }

    return synthesizeLocked();
  }

 private:
  CaptureResult synthesizeLocked() {
    CaptureResult r;
    std::uniform_real_distribution<double> jitter(-0.5, 0.5);
    const double cx = 0.5 * static_cast<double>(width_) + jitter(rng_);
    const double cy = 0.5 * static_cast<double>(height_) + jitter(rng_);
    const double sx = 0.12 * static_cast<double>(width_);
    const double sy = 0.10 * static_cast<double>(height_);

    // Exposure/gain placeholders modulate amplitude (not physically calibrated).
    const double amp =
        1.0 * (1.0 + 0.05 * controls_.gain_db) *
        std::clamp(controls_.exposure_us / 1000.0, 0.05, 10.0);

    r.frame = beam::core::makeGaussianFrame(
        width_, height_, cx, cy, sx, sy, amp, pixelSize_um_, pixelSize_um_, 0.01);
    r.ok = true;
    r.message = "OK";
    r.timestamp = std::chrono::steady_clock::now();
    r.frameIndex = ++frameIndex_;
    return r;
  }

  void workerLoop() {
    using clock = std::chrono::steady_clock;
    auto nextFreeRun = clock::now();
    for (;;) {
      CameraControls ctrl;
      {
        std::unique_lock lock(mutex_);
        if (stopWorker_ || !acquiring_) {
          return;
        }
        ctrl = controls_;

        if (ctrl.trigger == TriggerMode::FreeRun) {
          const auto period = std::chrono::duration_cast<clock::duration>(
              std::chrono::duration<double>(1.0 / std::max(0.1, ctrl.targetFps)));
          if (cv_.wait_until(lock, nextFreeRun, [this] {
                return stopWorker_ || !acquiring_ ||
                       controls_.trigger != TriggerMode::FreeRun;
              })) {
            if (stopWorker_ || !acquiring_) {
              return;
            }
            continue;
          }
          nextFreeRun = clock::now() + period;
        } else if (ctrl.trigger == TriggerMode::Software) {
          cv_.wait(lock, [this] {
            return stopWorker_ || !acquiring_ || pendingTrigger_ ||
                   controls_.trigger != TriggerMode::Software;
          });
          if (stopWorker_ || !acquiring_) {
            return;
          }
          if (controls_.trigger != TriggerMode::Software) {
            continue;
          }
          if (!pendingTrigger_) {
            continue;
          }
          pendingTrigger_ = false;
        } else {
          // Hardware trigger stub: wait forever until mode changes / stop.
          cv_.wait(lock, [this] {
            return stopWorker_ || !acquiring_ ||
                   controls_.trigger != TriggerMode::Hardware;
          });
          if (stopWorker_ || !acquiring_) {
            return;
          }
          continue;
        }

        latest_ = synthesizeLocked();
        cv_.notify_all();
      }

      // Approximate exposure dwell outside the lock (placeholder).
      const auto dwell = std::chrono::duration_cast<clock::duration>(
          std::chrono::duration<double, std::micro>(
              std::min(ctrl.exposure_us, 5000.0)));
      if (dwell.count() > 0) {
        std::this_thread::sleep_for(dwell);
      }
    }
  }

  std::size_t width_;
  std::size_t height_;
  double pixelSize_um_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool open_ = false;
  bool acquiring_ = false;
  bool stopWorker_ = false;
  bool pendingTrigger_ = false;
  CameraControls controls_{};
  CaptureResult latest_{};
  std::uint64_t frameIndex_ = 0;
  std::mt19937 rng_{42};
  std::thread worker_;
};

}  // namespace

std::unique_ptr<ICamera> createSimulatedCamera(std::size_t width,
                                               std::size_t height,
                                               double pixelSize_um) {
  return std::make_unique<SimulatedCamera>(width, height, pixelSize_um);
}

}  // namespace beam::camera
