#include "beam/core/MotorScan.hpp"
#include "beam/io/ResultExporter.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0
      << " [--lambda-nm N] [--m2 X] [--m2y Y] [--d0-um D] [--waist-z-mm Z]\n"
      << "       [--coarse-start MM] [--coarse-end MM] [--coarse-step MM]\n"
      << "       [--fine-half-range MM] [--fine-step MM] [--no-fine]\n"
      << "       [--out-prefix PATH] [--noise-um N]\n"
      << "\n"
      << "CLI-usable M² motor scan session (simulated stage + synthetic beam).\n"
      << "Writes: <prefix>_report.md, <prefix>_report.json, <prefix>_dz.csv\n"
      << "Prints ASCII D(z) plot to stdout.\n";
}

bool parseDouble(const char* s, double& out) {
  char* end = nullptr;
  out = std::strtod(s, &end);
  return end != s && end && *end == '\0' && std::isfinite(out);
}

}  // namespace

int main(int argc, char** argv) {
  beam::core::ScanSessionConfig cfg;
  beam::core::SyntheticBeamModel model;
  model.wavelength_nm = 1064.0;
  model.m2X = 1.0;
  model.m2Y = 1.0;
  model.waistDiameterX_um = 100.0;
  model.waistDiameterY_um = 100.0;
  model.waistZ_mm = 0.0;

  cfg.wavelength_nm = 1064.0;
  cfg.coarse = {-40.0, 40.0, 10.0};
  cfg.fine = {8.0, 1.0, true, 1e-3};
  cfg.runFineAfterCoarse = true;

  std::string outPrefix = "m2_scan";
  bool showHelp = false;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](double& dst) -> bool {
      if (i + 1 >= argc) {
        return false;
      }
      return parseDouble(argv[++i], dst);
    };
    if (a == "-h" || a == "--help") {
      showHelp = true;
    } else if (a == "--lambda-nm") {
      if (!need(cfg.wavelength_nm)) {
        showHelp = true;
      }
      model.wavelength_nm = cfg.wavelength_nm;
    } else if (a == "--m2") {
      if (!need(model.m2X)) {
        showHelp = true;
      }
      model.m2Y = model.m2X;
    } else if (a == "--m2y") {
      if (!need(model.m2Y)) {
        showHelp = true;
      }
    } else if (a == "--d0-um") {
      double d0 = 0.0;
      if (!need(d0)) {
        showHelp = true;
      } else {
        model.waistDiameterX_um = d0;
        model.waistDiameterY_um = d0;
      }
    } else if (a == "--waist-z-mm") {
      if (!need(model.waistZ_mm)) {
        showHelp = true;
      }
    } else if (a == "--coarse-start") {
      if (!need(cfg.coarse.zStart_mm)) {
        showHelp = true;
      }
    } else if (a == "--coarse-end") {
      if (!need(cfg.coarse.zEnd_mm)) {
        showHelp = true;
      }
    } else if (a == "--coarse-step") {
      if (!need(cfg.coarse.step_mm)) {
        showHelp = true;
      }
    } else if (a == "--fine-half-range") {
      if (!need(cfg.fine.halfRange_mm)) {
        showHelp = true;
      }
    } else if (a == "--fine-step") {
      if (!need(cfg.fine.step_mm)) {
        showHelp = true;
      }
    } else if (a == "--no-fine") {
      cfg.runFineAfterCoarse = false;
    } else if (a == "--noise-um") {
      if (!need(model.diameterNoise_um)) {
        showHelp = true;
      }
    } else if (a == "--out-prefix") {
      if (i + 1 >= argc) {
        showHelp = true;
      } else {
        outPrefix = argv[++i];
      }
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      showHelp = true;
    }
  }

  if (showHelp) {
    printUsage(argv[0]);
    return showHelp && argc > 1 ? 1 : 0;
  }

  beam::core::SimulatedStage stage(cfg.coarse.zStart_mm);
  auto acquire = beam::core::makeSyntheticAcquire(model);
  const auto session = beam::core::runScanSession(stage, acquire, cfg);

  std::cout << "Stage: " << stage.name() << "\n";
  std::cout << "Points: " << session.points.size()
            << "  valid_fit=" << (session.valid ? "yes" : "no") << "\n";
  if (session.full.fitX.valid) {
    std::cout << "M2_X = " << session.full.fitX.m2 << " ± " << session.full.fitX.m2Uncertainty
              << "  flags=" << beam::core::describeQualityFlags(session.full.fitX.qualityFlags)
              << "\n";
  }
  if (session.full.fitY.valid) {
    std::cout << "M2_Y = " << session.full.fitY.m2 << " ± " << session.full.fitY.m2Uncertainty
              << "  flags=" << beam::core::describeQualityFlags(session.full.fitY.qualityFlags)
              << "\n";
  }
  std::cout << "\n" << beam::core::formatDzAsciiPlot(session.full);

  const std::string mdPath = outPrefix + "_report.md";
  const std::string jsonPath = outPrefix + "_report.json";
  const std::string csvPath = outPrefix + "_dz.csv";

  beam::io::ExportMeta meta;
  meta.note = "beam_m2_scan CLI session";
  if (!beam::io::exportM2ReportMarkdown(mdPath, session.full, meta)) {
    std::cerr << "Failed to write " << mdPath << "\n";
    return 2;
  }
  if (!beam::io::exportM2ReportJson(jsonPath, session.full, meta)) {
    std::cerr << "Failed to write " << jsonPath << "\n";
    return 2;
  }
  {
    std::ofstream csv(csvPath);
    if (!csv) {
      std::cerr << "Failed to write " << csvPath << "\n";
      return 2;
    }
    csv << beam::core::formatDzCurveCsv(session.full);
  }

  std::cout << "Wrote " << mdPath << ", " << jsonPath << ", " << csvPath << "\n";
  return session.valid ? 0 : 3;
}
