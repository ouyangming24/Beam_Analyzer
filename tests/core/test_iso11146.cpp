#include "beam/core/Iso11146.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/core/M2Scan.hpp"
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

  if (gFailures != 0) {
    std::cerr << gFailures << " failure(s)\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
