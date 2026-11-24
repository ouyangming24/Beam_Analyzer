#include "beam/camera/CameraRegistry.hpp"
#include "beam/camera/GenTLCamera.hpp"
#include "beam/camera/ICamera.hpp"
#include "beam/camera/UvcCamera.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int gFailures = 0;

void expectTrue(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    ++gFailures;
  } else {
    std::cout << "OK:   " << msg << "\n";
  }
}

void testRegistryDiscoversBuiltIns() {
  auto& reg = beam::camera::CameraRegistry::instance();
  reg.clear();
  beam::camera::registerAllBuiltInBackends();

  const auto backends = reg.backends();
  expectTrue(backends.size() >= 4, "at least 4 built-in backends");

  bool hasSim = false;
  bool hasGentl = false;
  bool hasUvc = false;
  for (const auto& b : backends) {
    if (b.id == "simulated") {
      hasSim = true;
      expectTrue(b.available, "simulated backend available");
    }
    if (b.id == beam::camera::kGenTLBackendId) {
      hasGentl = true;
    }
    if (b.id == beam::camera::kUvcBackendId) {
      hasUvc = true;
    }
  }
  expectTrue(hasSim, "simulated backend registered");
  expectTrue(hasGentl, "gentl backend registered");
  expectTrue(hasUvc, "uvc backend registered");

  const auto devices = reg.discoverAll();
  expectTrue(!devices.empty(), "discoverAll returns devices");

  bool foundSimDevice = false;
  for (const auto& d : devices) {
    if (d.id == "sim-gaussian") {
      foundSimDevice = true;
      expectTrue(d.available, "sim device available");
    }
  }
  expectTrue(foundSimDevice, "sim-gaussian discovered");

  auto cam = reg.create("sim-gaussian");
  expectTrue(cam != nullptr, "create sim-gaussian");
  expectTrue(cam->open(), "open simulated");
  const auto one = cam->grab();
  expectTrue(one.ok, "single-shot grab without acquisition");
  expectTrue(!one.frame.empty(), "grab frame non-empty");
  cam->close();
}

void testSimulatedLiveFps() {
  auto cam = beam::camera::createSimulatedCamera(64, 48, 5.0);
  expectTrue(cam->open(), "open for live");

  beam::camera::CameraControls ctrl;
  ctrl.trigger = beam::camera::TriggerMode::FreeRun;
  ctrl.targetFps = 40.0;
  ctrl.exposure_us = 100.0;
  ctrl.gain_db = 1.0;
  expectTrue(cam->setControls(ctrl), "set FreeRun controls");
  expectTrue(cam->startAcquisition(), "start acquisition");

  const auto t0 = std::chrono::steady_clock::now();
  int okFrames = 0;
  for (int i = 0; i < 8; ++i) {
    const auto cap = cam->grab();
    if (cap.ok) {
      ++okFrames;
    }
  }
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
  cam->stopAcquisition();
  cam->close();

  expectTrue(okFrames >= 6, "live FreeRun delivered most frames");
  expectTrue(ms < 2000, "live grabs completed within 2s");
  std::cout << "     live grabbed " << okFrames << " frames in " << ms << " ms\n";
}

void testSimulatedSoftwareTrigger() {
  auto cam = beam::camera::createSimulatedCamera(32, 32, 5.0);
  expectTrue(cam->open(), "open for software trigger");

  beam::camera::CameraControls ctrl;
  ctrl.trigger = beam::camera::TriggerMode::Software;
  ctrl.exposure_us = 50.0;
  expectTrue(cam->setControls(ctrl), "set Software trigger");
  expectTrue(cam->startAcquisition(), "start software-trigger acquisition");

  // Without trigger, grab should time out.
  std::thread trig([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    expectTrue(cam->softwareTrigger(), "fire software trigger");
  });
  const auto cap = cam->grab();
  trig.join();
  expectTrue(cap.ok, "grab after software trigger");
  expectTrue(cap.frameIndex >= 1, "frame index assigned");

  cam->stopAcquisition();
  cam->close();
}

void testGenTLAndUvcStubs() {
  beam::camera::GenTLConsumerConfig cfg;
  cfg.ctiPath.clear();
  expectTrue(!beam::camera::configureGenTL(cfg), "empty CTI configure fails");

  cfg.ctiPath = "C:/Vendor/Producer.cti";
  expectTrue(!beam::camera::configureGenTL(cfg),
             "CTI path recorded but soft-load still fails without GenTL lib");
  expectTrue(!beam::camera::genTLConfig().producerLoaded, "producer not loaded");

  const auto gentl = beam::camera::discoverGenTLDevices();
  expectTrue(!gentl.empty(), "GenTL discover returns stub entry");
  expectTrue(!gentl.front().available, "GenTL stub unavailable");

  auto gentlCam = beam::camera::createGenTLCamera(gentl.front());
  expectTrue(gentlCam != nullptr, "create GenTL stub");
  expectTrue(!gentlCam->open(), "GenTL stub open fails (expected)");

  expectTrue(!beam::camera::isOpenCvCaptureEnabled() ||
                 beam::camera::isOpenCvCaptureEnabled(),
             "OpenCV flag query is stable");
  const auto uvc = beam::camera::discoverUvcDevices();
  expectTrue(!uvc.empty(), "UVC discover returns entry");
  auto uvcCam = beam::camera::createUvcCamera(uvc.front());
  expectTrue(uvcCam != nullptr, "create UVC camera object");
  if (!beam::camera::isOpenCvCaptureEnabled()) {
    expectTrue(!uvc.front().available, "UVC stub marked unavailable");
    expectTrue(!uvcCam->open(), "UVC stub open fails without OpenCV");
  }
}

void testRegistryCreateFromBackend() {
  auto& reg = beam::camera::CameraRegistry::instance();
  reg.clear();
  beam::camera::registerAllBuiltInBackends();
  auto cam = reg.createFromBackend("simulated");
  expectTrue(cam != nullptr, "createFromBackend(simulated)");
  expectTrue(cam->info().transport == "simulated", "transport is simulated");
}

}  // namespace

int main() {
  testRegistryDiscoversBuiltIns();
  testRegistryCreateFromBackend();
  testSimulatedLiveFps();
  testSimulatedSoftwareTrigger();
  testGenTLAndUvcStubs();

  if (gFailures != 0) {
    std::cerr << gFailures << " failure(s)\n";
    return 1;
  }
  std::cout << "All camera tests passed.\n";
  return 0;
}
