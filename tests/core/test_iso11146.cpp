#include "beam/core/Iso11146.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/io/CsvImporter.hpp"
#include "beam/io/ResultExporter.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

  if (gFailures != 0) {
    std::cerr << gFailures << " failure(s)\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
