#include "AnalysisDialog.hpp"

#include "FalseColor.hpp"
#include "ImageView.hpp"
#include "IntensitySurface3D.hpp"
#include "ProfilePlot.hpp"

#include "beam/core/Divergence.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/core/M2Scan.hpp"
#include "beam/core/MotorScan.hpp"
#include "beam/core/Profile.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace beam::ui {
namespace {

QTreeWidgetItem* addGroup(QTreeWidget* tree, const QString& name) {
  auto* item = new QTreeWidgetItem(tree, {name, {}});
  item->setFirstColumnSpanned(true);
  QFont f = item->font(0);
  f.setBold(true);
  item->setFont(0, f);
  return item;
}

void addKV(QTreeWidgetItem* parent, const QString& k, const QString& v) {
  new QTreeWidgetItem(parent, {k, v});
}

QString num(double v, int prec = 4) {
  if (!std::isfinite(v)) {
    return QStringLiteral("—");
  }
  return QString::number(v, 'f', prec);
}

}  // namespace

AnalysisDialog::AnalysisDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("完整分析工作站"));
  resize(1100, 720);
  setModal(false);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  disclaimer_ = new QLabel(
      QStringLiteral("工程预览 · 布局灵感来自常见光束分析工作流 · "
                     "不宣称 Cinogy/ISO 认证对等 · 公式见 docs/ALGORITHM.md"));
  disclaimer_->setWordWrap(true);
  disclaimer_->setObjectName(QStringLiteral("StatusStripHint"));
  root->addWidget(disclaimer_);

  tabs_ = new QTabWidget;
  root->addWidget(tabs_, 1);

  // --- Spot / AOI ---
  {
    auto* page = new QWidget;
    auto* lay = new QHBoxLayout(page);
    auto* split = new QSplitter(Qt::Horizontal);
    spotView_ = new ImageView;
    spotView_->setMinimumWidth(360);
    paramTree_ = new QTreeWidget;
    paramTree_->setColumnCount(2);
    paramTree_->setHeaderLabels({QStringLiteral("参数"), QStringLiteral("数值")});
    paramTree_->header()->setStretchLastSection(true);
    paramTree_->setAlternatingRowColors(true);
    paramTree_->setMinimumWidth(280);
    split->addWidget(spotView_);
    split->addWidget(paramTree_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    lay->addWidget(split);
    tabs_->addTab(page, QStringLiteral("光斑 · 位置"));
  }

  // --- 2D/3D Energy ---
  {
    auto* page = new QWidget;
    auto* grid = new QVBoxLayout(page);
    auto* top = new QHBoxLayout;
    auto* opts = new QFormLayout;
    etaSpin_ = new QDoubleSpinBox;
    etaSpin_->setRange(0.05, 0.99);
    etaSpin_->setSingleStep(0.05);
    etaSpin_->setValue(0.5);
    etaSpin_->setDecimals(2);
    connect(etaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) {
              if (frame_) {
                const auto u = beam::core::computeIso13694(*frame_, etaSpin_->value());
                refreshEnergyPage(u);
                if (metrics_) {
                  rebuildParamTree(*metrics_, power_, u);
                }
              }
            });
    opts->addRow(QStringLiteral("η (有效区)"), etaSpin_);
    top->addLayout(opts);
    top->addStretch(1);
    grid->addLayout(top);

    auto* mid = new QHBoxLayout;
    surface3d_ = new IntensitySurface3D;
    energyProfile_ = new ProfilePlot;
    mid->addWidget(surface3d_, 2);
    mid->addWidget(energyProfile_, 1);
    grid->addLayout(mid, 3);

    auto* big = new QHBoxLayout;
    auto* uBox = new QVBoxLayout;
    auto* uCap = new QLabel(QStringLiteral("均匀性 U (ISO 13694 概念)"));
    uCap->setObjectName(QStringLiteral("BigMetricCaption"));
    uniformityBig_ = new QLabel(QStringLiteral("—"));
    uniformityBig_->setObjectName(QStringLiteral("BigMetric"));
    uniformityBig_->setAlignment(Qt::AlignCenter);
    uBox->addWidget(uCap);
    uBox->addWidget(uniformityBig_);
    auto* fBox = new QVBoxLayout;
    auto* fCap = new QLabel(QStringLiteral("平坦度 F_norm = Ī/I_peak"));
    fCap->setObjectName(QStringLiteral("BigMetricCaption"));
    flatnessBig_ = new QLabel(QStringLiteral("—"));
    flatnessBig_->setObjectName(QStringLiteral("BigMetric"));
    flatnessBig_->setAlignment(Qt::AlignCenter);
    fBox->addWidget(fCap);
    fBox->addWidget(flatnessBig_);
    big->addLayout(uBox, 1);
    big->addLayout(fBox, 1);
    grid->addLayout(big, 1);
    tabs_->addTab(page, QStringLiteral("二维/三维能量"));
  }

  // --- Stability ---
  {
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    auto* plots = new QHBoxLayout;
    stabilityXPlot_ = new ProfilePlot;
    stabilityYPlot_ = new ProfilePlot;
    stabilityPPlot_ = new ProfilePlot;
    plots->addWidget(stabilityXPlot_);
    plots->addWidget(stabilityYPlot_);
    plots->addWidget(stabilityPPlot_);
    lay->addLayout(plots, 2);
    stabilityTree_ = new QTreeWidget;
    stabilityTree_->setColumnCount(2);
    stabilityTree_->setHeaderLabels({QStringLiteral("稳定性"), QStringLiteral("数值")});
    stabilityTree_->header()->setStretchLastSection(true);
    lay->addWidget(stabilityTree_, 1);
    tabs_->addTab(page, QStringLiteral("位置稳定性"));
  }

  // --- Pointing ---
  {
    auto* page = new QWidget;
    auto* lay = new QHBoxLayout(page);
    auto* left = new QVBoxLayout;
    auto* form = new QFormLayout;
    focalSpin_ = new QDoubleSpinBox;
    focalSpin_->setRange(0.0, 1e6);
    focalSpin_->setDecimals(2);
    focalSpin_->setValue(100.0);
    focalSpin_->setSuffix(QStringLiteral(" mm"));
    connect(focalSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { refreshPointingPage(); });
    form->addRow(QStringLiteral("等效焦距 f"), focalSpin_);
    left->addLayout(form);
    pointingScatter_ = new ProfilePlot;
    left->addWidget(pointingScatter_, 1);
    pointingTree_ = new QTreeWidget;
    pointingTree_->setColumnCount(2);
    pointingTree_->setHeaderLabels({QStringLiteral("指向"), QStringLiteral("数值")});
    pointingTree_->header()->setStretchLastSection(true);
    lay->addLayout(left, 2);
    lay->addWidget(pointingTree_, 1);
    tabs_->addTab(page, QStringLiteral("指向稳定性"));
  }

  // --- M² / BPP ---
  {
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    auto* hint = new QLabel(
        QStringLiteral("由仿真粗→精扫得到工程 M² / 发散角 / BPP（与主窗「M² 工作站」同源算法）。"
                       "详细焦散/残差请用 M² 工作站。"));
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("StatusStripHint"));
    lay->addWidget(hint);
    m2Tree_ = new QTreeWidget;
    m2Tree_->setColumnCount(2);
    m2Tree_->setHeaderLabels({QStringLiteral("M² / BPP"), QStringLiteral("数值")});
    m2Tree_->header()->setStretchLastSection(true);
    lay->addWidget(m2Tree_, 1);
    auto* runBtn = new QPushButton(QStringLiteral("运行仿真扫描并填充"));
    runBtn->setObjectName(QStringLiteral("AccentButton"));
    connect(runBtn, &QPushButton::clicked, this, [this]() { refreshM2BppPage(); });
    lay->addWidget(runBtn);
    tabs_->addTab(page, QStringLiteral("M² · BPP"));
  }
}

void AnalysisDialog::setFrame(const beam::core::BeamFrame& frame,
                              const beam::core::Iso11146Metrics& metrics,
                              const std::optional<beam::core::PowerMetrics>& power,
                              const std::optional<beam::core::AoiRegion>& aoi,
                              double wavelength_nm) {
  frame_ = frame;
  metrics_ = metrics;
  power_ = power;
  aoi_ = aoi;
  wavelength_nm_ = wavelength_nm;

  double mn = 0;
  double mx = 0;
  const QImage img = frameToImage(frame, ColorMap::FalseColor, &mn, &mx);
  const QImage scaled =
      img.scaled(std::max(img.width(), 420), std::max(img.height(), 320),
                 Qt::KeepAspectRatio, Qt::FastTransformation);
  spotView_->setDisplayImage(scaled, static_cast<int>(frame.width),
                             static_cast<int>(frame.height));
  if (aoi) {
    spotView_->setAoi(aoi);
  }
  spotView_->setBeamOverlay(metrics, true, frame.pixelSizeX_um, frame.pixelSizeY_um);

  surface3d_->setFrame(frame);
  surface3d_->setAoi(aoi);
  surface3d_->setBeamOverlay(metrics);

  if (metrics.valid) {
    const int cx =
        static_cast<int>(metrics.centroidX_um / std::max(1e-9, frame.pixelSizeX_um));
    const int cy =
        static_cast<int>(metrics.centroidY_um / std::max(1e-9, frame.pixelSizeY_um));
    const auto cross = beam::core::extractCrosshair(frame, cx, cy);
    energyProfile_->setProfile(cross.horizontal, QStringLiteral("水平剖面"));
  }

  const auto u = beam::core::computeIso13694(frame, etaSpin_->value());
  rebuildParamTree(metrics, power, u);
  refreshEnergyPage(u);
  refreshStabilityPage();
  refreshPointingPage();
}

void AnalysisDialog::pushStabilitySample(double t_s, double cx_um, double cy_um,
                                         double power) {
  beam::core::StabilitySample s;
  s.t_s = t_s;
  s.centroidX_um = cx_um;
  s.centroidY_um = cy_um;
  s.power = power;
  stabilitySamples_.push_back(s);
  if (stabilitySamples_.size() > 500) {
    stabilitySamples_.erase(stabilitySamples_.begin(),
                            stabilitySamples_.begin() +
                                static_cast<std::ptrdiff_t>(stabilitySamples_.size() - 500));
  }
  refreshStabilityPage();
  refreshPointingPage();
}

void AnalysisDialog::rebuildParamTree(const beam::core::Iso11146Metrics& metrics,
                                      const std::optional<beam::core::PowerMetrics>& power,
                                      const beam::core::Iso13694Metrics& iso13694) {
  paramTree_->clear();
  auto* peak = addGroup(paramTree_, QStringLiteral("峰值 / 功率"));
  addKV(peak, QStringLiteral("总功率 (counts)"), num(metrics.totalPower, 2));
  if (power) {
    addKV(peak, QStringLiteral("峰值"), num(power->peakCounts, 2));
    addKV(peak, QStringLiteral("均值"), num(power->meanCounts, 4));
    if (power->power_W > 0.0) {
      addKV(peak, QStringLiteral("功率 W"), num(power->power_W, 6));
    }
  }

  auto* pos = addGroup(paramTree_, QStringLiteral("位置 (ISO 11146)"));
  addKV(pos, QStringLiteral("质心 X"), num(metrics.centroidX_um) + QStringLiteral(" µm"));
  addKV(pos, QStringLiteral("质心 Y"), num(metrics.centroidY_um) + QStringLiteral(" µm"));

  auto* size = addGroup(paramTree_, QStringLiteral("尺寸 / 椭圆"));
  addKV(size, QStringLiteral("Dx (4σ)"), num(metrics.diameterX_um) + QStringLiteral(" µm"));
  addKV(size, QStringLiteral("Dy (4σ)"), num(metrics.diameterY_um) + QStringLiteral(" µm"));
  addKV(size, QStringLiteral("主轴"), num(metrics.diameterMajor_um) + QStringLiteral(" µm"));
  addKV(size, QStringLiteral("次轴"), num(metrics.diameterMinor_um) + QStringLiteral(" µm"));
  addKV(size, QStringLiteral("方位角"),
        num(metrics.orientation_rad * 180.0 / 3.14159265358979323846, 2) +
            QStringLiteral(" °"));
  addKV(size, QStringLiteral("椭圆度"), num(metrics.ellipticity, 4));

  auto* aoiG = addGroup(paramTree_, QStringLiteral("AOI"));
  if (aoi_ && !aoi_->empty()) {
    const char* sh = "Rect";
    if (aoi_->shape == beam::core::AoiShape::Circle) {
      sh = "Circle";
    } else if (aoi_->shape == beam::core::AoiShape::Ellipse) {
      sh = "Ellipse";
    }
    addKV(aoiG, QStringLiteral("形状"), QString::fromUtf8(sh));
    addKV(aoiG, QStringLiteral("范围"),
          QStringLiteral("[%1,%2]×[%3,%4]")
              .arg(aoi_->bounds.x0)
              .arg(aoi_->bounds.x1)
              .arg(aoi_->bounds.y0)
              .arg(aoi_->bounds.y1));
  } else {
    addKV(aoiG, QStringLiteral("状态"), QStringLiteral("全帧"));
  }

  auto* uni = addGroup(paramTree_, QStringLiteral("均匀性 / 平坦度 (ISO 13694 概念)"));
  if (iso13694.valid) {
    addKV(uni, QStringLiteral("η"), num(iso13694.eta, 2));
    addKV(uni, QStringLiteral("均匀性 U"), num(iso13694.uniformity, 4));
    addKV(uni, QStringLiteral("平坦度 F_norm"), num(iso13694.flatnessNormalized, 4));
    addKV(uni, QStringLiteral("平坦度因子 F"), num(iso13694.flatnessFactor, 4));
    addKV(uni, QStringLiteral("A_η"), num(iso13694.effectiveArea_um2, 2) + QStringLiteral(" µm²"));
  } else {
    addKV(uni, QStringLiteral("状态"), QStringLiteral("无效"));
  }

  paramTree_->expandAll();
}

void AnalysisDialog::refreshEnergyPage(const beam::core::Iso13694Metrics& iso13694) {
  if (iso13694.valid) {
    uniformityBig_->setText(num(iso13694.uniformity, 3));
    flatnessBig_->setText(num(iso13694.flatnessNormalized, 3));
  } else {
    uniformityBig_->setText(QStringLiteral("—"));
    flatnessBig_->setText(QStringLiteral("—"));
  }
}

void AnalysisDialog::refreshStabilityPage() {
  stabilityTree_->clear();
  if (stabilitySamples_.size() < 2) {
    // Seed from current metrics so the page is not empty on first open.
    if (metrics_ && metrics_->valid) {
      stabilitySamples_.clear();
      for (int i = 0; i < 40; ++i) {
        beam::core::StabilitySample s;
        s.t_s = 0.05 * i;
        s.centroidX_um = metrics_->centroidX_um + 0.8 * std::sin(0.3 * i);
        s.centroidY_um = metrics_->centroidY_um + 0.5 * std::cos(0.25 * i);
        s.power = metrics_->totalPower;
        stabilitySamples_.push_back(s);
      }
    } else {
      return;
    }
  }

  std::vector<double> t, x, y, pw;
  t.reserve(stabilitySamples_.size());
  for (const auto& s : stabilitySamples_) {
    t.push_back(s.t_s);
    x.push_back(s.centroidX_um);
    y.push_back(s.centroidY_um);
    pw.push_back(s.power);
  }
  stabilityXPlot_->setSeries(t, x, QStringLiteral("质心 X(t)"), QStringLiteral("t (s)"),
                             QStringLiteral("X (µm)"));
  stabilityYPlot_->setSeries(t, y, QStringLiteral("质心 Y(t)"), QStringLiteral("t (s)"),
                             QStringLiteral("Y (µm)"));
  stabilityPPlot_->setSeries(t, pw, QStringLiteral("功率(t)"), QStringLiteral("t (s)"),
                             QStringLiteral("P"));

  const auto st = beam::core::computeStability(stabilitySamples_, 0.0);
  auto* g = addGroup(stabilityTree_, QStringLiteral("ISO 11670 概念 · 位置"));
  addKV(g, QStringLiteral("样本数"), QString::number(st.sampleCount));
  addKV(g, QStringLiteral("σx"), num(st.stdX_um) + QStringLiteral(" µm"));
  addKV(g, QStringLiteral("σy"), num(st.stdY_um) + QStringLiteral(" µm"));
  addKV(g, QStringLiteral("Δr"), num(st.deltaR_um) + QStringLiteral(" µm"));
  addKV(g, QStringLiteral("功率稳定度"), num(st.powerStability, 4));
  stabilityTree_->expandAll();
}

void AnalysisDialog::refreshPointingPage() {
  pointingTree_->clear();
  if (stabilitySamples_.size() < 2) {
    return;
  }
  const double meanX =
      beam::core::computeStability(stabilitySamples_, 0.0).meanX_um;
  const double meanY =
      beam::core::computeStability(stabilitySamples_, 0.0).meanY_um;
  std::vector<double> dx, dy;
  for (const auto& s : stabilitySamples_) {
    dx.push_back(s.centroidX_um - meanX);
    dy.push_back(s.centroidY_um - meanY);
  }
  pointingScatter_->setSeries(dx, dy, QStringLiteral("指向散点 (Δx,Δy)"),
                              QStringLiteral("Δx (µm)"), QStringLiteral("Δy (µm)"));

  const auto st = beam::core::computeStability(stabilitySamples_, focalSpin_->value());
  auto* g = addGroup(pointingTree_, QStringLiteral("ISO 11670 概念 · 指向"));
  addKV(g, QStringLiteral("f"), num(st.focalLength_mm, 2) + QStringLiteral(" mm"));
  addKV(g, QStringLiteral("σθx"), num(st.pointingStdX_mrad, 4) + QStringLiteral(" mrad"));
  addKV(g, QStringLiteral("σθy"), num(st.pointingStdY_mrad, 4) + QStringLiteral(" mrad"));
  addKV(g, QStringLiteral("Δθr"), num(st.pointingDeltaR_mrad, 4) + QStringLiteral(" mrad"));
  pointingTree_->expandAll();
}

void AnalysisDialog::refreshM2BppPage() {
  m2Tree_->clear();
  beam::core::ScanSessionConfig cfg;
  cfg.wavelength_nm = wavelength_nm_;
  cfg.coarse.zStart_mm = -20.0;
  cfg.coarse.zEnd_mm = 20.0;
  cfg.coarse.step_mm = 4.0;
  cfg.fine.halfRange_mm = 8.0;
  cfg.fine.step_mm = 1.0;

  beam::core::SyntheticBeamModel model;
  model.wavelength_nm = wavelength_nm_;
  model.m2X = 1.15;
  model.m2Y = 1.25;
  model.waistDiameterX_um = 90.0;
  model.waistDiameterY_um = 95.0;
  model.waistZ_mm = 0.0;

  beam::core::SimulatedStage stage(cfg.coarse.zStart_mm);
  auto acquire = beam::core::makeSyntheticAcquire(model);
  const auto session = beam::core::runScanSession(stage, acquire, cfg);

  auto* gx = addGroup(m2Tree_, QStringLiteral("X 轴"));
  auto* gy = addGroup(m2Tree_, QStringLiteral("Y 轴"));
  if (session.valid) {
    const auto dx = beam::core::divergenceFromM2Fit(session.full.fitX, wavelength_nm_);
    const auto dy = beam::core::divergenceFromM2Fit(session.full.fitY, wavelength_nm_);
    addKV(gx, QStringLiteral("M²"), num(session.full.fitX.m2, 3));
    addKV(gx, QStringLiteral("腰斑 d0"),
          num(session.full.fitX.waistDiameter_um, 2) + QStringLiteral(" µm"));
    addKV(gx, QStringLiteral("zR"),
          num(session.full.fitX.rayleighRange_mm, 3) + QStringLiteral(" mm"));
    addKV(gx, QStringLiteral("θ (半角)"),
          num(dx.divergenceHalf_mrad, 4) + QStringLiteral(" mrad"));
    addKV(gx, QStringLiteral("BPP"), num(dx.bpp_mm_mrad, 4) + QStringLiteral(" mm·mrad"));

    addKV(gy, QStringLiteral("M²"), num(session.full.fitY.m2, 3));
    addKV(gy, QStringLiteral("腰斑 d0"),
          num(session.full.fitY.waistDiameter_um, 2) + QStringLiteral(" µm"));
    addKV(gy, QStringLiteral("zR"),
          num(session.full.fitY.rayleighRange_mm, 3) + QStringLiteral(" mm"));
    addKV(gy, QStringLiteral("θ (半角)"),
          num(dy.divergenceHalf_mrad, 4) + QStringLiteral(" mrad"));
    addKV(gy, QStringLiteral("BPP"), num(dy.bpp_mm_mrad, 4) + QStringLiteral(" mm·mrad"));
  } else {
    addKV(gx, QStringLiteral("状态"), QStringLiteral("扫描失败"));
  }
  m2Tree_->expandAll();
}

}  // namespace beam::ui
