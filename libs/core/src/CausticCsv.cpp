#include "beam/core/CausticCsv.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace beam::core {
namespace {

std::string trim(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
    s.erase(s.begin());
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
    s.pop_back();
  }
  return s;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string cur;
  bool inQuotes = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"') {
      if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
        cur.push_back('"');
        ++i;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (c == ',' && !inQuotes) {
      fields.push_back(trim(cur));
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  fields.push_back(trim(cur));
  return fields;
}

bool parseDouble(const std::string& s, double* out) {
  if (!out || s.empty()) {
    return false;
  }
  try {
    std::size_t idx = 0;
    const double v = std::stod(s, &idx);
    if (idx != s.size() || !std::isfinite(v)) {
      return false;
    }
    *out = v;
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

CausticCsvParseResult parseCausticCsv(const std::string& textIn) {
  CausticCsvParseResult out;
  std::string text = textIn;
  if (!text.empty() && static_cast<unsigned char>(text[0]) == 0xEF) {
    // Strip UTF-8 BOM
    if (text.size() >= 3 && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
      text.erase(0, 3);
    }
  }

  std::istringstream iss(text);
  std::string line;
  std::vector<std::string> header;
  while (std::getline(iss, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    header = splitCsvLine(line);
    break;
  }
  if (header.size() < 3) {
    out.message = "CSV header needs at least 3 columns";
    return out;
  }

  const bool diam =
      header[0] == "z_mm" && (header[1] == "Dx_um" || header[1] == "dx_um") &&
      (header[2] == "Dy_um" || header[2] == "dy_um");
  const bool radii =
      header[0] == "z_mm" && (header[1] == "wx_um" || header[1] == "Wx_um") &&
      (header[2] == "wy_um" || header[2] == "Wy_um");
  if (!diam && !radii) {
    out.message =
        "CSV header must be z_mm,Dx_um,Dy_um or z_mm,wx_um,wy_um "
        "(optional included,note)";
    return out;
  }
  out.radiiInput = radii;
  if (header.size() > 5) {
    out.message = "Too many columns (max 5: z,Dx/wx,Dy/wy,included,note)";
    return out;
  }
  if (header.size() >= 4 && header[3] != "included" && header[3] != "valid") {
    out.message = "4th column must be included or valid";
    return out;
  }
  if (header.size() >= 5 && header[4] != "note" && header[4] != "source") {
    out.message = "5th column must be note or source";
    return out;
  }

  std::unordered_set<std::string> seenZ;
  int rowNum = 1;
  while (std::getline(iss, line)) {
    ++rowNum;
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const auto fields = splitCsvLine(line);
    if (fields.size() < 3) {
      out.message = "Row " + std::to_string(rowNum) + ": need z,Dx/wx,Dy/wy";
      return out;
    }
    double z = 0;
    double a = 0;
    double b = 0;
    if (!parseDouble(fields[0], &z) || !parseDouble(fields[1], &a) || !parseDouble(fields[2], &b)) {
      out.message = "Row " + std::to_string(rowNum) + ": non-finite numeric fields";
      return out;
    }
    if (!(a > 0.0) || !(b > 0.0)) {
      out.message = "Row " + std::to_string(rowNum) + ": widths must be positive";
      return out;
    }
    const std::string zKey = std::to_string(z);
    if (!seenZ.insert(zKey).second) {
      out.message = "Duplicate z position at row " + std::to_string(rowNum);
      return out;
    }
    M2ScanPoint p;
    p.z_mm = z;
    if (radii) {
      p.diameterX_um = 2.0 * a;
      p.diameterY_um = 2.0 * b;
    } else {
      p.diameterX_um = a;
      p.diameterY_um = b;
    }
    p.valid = true;
    if (fields.size() >= 4 && !fields[3].empty()) {
      p.valid = !(fields[3] == "0" || fields[3] == "false" || fields[3] == "False");
    }
    if (fields.size() >= 5) {
      p.sourceLabel = fields[4];
    } else {
      p.sourceLabel = radii ? "csv:radius" : "csv:diameter";
    }
    out.points.push_back(p);
    if (out.points.size() > 512) {
      out.message = "Too many rows (max 512)";
      return out;
    }
  }

  if (out.points.empty()) {
    out.message = "CSV has header but no data rows";
    return out;
  }
  out.ok = true;
  out.message = "OK";
  return out;
}

M2AxisDiagnostics diagnoseM2Axis(const M2FitResult& fit,
                                 const std::vector<M2ScanPoint>& points,
                                 bool useY) {
  M2AxisDiagnostics d;
  if (!fit.valid) {
    d.warnings.push_back("axis fit invalid");
    return d;
  }

  const double z0 = fit.waistZ_mm;
  const double zR = std::max(fit.rayleighRange_mm, 1e-12);
  const double w0_m = (fit.waistDiameter_um * 0.5) * 1e-6;
  // θ ≈ λ M² / (π w0); equivalently from zR: zR = π w0² /(M² λ) → θ = w0/zR
  const double zR_m = zR * 1e-3;
  d.divergenceHalf_mrad = (zR_m > 0.0) ? (w0_m / zR_m) * 1e3 : 0.0;
  d.bpp_mm_mrad = (w0_m * 1e3) * d.divergenceHalf_mrad;

  for (const auto& p : points) {
    if (!p.valid) {
      continue;
    }
    const double diam = useY ? p.diameterY_um : p.diameterX_um;
    if (!(diam > 0.0) || !std::isfinite(p.z_mm)) {
      continue;
    }
    ++d.includedCount;
    if (p.z_mm < z0) {
      ++d.leftOfWaist;
    } else if (p.z_mm > z0) {
      ++d.rightOfWaist;
    }
    const double dz = std::abs(p.z_mm - z0);
    if (dz <= zR) {
      ++d.nearWaist;
    }
    if (dz >= 2.0 * zR) {
      ++d.farField;
    }
  }

  if (fit.m2 < 0.9) {
    d.warnings.push_back("M² < 0.9: check units / model applicability");
  }
  if (d.includedCount > 0) {
    double zMin = 0;
    double zMax = 0;
    bool init = false;
    for (const auto& p : points) {
      if (!p.valid) {
        continue;
      }
      if (!init) {
        zMin = zMax = p.z_mm;
        init = true;
      } else {
        zMin = std::min(zMin, p.z_mm);
        zMax = std::max(zMax, p.z_mm);
      }
    }
    if (init && (z0 < zMin || z0 > zMax)) {
      d.warnings.push_back("fitted waist outside sampled z range (extrapolation)");
    }
  }
  if (!d.leftOfWaist || !d.rightOfWaist) {
    d.warnings.push_back("waist not covered on both sides");
  }
  if (!d.nearWaist) {
    d.warnings.push_back("no sample within one Rayleigh range of waist");
  }
  if (!d.farField) {
    d.warnings.push_back("no sample at ≥ 2× Rayleigh range");
  }
  if (d.includedCount < 5) {
    d.warnings.push_back("fewer than 5 included stations (engineering floor)");
  }
  if (fit.qualityFlags & M2QualityHighResidual) {
    d.warnings.push_back("high relative residual");
  }
  if (fit.qualityFlags & M2QualityAsymmetricCoverage) {
    d.warnings.push_back("asymmetric axial coverage");
  }
  return d;
}

std::string formatM2ReportRich(const M2ScanResult& result,
                               const M2ReportMeta& meta,
                               const std::string& title) {
  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(6);

  ss << "# " << title << "\n\n";
  ss << "> Analysis record for engineering review — not a metrology certificate. "
        "ISO 11146 conformity and measurement uncertainty are not claimed.\n\n";

  ss << "## Measurement metadata\n\n";
  ss << "| Field | Value |\n|---|---|\n";
  ss << "| Sample / measurement | " << (meta.sampleName.empty() ? "—" : meta.sampleName)
     << " |\n";
  ss << "| Operator | " << (meta.operatorName.empty() ? "—" : meta.operatorName) << " |\n";
  ss << "| Device ID | " << (meta.deviceId.empty() ? "—" : meta.deviceId) << " |\n";
  ss << "| Calibration ID | " << (meta.calibrationId.empty() ? "—" : meta.calibrationId)
     << " |\n";
  ss << "| Environment / notes | "
     << (meta.environmentNote.empty() ? "—" : meta.environmentNote) << " |\n";
  ss << "| Source | " << (meta.sourcePath.empty() ? "—" : meta.sourcePath) << " |\n";
  ss << "| Note | " << (meta.note.empty() ? "—" : meta.note) << " |\n";
  ss << "| Wavelength (nm) | " << result.wavelength_nm << " |\n";
  ss << "| Points | " << result.points.size() << " |\n";
  ss << "| Valid | " << (result.valid ? "yes" : "no") << " |\n\n";

  ss << formatM2Report(result, title);

  const auto dx = diagnoseM2Axis(result.fitX, result.points, false);
  const auto dy = diagnoseM2Axis(result.fitY, result.points, true);
  ss << "\n## Coverage / quality review\n\n";
  ss << "| Item | X | Y |\n|---|---|---|\n";
  ss << "| Included count | " << dx.includedCount << " | " << dy.includedCount << " |\n";
  ss << "| Left / right of waist | " << dx.leftOfWaist << " / " << dx.rightOfWaist << " | "
     << dy.leftOfWaist << " / " << dy.rightOfWaist << " |\n";
  ss << "| Near (≤zR) / far (≥2zR) | " << dx.nearWaist << " / " << dx.farField << " | "
     << dy.nearWaist << " / " << dy.farField << " |\n";
  ss << "| Divergence half-angle (mrad) | " << dx.divergenceHalf_mrad << " | "
     << dy.divergenceHalf_mrad << " |\n";
  ss << "| BPP (mm·mrad) | " << dx.bpp_mm_mrad << " | " << dy.bpp_mm_mrad << " |\n";
  ss << "| RMS residual (µm) | " << result.fitX.rmsResidual << " | " << result.fitY.rmsResidual
     << " |\n\n";

  auto dumpWarn = [&](const char* axis, const M2AxisDiagnostics& d) {
    for (const auto& w : d.warnings) {
      ss << "- " << axis << ": " << w << "\n";
    }
  };
  ss << "### Warnings\n\n";
  if (dx.warnings.empty() && dy.warnings.empty()) {
    ss << "- No engineering warnings triggered (does not imply standard conformity).\n";
  } else {
    dumpWarn("X", dx);
    dumpWarn("Y", dy);
  }
  ss << "\n---\nGenerated by Beam Analyzer\n";
  return ss.str();
}

}  // namespace beam::core
