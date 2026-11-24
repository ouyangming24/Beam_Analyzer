#include "beam/core/Iso11146.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/core/M2Scan.hpp"
#include "beam/core/MomentOptions.hpp"
#include "beam/core/MotorScan.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Profile.hpp"
#include "beam/core/Roi.hpp"
#include "beam/io/CsvImporter.hpp"
#include "beam/io/ResultExporter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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

void expectNear(double actual, double expected, double tol, const char* msg) {
  const bool ok = std::isfinite(actual) && std::abs(actual - expected) <= tol;
  if (!ok) {
    std::cerr << "FAIL: " << msg << " actual=" << actual << " expected=" << expected
              << " tol=" << tol << "\n";
    ++gFailures;
  } else {
    std::cout << "OK:   " << msg << " (" << actual << ")\n";
  }
}

}  // namespace

int main() {
  using beam::core::computeIso11146;
  using beam::core::makeGaussianFrame;

  // Circular Gaussian: σ=10 px, pixel=5 µm → dσ = 4σ = 40 px = 200 µm
  {
    constexpr double pixel = 5.0;
    constexpr double sigmaPx = 10.0;
    auto frame = makeGaussianFrame(128, 128, 64.0, 64.0, sigmaPx, sigmaPx, 1.0, pixel, pixel);
    const auto m = computeIso11146(frame);
    expectTrue(m.valid, "circular gaussian metrics valid");
    expectNear(m.centroidX_um, 64.0 * pixel, 1.0, "centroid X");
    expectNear(m.centroidY_um, 64.0 * pixel, 1.0, "centroid Y");
    expectNear(m.diameterX_um, 4.0 * sigmaPx * pixel, 4.0, "diameter X ≈ 4σ");
    expectNear(m.diameterY_um, 4.0 * sigmaPx * pixel, 4.0, "diameter Y ≈ 4σ");
    expectNear(m.ellipticity, 1.0, 0.02, "ellipticity ≈ 1");
  }

  // Elliptical Gaussian
  {
    constexpr double pixel = 1.0;
    auto frame = makeGaussianFrame(200, 200, 100.0, 100.0, 20.0, 10.0, 1.0, pixel, pixel);
    const auto m = computeIso11146(frame);
    expectTrue(m.valid, "elliptical gaussian metrics valid");
    expectNear(m.diameterX_um, 80.0, 3.0, "elliptical Dx");
    expectNear(m.diameterY_um, 40.0, 3.0, "elliptical Dy");
    expectNear(m.ellipticity, 0.5, 0.05, "ellipticity ≈ 0.5");
  }

  // M² interface: synthetic TEM00-like hyperbola with M²=1
  {
    const double lambda_nm = 1064.0;
    const double d0_um = 100.0;
    const double w0_um = d0_um * 0.5;
    const double zR_um = 3.14159265358979323846 * w0_um * w0_um / (lambda_nm * 1e-3);  // µm
    const double zR_mm = zR_um / 1000.0;

    std::vector<beam::core::M2Sample> samples;
    for (double z : {-2.0 * zR_mm, -zR_mm, 0.0, zR_mm, 2.0 * zR_mm}) {
      const double d = d0_um * std::sqrt(1.0 + (z / zR_mm) * (z / zR_mm));
      samples.push_back({z, d});
    }
    const auto fit = beam::core::fitM2(samples, lambda_nm);
    expectTrue(fit.valid, "M2 fit valid");
    expectNear(fit.m2, 1.0, 0.15, "M2 ≈ 1 for ideal TEM00");
    expectNear(fit.waistDiameter_um, d0_um, 5.0, "waist diameter");
  }

  // CSV round-trip of a tiny frame
  {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "beam_analyzer_test";
    fs::create_directories(dir);
    const fs::path csvPath = dir / "tiny.csv";
    {
      std::ofstream out(csvPath);
      out << "# pixel_um=2.5\n";
      out << "0,0,0\n";
      out << "0,1,0\n";
      out << "0,0,0\n";
    }
    const auto loaded = beam::io::loadFrameCsv(csvPath.string());
    expectTrue(loaded.ok, "CSV import ok");
    expectTrue(loaded.frame.width == 3 && loaded.frame.height == 3, "CSV shape 3x3");
    expectNear(loaded.frame.pixelSizeX_um, 2.5, 1e-9, "CSV meta pixel size");
    expectNear(loaded.frame.at(1, 1), 1.0, 1e-12, "CSV peak value");

    const auto metrics = computeIso11146(loaded.frame);
    const fs::path jsonPath = dir / "out.json";
    const fs::path metricsCsv = dir / "out.csv";
    expectTrue(beam::io::exportMetricsJson(jsonPath.string(), metrics, {"tiny.csv", "test"}),
               "export JSON");
    expectTrue(beam::io::exportMetricsCsv(metricsCsv.string(), metrics, {"tiny.csv", "test"}),
               "export CSV");
  }

  // --- Phase 2: background subtraction ---
  {
    auto frame = makeGaussianFrame(64, 64, 32.0, 32.0, 6.0, 6.0, 10.0, 1.0, 1.0, 2.0);
    const double bg = beam::core::estimateBackground(
        frame, {beam::core::BackgroundMode::Constant, 2.0});
    expectNear(bg, 2.0, 1e-12, "constant background estimate");
    auto cleaned = beam::core::subtractBackground(frame, 2.0, true);
    expectNear(cleaned.at(0, 0), 0.0, 1e-6, "corner near zero after bg subtract");
    const auto m = computeIso11146(cleaned);
    expectTrue(m.valid, "metrics after bg subtract");
  }

  // --- Phase 2: ROI crop ---
  {
    auto frame = makeGaussianFrame(100, 100, 50.0, 50.0, 8.0, 8.0, 1.0, 1.0, 1.0);
    beam::core::RectRoi roi{30, 30, 70, 70};
    auto cropped = beam::core::cropToRoi(frame, roi);
    expectTrue(cropped.width == 41 && cropped.height == 41, "ROI crop size 41x41");
    beam::core::AnalysisPrep prep;
    prep.useRoi = true;
    prep.roi = roi;
    prep.background.mode = beam::core::BackgroundMode::None;
    auto prepared = beam::core::prepareAnalysisFrame(frame, prep);
    expectTrue(prepared.width == 41, "prepareAnalysisFrame crops");
    const auto m = computeIso11146(prepared);
    expectTrue(m.valid, "ROI metrics valid");
    expectNear(m.centroidX_um, 20.5, 2.0, "ROI-local centroid near center");
  }

  // --- Phase 2: power calibration hooks ---
  {
    auto frame = makeGaussianFrame(32, 32, 16.0, 16.0, 4.0, 4.0, 1.0, 1.0, 1.0);
    beam::core::PowerCalibration cal;
    cal.countsToWatts = 0.0;
    auto uncal = beam::core::computePowerMetrics(frame, cal);
    expectTrue(uncal.valid && !uncal.calibrated, "uncalibrated power");
    expectTrue(uncal.sumCounts > 0.0, "sum counts positive");

    cal.countsToWatts = 2.0;
    cal.exposure_s = 0.5;
    auto calp = beam::core::computePowerMetrics(frame, cal);
    expectTrue(calp.calibrated, "calibrated power");
    expectNear(calp.power_W, calp.sumCounts * 2.0, 1e-9, "power = sum * scale");
    expectNear(calp.energy_J, calp.power_W * 0.5, 1e-9, "energy = power * exposure");
  }

  // --- Phase 2: line profiles ---
  {
    auto frame = makeGaussianFrame(40, 40, 20.0, 20.0, 5.0, 5.0, 1.0, 2.0, 2.0);
    const auto h = beam::core::extractHorizontalProfile(frame, 20);
    expectTrue(h.valid && h.intensity.size() == 40, "horizontal profile length");
    expectNear(h.position_um.back(), 39.5 * 2.0, 1e-9, "horizontal end position");
    // Peak near center
    const auto peakIt = std::max_element(h.intensity.begin(), h.intensity.end());
    const auto peakIdx = static_cast<std::size_t>(std::distance(h.intensity.begin(), peakIt));
    expectTrue(peakIdx >= 18 && peakIdx <= 21, "horizontal peak near center");

    const auto v = beam::core::extractVerticalProfile(frame, 20);
    expectTrue(v.valid && v.intensity.size() == 40, "vertical profile length");

    const auto line = beam::core::extractLineProfile(frame, 0, 0, 39, 39);
    expectTrue(line.valid && line.intensity.size() >= 40, "diagonal profile");
  }

  // --- Phase 2: M² scan + report export ---
  {
    const double lambda_nm = 1064.0;
    const double d0 = 80.0;
    const double w0 = d0 * 0.5;
    const double zR_um = 3.14159265358979323846 * w0 * w0 / (lambda_nm * 1e-3);
    const double zR_mm = zR_um / 1000.0;

    std::vector<beam::core::M2ScanPoint> points;
    for (double z : {-2.0 * zR_mm, -zR_mm, 0.0, zR_mm, 2.0 * zR_mm}) {
      const double d = d0 * std::sqrt(1.0 + (z / zR_mm) * (z / zR_mm));
      points.push_back({z, d, d * 1.02, "synth", true});
    }
    const auto scan = beam::core::analyzeM2Scan(points, lambda_nm);
    expectTrue(scan.valid, "M2 scan valid");
    expectNear(scan.fitX.m2, 1.0, 0.2, "scan M2_X ≈ 1");
    expectNear(scan.fitY.m2, 1.0, 0.2, "scan M2_Y ≈ 1");

    const std::string report = beam::core::formatM2Report(scan);
    expectTrue(report.find("M²") != std::string::npos || report.find("M2") != std::string::npos ||
                   report.find("m2") != std::string::npos || report.find("Fit") != std::string::npos,
               "report contains fit section");

    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "beam_analyzer_test";
    fs::create_directories(dir);
    expectTrue(beam::io::exportM2ReportMarkdown((dir / "m2.md").string(), scan, {"", "test"}),
               "export M2 markdown");
    expectTrue(beam::io::exportM2ReportJson((dir / "m2.json").string(), scan, {"", "test"}),
               "export M2 json");

    const auto h = beam::core::extractHorizontalProfile(
        makeGaussianFrame(16, 16, 8, 8, 2, 2), 8);
    expectTrue(beam::io::exportProfileCsv((dir / "prof.csv").string(), h, {"", "p"}),
               "export profile csv");
  }

  // --- Phase 5: noise floor + MomentOptions ---
  {
    // Ideal clean Gaussian: Absolute floor must not destroy 4σ≈32 µm.
    auto clean = makeGaussianFrame(96, 96, 48.0, 48.0, 8.0, 8.0, 20.0, 1.0, 1.0, 0.0);
    beam::core::MomentOptions soft;
    soft.noise.mode = beam::core::NoiseFloorMode::Absolute;
    soft.noise.absoluteLevel = 0.05;  // tiny vs peak=20
    const auto softM = computeIso11146(clean, soft);
    expectTrue(softM.valid, "MomentOptions metrics valid");
    expectNear(softM.appliedNoiseFloor, 0.05, 1e-12, "noise floor applied");
    expectNear(softM.diameterX_um, 32.0, 2.0, "soft Absolute floor keeps 4σ");

    // High pedestal + noise: FractionOfPeak after constant bg should shrink Dx vs raw.
    auto noisy = beam::core::addGaussianNoise(
        makeGaussianFrame(96, 96, 48.0, 48.0, 8.0, 8.0, 20.0, 1.0, 1.0, 2.0), 0.8, 7);
    beam::core::MomentOptions rawOpt;
    const auto raw = computeIso11146(noisy, rawOpt);
    beam::core::MomentOptions gated;
    gated.prep.background.mode = beam::core::BackgroundMode::Constant;
    gated.prep.background.constantValue = 2.0;
    gated.noise.mode = beam::core::NoiseFloorMode::FractionOfPeak;
    gated.noise.fractionOfPeak = 0.05;
    const auto m = computeIso11146(noisy, gated);
    expectTrue(m.valid && raw.valid, "raw and gated metrics valid");
    expectTrue(m.pixelsUsed < noisy.width * noisy.height, "noise gate drops some pixels");
    expectTrue(m.diameterX_um < raw.diameterX_um, "noise gate reduces noise-inflated Dx");
  }

  // --- Phase 5: TEM00 M²≈1 with residuals / flags ---
  {
    const double lambda_nm = 1064.0;
    const double d0_um = 120.0;
    std::vector<beam::core::M2Sample> samples;
    for (double z = -30.0; z <= 30.0 + 1e-9; z += 5.0) {
      samples.push_back(
          {z, beam::core::hyperbolicDiameter_um(z, d0_um, 0.0, 1.0, lambda_nm), 1.0});
    }
    const auto fit = beam::core::fitM2(samples, lambda_nm);
    expectTrue(fit.valid, "TEM00 fit valid");
    expectNear(fit.m2, 1.0, 0.08, "TEM00 M2≈1");
    expectTrue(fit.residuals.size() == samples.size(), "residuals per sample");
    expectTrue(fit.rmsResidual < 1.0, "low RMS residual for exact hyperbola");
    expectTrue(fit.qualityFlags == beam::core::M2QualityOk ||
                   (fit.qualityFlags & beam::core::M2QualityTooFewPoints) == 0,
               "no severe quality flags for good TEM00");
  }

  // --- Phase 5: non-ideal M²≈2.5 ---
  {
    const double lambda_nm = 532.0;
    const double d0_um = 80.0;
    const double m2True = 2.5;
    std::vector<beam::core::M2Sample> samples;
    for (double z = -25.0; z <= 25.0 + 1e-9; z += 5.0) {
      samples.push_back(
          {z, beam::core::hyperbolicDiameter_um(z, d0_um, 2.0, m2True, lambda_nm), 1.0});
    }
    const auto fit = beam::core::fitM2(samples, lambda_nm);
    expectTrue(fit.valid, "non-ideal fit valid");
    expectNear(fit.m2, m2True, 0.2, "non-ideal M2≈2.5");
    expectNear(fit.waistZ_mm, 2.0, 1.0, "non-ideal waist z");
  }

  // --- Phase 5: motor coarse→fine session ---
  {
    beam::core::ScanSessionConfig cfg;
    cfg.wavelength_nm = 1064.0;
    cfg.coarse = {-40.0, 40.0, 10.0};
    cfg.fine = {6.0, 1.5, true, 1e-3};
    cfg.runFineAfterCoarse = true;

    beam::core::SyntheticBeamModel model;
    model.wavelength_nm = 1064.0;
    model.m2X = 1.0;
    model.m2Y = 1.3;
    model.waistDiameterX_um = 90.0;
    model.waistDiameterY_um = 95.0;
    model.waistZ_mm = 3.0;

    const auto coarseOnly = beam::core::planCoarsePositions(cfg.coarse);
    expectTrue(coarseOnly.size() >= 5, "coarse plan has several stations");

    beam::core::SimulatedStage stage(cfg.coarse.zStart_mm);
    auto acquire = beam::core::makeSyntheticAcquire(model);
    const auto session = beam::core::runScanSession(stage, acquire, cfg);
    expectTrue(session.valid, "scan session valid");
    expectTrue(session.points.size() > coarseOnly.size(), "fine adds points");
    expectNear(session.full.fitX.m2, 1.0, 0.15, "session M2_X≈1");
    expectNear(session.full.fitY.m2, 1.3, 0.2, "session M2_Y≈1.3");

    const std::string csv = beam::core::formatDzCurveCsv(session.full);
    expectTrue(csv.find("z_mm") != std::string::npos, "D(z) CSV header");
    const std::string plot = beam::core::formatDzAsciiPlot(session.full);
    expectTrue(plot.find("D(z)") != std::string::npos, "ASCII D(z) plot");

    // Single-axis API
    const auto sx =
        beam::core::analyzeM2ScanSingleAxis(session.points, cfg.wavelength_nm, false);
    expectTrue(sx.valid && sx.fitX.valid, "single-axis X fit");
  }

  if (gFailures != 0) {
    std::cerr << gFailures << " failure(s)\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
