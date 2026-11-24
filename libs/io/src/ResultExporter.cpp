#include "beam/io/ResultExporter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace beam::io {
namespace {

constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

std::string escapeJson(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

void writeFitJson(std::ostream& out, const char* key, const beam::core::M2FitResult& f) {
  out << "  \"" << key << "\": {\n";
  out << "    \"valid\": " << (f.valid ? "true" : "false") << ",\n";
  out << "    \"m2\": " << f.m2 << ",\n";
  out << "    \"m2_uncertainty\": " << f.m2Uncertainty << ",\n";
  out << "    \"waist_diameter_um\": " << f.waistDiameter_um << ",\n";
  out << "    \"waist_diameter_uncertainty_um\": " << f.waistDiameterUncertainty_um << ",\n";
  out << "    \"waist_z_mm\": " << f.waistZ_mm << ",\n";
  out << "    \"waist_z_uncertainty_mm\": " << f.waistZUncertainty_mm << ",\n";
  out << "    \"rayleigh_range_mm\": " << f.rayleighRange_mm << ",\n";
  out << "    \"rms_residual_um\": " << f.rmsResidual << ",\n";
  out << "    \"rms_residual_d2\": " << f.rmsResidualD2 << ",\n";
  out << "    \"relative_rms\": " << f.relativeRms << ",\n";
  out << "    \"quality_flags\": " << f.qualityFlags << ",\n";
  out << "    \"quality_flags_text\": \""
      << escapeJson(beam::core::describeQualityFlags(f.qualityFlags)) << "\",\n";
  out << "    \"residuals\": [\n";
  for (std::size_t i = 0; i < f.residuals.size(); ++i) {
    const auto& r = f.residuals[i];
    out << "      {\"z_mm\": " << r.z_mm << ", \"measured_um\": " << r.measured_um
        << ", \"fitted_um\": " << r.fitted_um << ", \"residual_um\": " << r.residual_um
        << "}";
    if (i + 1 < f.residuals.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "    ],\n";
  out << "    \"message\": \"" << escapeJson(f.message) << "\"\n";
  out << "  }";
}

}  // namespace

bool exportMetricsJson(const std::string& path,
                       const beam::core::Iso11146Metrics& metrics,
                       const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.iso11146.v1\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"valid\": " << (metrics.valid ? "true" : "false") << ",\n";
  out << "  \"total_power\": " << metrics.totalPower << ",\n";
  out << "  \"centroid_x_um\": " << metrics.centroidX_um << ",\n";
  out << "  \"centroid_y_um\": " << metrics.centroidY_um << ",\n";
  out << "  \"diameter_x_um\": " << metrics.diameterX_um << ",\n";
  out << "  \"diameter_y_um\": " << metrics.diameterY_um << ",\n";
  out << "  \"diameter_major_um\": " << metrics.diameterMajor_um << ",\n";
  out << "  \"diameter_minor_um\": " << metrics.diameterMinor_um << ",\n";
  out << "  \"orientation_rad\": " << metrics.orientation_rad << ",\n";
  out << "  \"orientation_deg\": " << (metrics.orientation_rad * kRadToDeg) << ",\n";
  out << "  \"ellipticity\": " << metrics.ellipticity << "\n";
  out << "}\n";
  return static_cast<bool>(out);
}

bool exportMetricsCsv(const std::string& path,
                      const beam::core::Iso11146Metrics& metrics,
                      const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  out << std::setprecision(10);
  out << "key,value\n";
  out << "schema,beam_analyzer.iso11146.v1\n";
  out << "source," << meta.sourcePath << "\n";
  out << "note," << meta.note << "\n";
  out << "valid," << (metrics.valid ? 1 : 0) << "\n";
  out << "total_power," << metrics.totalPower << "\n";
  out << "centroid_x_um," << metrics.centroidX_um << "\n";
  out << "centroid_y_um," << metrics.centroidY_um << "\n";
  out << "diameter_x_um," << metrics.diameterX_um << "\n";
  out << "diameter_y_um," << metrics.diameterY_um << "\n";
  out << "diameter_major_um," << metrics.diameterMajor_um << "\n";
  out << "diameter_minor_um," << metrics.diameterMinor_um << "\n";
  out << "orientation_rad," << metrics.orientation_rad << "\n";
  out << "orientation_deg," << (metrics.orientation_rad * kRadToDeg) << "\n";
  out << "ellipticity," << metrics.ellipticity << "\n";
  return static_cast<bool>(out);
}

bool exportPowerJson(const std::string& path,
                     const beam::core::PowerMetrics& power,
                     const beam::core::PowerCalibration& cal,
                     const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.power.v1\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"valid\": " << (power.valid ? "true" : "false") << ",\n";
  out << "  \"calibrated\": " << (power.calibrated ? "true" : "false") << ",\n";
  out << "  \"sum_counts\": " << power.sumCounts << ",\n";
  out << "  \"peak_counts\": " << power.peakCounts << ",\n";
  out << "  \"mean_counts\": " << power.meanCounts << ",\n";
  out << "  \"power_W\": " << power.power_W << ",\n";
  out << "  \"energy_J\": " << power.energy_J << ",\n";
  out << "  \"calibration\": {\n";
  out << "    \"counts_to_watts\": " << cal.countsToWatts << ",\n";
  out << "    \"exposure_s\": " << cal.exposure_s << ",\n";
  out << "    \"wavelength_nm\": " << cal.wavelength_nm << "\n";
  out << "  }\n";
  out << "}\n";
  return static_cast<bool>(out);
}

bool exportProfileCsv(const std::string& path,
                      const beam::core::LineProfile& profile,
                      const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "# source=" << meta.sourcePath << "\n";
  out << "# note=" << meta.note << "\n";
  out << "position_um,intensity\n";
  const std::size_t n = std::min(profile.position_um.size(), profile.intensity.size());
  for (std::size_t i = 0; i < n; ++i) {
    out << profile.position_um[i] << "," << profile.intensity[i] << "\n";
  }
  return static_cast<bool>(out);
}

bool exportM2ReportMarkdown(const std::string& path,
                            const beam::core::M2ScanResult& result,
                            const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << beam::core::formatM2ReportRich(result, toM2ReportMeta(meta));
  return static_cast<bool>(out);
}

bool exportM2ReportJson(const std::string& path,
                        const beam::core::M2ScanResult& result,
                        const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "{\n";
  out << "  \"schema\": \"beam_analyzer.m2_scan.v2\",\n";
  out << "  \"source\": \"" << escapeJson(meta.sourcePath) << "\",\n";
  out << "  \"note\": \"" << escapeJson(meta.note) << "\",\n";
  out << "  \"sample\": \"" << escapeJson(meta.sampleName) << "\",\n";
  out << "  \"operator\": \"" << escapeJson(meta.operatorName) << "\",\n";
  out << "  \"device_id\": \"" << escapeJson(meta.deviceId) << "\",\n";
  out << "  \"calibration_id\": \"" << escapeJson(meta.calibrationId) << "\",\n";
  out << "  \"environment\": \"" << escapeJson(meta.environmentNote) << "\",\n";
  out << "  \"wavelength_nm\": " << result.wavelength_nm << ",\n";
  out << "  \"valid\": " << (result.valid ? "true" : "false") << ",\n";
  out << "  \"message\": \"" << escapeJson(result.message) << "\",\n";
  out << "  \"points\": [\n";
  for (std::size_t i = 0; i < result.points.size(); ++i) {
    const auto& p = result.points[i];
    out << "    {\"z_mm\": " << p.z_mm << ", \"diameter_x_um\": " << p.diameterX_um
        << ", \"diameter_y_um\": " << p.diameterY_um << ", \"valid\": "
        << (p.valid ? "true" : "false") << ", \"source\": \""
        << escapeJson(p.sourceLabel) << "\"}";
    if (i + 1 < result.points.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ],\n";
  writeFitJson(out, "fit_x", result.fitX);
  out << ",\n";
  writeFitJson(out, "fit_y", result.fitY);
  out << ",\n";
  writeFitJson(out, "fit_combined", result.fitCombined);
  out << "\n}\n";
  return static_cast<bool>(out);
}

bool exportM2ResidualsCsv(const std::string& path,
                          const beam::core::M2ScanResult& result,
                          const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(10);
  out << "# source=" << meta.sourcePath << "\n";
  out << "# note=" << meta.note << "\n";
  out << "z_mm,measured_x_um,fitted_x_um,residual_x_um,"
         "measured_y_um,fitted_y_um,residual_y_um\n";

  const auto& rx = result.fitX.residuals;
  const auto& ry = result.fitY.residuals;
  const std::size_t n = std::max(rx.size(), ry.size());
  for (std::size_t i = 0; i < n; ++i) {
    const bool hasX = i < rx.size();
    const bool hasY = i < ry.size();
    const double z = hasX ? rx[i].z_mm : (hasY ? ry[i].z_mm : 0.0);
    out << z << ",";
    if (hasX) {
      out << rx[i].measured_um << "," << rx[i].fitted_um << "," << rx[i].residual_um;
    } else {
      out << ",,";
    }
    out << ",";
    if (hasY) {
      out << ry[i].measured_um << "," << ry[i].fitted_um << "," << ry[i].residual_um;
    } else {
      out << ",,";
    }
    out << "\n";
  }
  return static_cast<bool>(out);
}

bool exportM2ReportHtml(const std::string& path,
                        const beam::core::M2ScanResult& result,
                        const ExportMeta& meta) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  auto esc = [](const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '&':
          o += "&amp;";
          break;
        case '<':
          o += "&lt;";
          break;
        case '>':
          o += "&gt;";
          break;
        case '"':
          o += "&quot;";
          break;
        default:
          o += c;
          break;
      }
    }
    return o;
  };
  auto cell = [&](double v, int prec = 4) {
    std::ostringstream ss;
    if (!std::isfinite(v)) {
      return std::string("—");
    }
    ss << std::setprecision(prec) << v;
    return ss.str();
  };

  const auto dx = beam::core::diagnoseM2Axis(result.fitX, result.points, false);
  const auto dy = beam::core::diagnoseM2Axis(result.fitY, result.points, true);

  out << "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
         "<title>Beam Analyzer M² Report</title><style>"
         "body{font:13px/1.55 'Segoe UI','Microsoft YaHei',sans-serif;color:#1e293b;"
         "max-width:960px;margin:28px auto;padding:0 20px}"
         "h1{font-size:22px;margin-bottom:4px}h2{font-size:15px;border-bottom:1px solid #cbd5e1;"
         "padding-bottom:6px;margin-top:26px}"
         "table{border-collapse:collapse;width:100%;font-size:12px;margin:10px 0}"
         "th,td{border:1px solid #cbd5e1;padding:6px 8px;text-align:right}"
         "th:first-child,td:first-child{text-align:left}th{background:#e2e8f0}"
         ".notice{background:#fff7ed;border:1px solid #fdba74;padding:10px 12px;margin:12px 0}"
         "footer{margin-top:28px;border-top:1px solid #94a3b8;padding-top:12px;font-size:11px;"
         "color:#64748b}"
         "@media print{body{margin:0;max-width:none}}"
         "</style></head><body>";
  out << "<h1>光束传播质量（M²）分析报告</h1>";
  out << "<p>Beam Analyzer · 双轴焦散拟合 · 直径定义 D = D4σ</p>";
  out << "<div class=\"notice\">本文件为工程分析记录，不是计量证书。"
         "不声称 ISO 11146 符合性、完整广义像散或测量不确定度。</div>";

  out << "<h2>1. 测量元信息</h2><table><tbody>";
  auto row = [&](const char* k, const std::string& v) {
    out << "<tr><td>" << k << "</td><td>" << esc(v.empty() ? "—" : v) << "</td></tr>";
  };
  row("样品 / 测量名称", meta.sampleName);
  row("操作员", meta.operatorName);
  row("仪器 / 设备标识", meta.deviceId);
  row("标定记录 ID", meta.calibrationId);
  row("环境 / 备注", meta.environmentNote);
  row("来源", meta.sourcePath);
  row("说明", meta.note);
  out << "<tr><td>波长 / nm</td><td>" << cell(result.wavelength_nm) << "</td></tr>";
  out << "<tr><td>测点数</td><td>" << result.points.size() << "</td></tr>";
  out << "<tr><td>状态</td><td>" << esc(result.message) << "</td></tr>";
  out << "</tbody></table>";

  out << "<h2>2. 双轴结果</h2><table><thead><tr>"
         "<th>参数</th><th>单位</th><th>X</th><th>Y</th></tr></thead><tbody>";
  auto mrow = [&](const char* name, const char* unit, double x, double y) {
    out << "<tr><td>" << name << "</td><td>" << unit << "</td><td>" << cell(x)
        << "</td><td>" << cell(y) << "</td></tr>";
  };
  mrow("M²", "—", result.fitX.m2, result.fitY.m2);
  mrow("腰斑直径 D0", "µm", result.fitX.waistDiameter_um, result.fitY.waistDiameter_um);
  mrow("束腰位置 z0", "mm", result.fitX.waistZ_mm, result.fitY.waistZ_mm);
  mrow("瑞利长度 zR", "mm", result.fitX.rayleighRange_mm, result.fitY.rayleighRange_mm);
  mrow("发散半角 θ", "mrad", dx.divergenceHalf_mrad, dy.divergenceHalf_mrad);
  mrow("BPP", "mm·mrad", dx.bpp_mm_mrad, dy.bpp_mm_mrad);
  mrow("RMS 残差", "µm", result.fitX.rmsResidual, result.fitY.rmsResidual);
  mrow("相对 RMS", "—", result.fitX.relativeRms, result.fitY.relativeRms);
  out << "</tbody></table>";

  out << "<h2>3. 采样覆盖 / 质量复核</h2><table><thead><tr>"
         "<th>项目</th><th>X</th><th>Y</th></tr></thead><tbody>";
  out << "<tr><td>采用点数</td><td>" << dx.includedCount << "</td><td>" << dy.includedCount
      << "</td></tr>";
  out << "<tr><td>束腰左 / 右</td><td>" << dx.leftOfWaist << " / " << dx.rightOfWaist
      << "</td><td>" << dy.leftOfWaist << " / " << dy.rightOfWaist << "</td></tr>";
  out << "<tr><td>|z−z0|≤zR / ≥2zR</td><td>" << dx.nearWaist << " / " << dx.farField
      << "</td><td>" << dy.nearWaist << " / " << dy.farField << "</td></tr>";
  out << "</tbody></table><ul>";
  auto dumpW = [&](const char* axis, const beam::core::M2AxisDiagnostics& d) {
    for (const auto& w : d.warnings) {
      out << "<li>" << axis << "：" << esc(w) << "</li>";
    }
  };
  if (dx.warnings.empty() && dy.warnings.empty()) {
    out << "<li>未触发当前工程诊断提示；不等于标准符合性。</li>";
  } else {
    dumpW("X", dx);
    dumpW("Y", dy);
  }
  out << "</ul>";

  out << "<h2>4. 全部测点</h2><table><thead><tr>"
         "<th>z / mm</th><th>Dx / µm</th><th>Dy / µm</th><th>采用</th><th>来源 / 备注</th>"
         "</tr></thead><tbody>";
  for (const auto& p : result.points) {
    out << "<tr><td>" << cell(p.z_mm, 4) << "</td><td>" << cell(p.diameterX_um, 4)
        << "</td><td>" << cell(p.diameterY_um, 4) << "</td><td>"
        << (p.valid ? "是" : "否") << "</td><td>" << esc(p.sourceLabel) << "</td></tr>";
  }
  out << "</tbody></table>";

  out << "<h2>5. 残差（测 − 拟合）</h2><table><thead><tr>"
         "<th>z / mm</th><th>Δx / µm</th><th>Δy / µm</th></tr></thead><tbody>";
  const auto& rx = result.fitX.residuals;
  const auto& ry = result.fitY.residuals;
  const std::size_t n = std::max(rx.size(), ry.size());
  for (std::size_t i = 0; i < n; ++i) {
    const bool hasX = i < rx.size();
    const bool hasY = i < ry.size();
    const double z = hasX ? rx[i].z_mm : (hasY ? ry[i].z_mm : 0.0);
    out << "<tr><td>" << cell(z, 4) << "</td><td>"
        << (hasX ? cell(rx[i].residual_um, 4) : "—") << "</td><td>"
        << (hasY ? cell(ry[i].residual_um, 4) : "—") << "</td></tr>";
  }
  out << "</tbody></table>";

  out << "<footer>审核人：________________　日期：________________<br>"
         "Beam Analyzer · 原始数据请与测点表及会话文件一并归档。</footer>";
  out << "</body></html>\n";
  return static_cast<bool>(out);
}

namespace {

std::string escHtml(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&':
        o += "&amp;";
        break;
      case '<':
        o += "&lt;";
        break;
      case '>':
        o += "&gt;";
        break;
      case '"':
        o += "&quot;";
        break;
      default:
        o += c;
        break;
    }
  }
  return o;
}

std::string fmtNum(double v, int prec = 4) {
  if (!std::isfinite(v)) {
    return "—";
  }
  std::ostringstream ss;
  ss << std::setprecision(prec) << v;
  return ss.str();
}

}  // namespace

bool exportFrameAnalysisMarkdown(const std::string& path, const FrameAnalysisReport& report) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::setprecision(6);
  out << "# Beam Analyzer — 单帧分析报告\n\n";
  out << "> **概念实现 / 非正式计量证书。** 不声称与任何商业光束分析仪等同，"
         "也不构成 ISO 认证结果。\n\n";
  out << "## 元数据\n\n";
  out << "| 项 | 值 |\n|---|---|\n";
  out << "| 样品 | " << report.meta.sampleName << " |\n";
  out << "| 操作员 | " << report.meta.operatorName << " |\n";
  out << "| 设备 | " << report.meta.deviceId << " |\n";
  out << "| 标定 ID | " << report.meta.calibrationId << " |\n";
  out << "| 来源 | " << report.meta.sourcePath << " |\n";
  out << "| 帧尺寸 | " << report.frameWidth << " × " << report.frameHeight << " |\n";
  out << "| 像素尺寸 / µm | " << report.pixelSizeX_um << " × " << report.pixelSizeY_um
      << " |\n";
  out << "| 备注 | " << report.meta.note << " |\n\n";

  if (!report.annotatedImageRelPath.empty()) {
    out << "## 强度图\n\n!["
        << "annotated](" << report.annotatedImageRelPath << ")\n\n";
  }

  out << "## ISO 11146（二阶矩）\n\n";
  out << "| 量 | 数值 | 单位 |\n|---|---|---|\n";
  out << "| 质心 X | " << report.iso.centroidX_um << " | µm |\n";
  out << "| 质心 Y | " << report.iso.centroidY_um << " | µm |\n";
  out << "| Dx (4σ) | " << report.iso.diameterX_um << " | µm |\n";
  out << "| Dy (4σ) | " << report.iso.diameterY_um << " | µm |\n";
  out << "| D major | " << report.iso.diameterMajor_um << " | µm |\n";
  out << "| D minor | " << report.iso.diameterMinor_um << " | µm |\n";
  out << "| 方位角 | " << (report.iso.orientation_rad * kRadToDeg) << " | ° |\n";
  out << "| 椭圆度 | " << report.iso.ellipticity << " | — |\n\n";

  out << "## 其他宽度工具（概念）\n\n";
  out << "| 方法 | Wx / µm | Wy / µm | D / µm |\n|---|---|---|---|\n";
  auto wrow = [&](const char* name, const beam::core::AxisWidthResult& w) {
    out << "| " << name << " | " << w.widthX_um << " | " << w.widthY_um << " | "
        << w.diameter_um << " |\n";
  };
  wrow("刀口 10/90", report.widths.knife1090);
  wrow("刀口 16/84", report.widths.knife1684);
  wrow("% 峰值 (13.5%)", report.widths.percentPeak);
  wrow("% 能量 (86.5%)", report.widths.percentEnergy);
  wrow("包围功率 86.5%", report.widths.encircled);
  out << "\n";

  out << "## 功率 / 能量\n\n";
  out << "| 量 | 数值 |\n|---|---|\n";
  out << "| Σ counts | " << report.power.sumCounts << " |\n";
  out << "| 峰值 counts | " << report.power.peakCounts << " |\n";
  if (report.power.calibrated) {
    out << "| 功率 / W | " << report.power.power_W << " |\n";
    out << "| 能量 / J | " << report.power.energy_J << " |\n";
  } else {
    out << "| 功率 | 未标定 |\n";
  }
  out << "\n---\nBeam Analyzer · 工程预览报告\n";
  return static_cast<bool>(out);
}

bool exportFrameAnalysisHtml(const std::string& path, const FrameAnalysisReport& report) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
         "<title>Beam Analyzer Frame Report</title><style>"
         "body{font:13px/1.55 system-ui,sans-serif;color:#1e293b;max-width:920px;"
         "margin:24px auto;padding:0 16px}"
         "h1{font-size:20px}table{border-collapse:collapse;width:100%;margin:10px 0}"
         "th,td{border:1px solid #cbd5e1;padding:6px 8px;text-align:right}"
         "th:first-child,td:first-child{text-align:left}th{background:#e2e8f0}"
         ".notice{background:#fff7ed;border:1px solid #fdba74;padding:10px;margin:12px 0}"
         "img{max-width:100%;border:1px solid #cbd5e1}"
         "footer{margin-top:24px;font-size:11px;color:#64748b;border-top:1px solid #94a3b8;"
         "padding-top:10px}"
         "</style></head><body>";
  out << "<h1>Beam Analyzer — 单帧分析报告</h1>";
  out << "<div class=\"notice\"><strong>概念实现 / 非正式计量证书。</strong>"
         "不声称与商业光束分析仪等同，也不构成 ISO 认证结果。</div>";

  out << "<h2>元数据</h2><table><tbody>";
  auto row = [&](const char* k, const std::string& v) {
    out << "<tr><td>" << k << "</td><td>" << escHtml(v.empty() ? "—" : v) << "</td></tr>";
  };
  row("样品", report.meta.sampleName);
  row("操作员", report.meta.operatorName);
  row("设备", report.meta.deviceId);
  row("标定 ID", report.meta.calibrationId);
  row("来源", report.meta.sourcePath);
  out << "<tr><td>帧尺寸</td><td>" << report.frameWidth << " × " << report.frameHeight
      << "</td></tr>";
  out << "<tr><td>像素尺寸 / µm</td><td>" << fmtNum(report.pixelSizeX_um) << " × "
      << fmtNum(report.pixelSizeY_um) << "</td></tr>";
  row("备注", report.meta.note);
  out << "</tbody></table>";

  if (!report.annotatedImageRelPath.empty()) {
    out << "<h2>强度图</h2><p><img src=\"" << escHtml(report.annotatedImageRelPath)
        << "\" alt=\"annotated\"/></p>";
  }

  out << "<h2>ISO 11146</h2><table><thead><tr><th>量</th><th>数值</th><th>单位</th>"
         "</tr></thead><tbody>";
  auto mrow = [&](const char* n, double v, const char* u) {
    out << "<tr><td>" << n << "</td><td>" << fmtNum(v) << "</td><td>" << u << "</td></tr>";
  };
  mrow("质心 X", report.iso.centroidX_um, "µm");
  mrow("质心 Y", report.iso.centroidY_um, "µm");
  mrow("Dx (4σ)", report.iso.diameterX_um, "µm");
  mrow("Dy (4σ)", report.iso.diameterY_um, "µm");
  mrow("D major", report.iso.diameterMajor_um, "µm");
  mrow("D minor", report.iso.diameterMinor_um, "µm");
  mrow("方位角", report.iso.orientation_rad * kRadToDeg, "°");
  mrow("椭圆度", report.iso.ellipticity, "—");
  out << "</tbody></table>";

  out << "<h2>宽度工具</h2><table><thead><tr><th>方法</th><th>Wx / µm</th>"
         "<th>Wy / µm</th><th>D / µm</th></tr></thead><tbody>";
  auto wrow = [&](const char* n, const beam::core::AxisWidthResult& w) {
    out << "<tr><td>" << n << "</td><td>" << fmtNum(w.widthX_um) << "</td><td>"
        << fmtNum(w.widthY_um) << "</td><td>" << fmtNum(w.diameter_um) << "</td></tr>";
  };
  wrow("刀口 10/90", report.widths.knife1090);
  wrow("刀口 16/84", report.widths.knife1684);
  wrow("% 峰值", report.widths.percentPeak);
  wrow("% 能量", report.widths.percentEnergy);
  wrow("包围功率", report.widths.encircled);
  out << "</tbody></table>";

  out << "<h2>功率</h2><table><tbody>";
  out << "<tr><td>Σ counts</td><td>" << fmtNum(report.power.sumCounts, 6) << "</td></tr>";
  out << "<tr><td>峰值</td><td>" << fmtNum(report.power.peakCounts, 6) << "</td></tr>";
  if (report.power.calibrated) {
    out << "<tr><td>功率 / W</td><td>" << fmtNum(report.power.power_W, 6) << "</td></tr>";
  } else {
    out << "<tr><td>功率</td><td>未标定</td></tr>";
  }
  out << "</tbody></table>";

  out << "<footer>Beam Analyzer · 概念实现 / 非正式计量证书 · 可打印后另存为 PDF</footer>";
  out << "</body></html>\n";
  return static_cast<bool>(out);
}

bool exportFrameAnalysisPackage(const std::string& pathPrefix,
                                const FrameAnalysisReport& report) {
  const bool md = exportFrameAnalysisMarkdown(pathPrefix + "_report.md", report);
  const bool html = exportFrameAnalysisHtml(pathPrefix + "_report.html", report);
  const bool json = exportMetricsJson(pathPrefix + "_metrics.json", report.iso, report.meta);
  return md && html && json;
}

}  // namespace beam::io

