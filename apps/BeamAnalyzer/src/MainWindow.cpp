#include "MainWindow.hpp"

#include "M2ScanDialog.hpp"
#include "ProfilePlot.hpp"

#include "beam/io/CsvImporter.hpp"
#include "beam/io/ResultExporter.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include <cmath>

namespace beam::ui {
namespace {

QString fmt(double v, int digits = 4) {
  return QString::number(v, 'g', digits);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Beam Analyzer — Phase 2"));
  resize(1280, 820);
  rebuildUi();
  simCamera_ = beam::camera::createSimulatedCamera(160, 120, 5.0);
  loadSettings();
}

MainWindow::~MainWindow() {
  saveSettings();
}

void MainWindow::rebuildUi() {
  auto* tool = addToolBar(QStringLiteral("主工具栏"));
  tool->setMovable(false);

  auto* openAct = tool->addAction(QStringLiteral("打开 CSV…"));
  connect(openAct, &QAction::triggered, this, &MainWindow::openCsv);

  auto* simAct = tool->addAction(QStringLiteral("模拟帧"));
  connect(simAct, &QAction::triggered, this, &MainWindow::loadSimulatedFrame);

  tool->addSeparator();
  exportJsonAction_ = tool->addAction(QStringLiteral("导出 JSON…"));
  connect(exportJsonAction_, &QAction::triggered, this, &MainWindow::exportJson);
  exportCsvAction_ = tool->addAction(QStringLiteral("导出 CSV…"));
  connect(exportCsvAction_, &QAction::triggered, this, &MainWindow::exportCsv);
  auto* exportPowerAct = tool->addAction(QStringLiteral("导出功率…"));
  connect(exportPowerAct, &QAction::triggered, this, &MainWindow::exportPower);
  exportJsonAction_->setEnabled(false);
  exportCsvAction_->setEnabled(false);

  tool->addSeparator();
  roiModeAction_ = tool->addAction(QStringLiteral("框选 ROI"));
  roiModeAction_->setCheckable(true);
  connect(roiModeAction_, &QAction::toggled, this, &MainWindow::onRoiModeToggled);
  auto* clearRoiAct = tool->addAction(QStringLiteral("清除 ROI"));
  connect(clearRoiAct, &QAction::triggered, this, &MainWindow::onClearRoi);

  tool->addSeparator();
  auto* m2Act = tool->addAction(QStringLiteral("M² 扫描…"));
  connect(m2Act, &QAction::triggered, this, &MainWindow::openM2Scan);

  tool->addSeparator();
  tool->addWidget(new QLabel(QStringLiteral("  色图: ")));
  colorMapBox_ = new QComboBox;
  colorMapBox_->addItem(QStringLiteral("假彩色"), static_cast<int>(ColorMap::FalseColor));
  colorMapBox_->addItem(QStringLiteral("灰度"), static_cast<int>(ColorMap::Grayscale));
  connect(colorMapBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::onColorMapChanged);
  tool->addWidget(colorMapBox_);

  auto* mainSplit = new QSplitter(Qt::Horizontal);

  auto* leftSplit = new QSplitter(Qt::Vertical);
  imageView_ = new ImageView;
  connect(imageView_, &ImageView::roiChanged, this, &MainWindow::onRoiChanged);
  connect(imageView_, &ImageView::crosshairChanged, this, &MainWindow::onCrosshairChanged);

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setWidget(imageView_);
  leftSplit->addWidget(scroll);

  auto* profiles = new QWidget;
  auto* profilesLayout = new QHBoxLayout(profiles);
  profilesLayout->setContentsMargins(0, 0, 0, 0);
  hProfilePlot_ = new ProfilePlot;
  vProfilePlot_ = new ProfilePlot;
  profilesLayout->addWidget(hProfilePlot_);
  profilesLayout->addWidget(vProfilePlot_);
  leftSplit->addWidget(profiles);
  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 1);
  mainSplit->addWidget(leftSplit);

  auto* right = new QWidget;
  auto* rightLayout = new QVBoxLayout(right);

  auto* toolsBox = new QGroupBox(QStringLiteral("分析工具"));
  auto* toolsForm = new QFormLayout(toolsBox);

  useRoiCheck_ = new QCheckBox(QStringLiteral("启用 ROI"));
  connect(useRoiCheck_, &QCheckBox::toggled, this, [this](bool) { onRecompute(); });
  toolsForm->addRow(useRoiCheck_);

  backgroundBox_ = new QComboBox;
  backgroundBox_->addItem(QStringLiteral("无"), static_cast<int>(beam::core::BackgroundMode::None));
  backgroundBox_->addItem(QStringLiteral("常数"), static_cast<int>(beam::core::BackgroundMode::Constant));
  backgroundBox_->addItem(QStringLiteral("边框均值"),
                          static_cast<int>(beam::core::BackgroundMode::BorderMean));
  backgroundBox_->addItem(QStringLiteral("背景 ROI 均值"),
                          static_cast<int>(beam::core::BackgroundMode::RoiMean));
  connect(backgroundBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::onBackgroundModeChanged);
  toolsForm->addRow(QStringLiteral("背景"), backgroundBox_);

  bgConstantSpin_ = new QDoubleSpinBox;
  bgConstantSpin_->setRange(-1e9, 1e9);
  bgConstantSpin_->setDecimals(4);
  bgConstantSpin_->setEnabled(false);
  connect(bgConstantSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { onRecompute(); });
  toolsForm->addRow(QStringLiteral("常数背景"), bgConstantSpin_);

  countsToWattsSpin_ = new QDoubleSpinBox;
  countsToWattsSpin_->setRange(0.0, 1e3);
  countsToWattsSpin_->setDecimals(10);
  countsToWattsSpin_->setSingleStep(1e-6);
  countsToWattsSpin_->setSpecialValueText(QStringLiteral("未标定"));
  connect(countsToWattsSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCalibrationChanged);
  toolsForm->addRow(QStringLiteral("counts→W"), countsToWattsSpin_);

  exposureSpin_ = new QDoubleSpinBox;
  exposureSpin_->setRange(1e-9, 1e3);
  exposureSpin_->setDecimals(6);
  exposureSpin_->setValue(1.0);
  exposureSpin_->setSuffix(QStringLiteral(" s"));
  connect(exposureSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCalibrationChanged);
  toolsForm->addRow(QStringLiteral("曝光"), exposureSpin_);

  wavelengthSpin_ = new QDoubleSpinBox;
  wavelengthSpin_->setRange(1.0, 1e6);
  wavelengthSpin_->setDecimals(3);
  wavelengthSpin_->setValue(1064.0);
  wavelengthSpin_->setSuffix(QStringLiteral(" nm"));
  connect(wavelengthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCalibrationChanged);
  toolsForm->addRow(QStringLiteral("波长"), wavelengthSpin_);

  auto* exportProfRow = new QHBoxLayout;
  auto* exportH = new QPushButton(QStringLiteral("导出水平剖面"));
  auto* exportV = new QPushButton(QStringLiteral("导出垂直剖面"));
  connect(exportH, &QPushButton::clicked, this, &MainWindow::exportHProfile);
  connect(exportV, &QPushButton::clicked, this, &MainWindow::exportVProfile);
  exportProfRow->addWidget(exportH);
  exportProfRow->addWidget(exportV);
  toolsForm->addRow(exportProfRow);

  rightLayout->addWidget(toolsBox);

  rightLayout->addWidget(new QLabel(QStringLiteral("ISO 11146 / 功率指标")));
  metricsView_ = new QTextEdit;
  metricsView_->setReadOnly(true);
  metricsView_->setFont(QFont(QStringLiteral("Consolas"), 10));
  metricsView_->setPlainText(QStringLiteral("尚无数据。点击图像设置剖面十字线。"));
  rightLayout->addWidget(metricsView_, 1);

  auto* hint = new QLabel(
      QStringLiteral("Phase 2：功率标定占位、线剖面、ROI、背景扣除、M² 扫描。\n"
                     "ICamera 仍为 Null/Simulated；无真实 SDK。\n"
                     "框选 ROI 后勾选「启用 ROI」；单击图像更新剖面。"));
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color: #666;"));
  rightLayout->addWidget(hint);

  mainSplit->addWidget(right);
  mainSplit->setStretchFactor(0, 3);
  mainSplit->setStretchFactor(1, 1);
  setCentralWidget(mainSplit);

  statusLabel_ = new QLabel(QStringLiteral("就绪"));
  statusBar()->addWidget(statusLabel_, 1);
}

beam::core::AnalysisPrep MainWindow::currentPrep() const {
  beam::core::AnalysisPrep prep;
  prep.useRoi = useRoiCheck_->isChecked() && roi_.has_value() && !roi_->empty();
  if (roi_) {
    prep.roi = *roi_;
  }
  prep.background.mode = static_cast<beam::core::BackgroundMode>(
      backgroundBox_->currentData().toInt());
  prep.background.constantValue = bgConstantSpin_->value();
  prep.background.clipNegative = true;
  // Background ROI mean: use a thin strip outside analysis ROI if available;
  // otherwise fall back to full-frame border estimate via BorderMean semantics
  // when user picks RoiMean without a dedicated bg ROI — use border of full frame.
  if (prep.background.mode == beam::core::BackgroundMode::RoiMean) {
    if (roi_ && !roi_->empty() && rawFrame_) {
      // Use left 2-column strip of the full frame as placeholder bg ROI
      prep.background.backgroundRoi = {
          0, 0, std::min(1, static_cast<int>(rawFrame_->width) - 1),
          static_cast<int>(rawFrame_->height) - 1};
    } else if (rawFrame_) {
      prep.background.backgroundRoi = {
          0, 0, std::min(1, static_cast<int>(rawFrame_->width) - 1),
          static_cast<int>(rawFrame_->height) - 1};
    }
  }
  return prep;
}

beam::core::PowerCalibration MainWindow::currentCalibration() const {
  beam::core::PowerCalibration cal;
  cal.countsToWatts = countsToWattsSpin_->value();
  cal.exposure_s = exposureSpin_->value();
  cal.wavelength_nm = wavelengthSpin_->value();
  return cal;
}

void MainWindow::openCsv() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开光束帧 CSV"), QString(),
      QStringLiteral("CSV (*.csv);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  beam::io::CsvImportOptions opt;
  opt.pixelSizeX_um = 5.0;
  opt.pixelSizeY_um = 5.0;
  const auto result = beam::io::loadFrameCsv(path.toStdString(), opt);
  if (!result.ok) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QString::fromStdString(result.message));
    return;
  }
  applyFrame(result.frame, path);
}

void MainWindow::loadSimulatedFrame() {
  if (!simCamera_->isOpen()) {
    simCamera_->open();
  }
  const auto cap = simCamera_->grab();
  if (!cap.ok) {
    QMessageBox::warning(this, QStringLiteral("模拟相机"),
                         QString::fromStdString(cap.message));
    return;
  }
  applyFrame(cap.frame, QStringLiteral("<simulated>"));
}

void MainWindow::applyFrame(beam::core::BeamFrame frame, const QString& sourceLabel) {
  rawFrame_ = std::move(frame);
  sourcePath_ = sourceLabel;
  crossX_ = static_cast<int>(rawFrame_->width / 2);
  crossY_ = static_cast<int>(rawFrame_->height / 2);
  imageView_->setCrosshair(crossX_, crossY_);
  exportJsonAction_->setEnabled(true);
  exportCsvAction_->setEnabled(true);
  refreshAnalysis();
  statusLabel_->setText(
      QStringLiteral("已加载 %1×%2 — %3")
          .arg(rawFrame_->width)
          .arg(rawFrame_->height)
          .arg(sourceLabel));
}

void MainWindow::refreshAnalysis() {
  if (!rawFrame_) {
    return;
  }
  analysisFrame_ = beam::core::prepareAnalysisFrame(*rawFrame_, currentPrep());
  metrics_ = beam::core::computeIso11146(*analysisFrame_);
  power_ = beam::core::computePowerMetrics(*analysisFrame_, currentCalibration());
  refreshImage();
  refreshProfiles();
  refreshMetricsPanel();
}

void MainWindow::refreshImage() {
  if (!rawFrame_) {
    return;
  }
  // Display analysis frame when ROI/bg active so the view matches metrics;
  // otherwise show raw.
  const beam::core::BeamFrame& shown =
      (analysisFrame_ && (useRoiCheck_->isChecked() ||
                          backgroundBox_->currentData().toInt() !=
                              static_cast<int>(beam::core::BackgroundMode::None)))
          ? *analysisFrame_
          : *rawFrame_;

  double mn = 0;
  double mx = 0;
  const QImage img = frameToImage(shown, colorMap_, &mn, &mx);
  if (img.isNull()) {
    return;
  }

  const int targetW = std::max(img.width(), 480);
  const int targetH = std::max(img.height(), 360);
  const QImage scaled =
      img.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::FastTransformation);

  // When cropped, overlay coords differ — show raw for ROI drawing consistency.
  const QImage displayBase = frameToImage(*rawFrame_, colorMap_, &mn, &mx);
  const QImage displayScaled = displayBase.scaled(
      std::max(displayBase.width(), 480), std::max(displayBase.height(), 360),
      Qt::KeepAspectRatio, Qt::FastTransformation);
  imageView_->setDisplayImage(displayScaled, static_cast<int>(rawFrame_->width),
                              static_cast<int>(rawFrame_->height));
  if (roi_) {
    imageView_->setRoi(roi_);
  }
  Q_UNUSED(scaled);
}

void MainWindow::refreshProfiles() {
  if (!rawFrame_ || crossX_ < 0 || crossY_ < 0) {
    hProfilePlot_->clearProfile();
    vProfilePlot_->clearProfile();
    return;
  }
  // Profiles from analysis frame when same size; else from raw with crosshair.
  const beam::core::BeamFrame* src = rawFrame_.operator->();
  int cx = crossX_;
  int cy = crossY_;
  if (analysisFrame_ && analysisFrame_->width == rawFrame_->width &&
      analysisFrame_->height == rawFrame_->height) {
    src = analysisFrame_.operator->();
  } else if (useRoiCheck_->isChecked() && roi_ && analysisFrame_) {
    // Map crosshair into cropped frame if inside ROI
    if (roi_->contains(crossX_, crossY_)) {
      src = analysisFrame_.operator->();
      cx = crossX_ - roi_->x0;
      cy = crossY_ - roi_->y0;
    }
  }

  const auto cross = beam::core::extractCrosshair(*src, cx, cy);
  hProfile_ = cross.horizontal;
  vProfile_ = cross.vertical;
  hProfilePlot_->setProfile(hProfile_, QStringLiteral("水平剖面 (行 %1)").arg(cy));
  vProfilePlot_->setProfile(vProfile_, QStringLiteral("垂直剖面 (列 %1)").arg(cx));
  imageView_->setCrosshair(crossX_, crossY_);
}

void MainWindow::refreshMetricsPanel() {
  if (!metrics_ || !metrics_->valid) {
    metricsView_->setPlainText(QStringLiteral("指标无效（空帧或零功率）。"));
    return;
  }
  const auto& m = *metrics_;
  const double oriDeg = m.orientation_rad * 180.0 / 3.14159265358979323846;
  QString text;
  text += QStringLiteral("来源: %1\n\n").arg(sourcePath_);
  text += QStringLiteral("— ISO 11146 —\n");
  text += QStringLiteral("总功率 (a.u.): %1\n").arg(fmt(m.totalPower, 6));
  text += QStringLiteral("质心 X (µm):   %1\n").arg(fmt(m.centroidX_um));
  text += QStringLiteral("质心 Y (µm):   %1\n").arg(fmt(m.centroidY_um));
  text += QStringLiteral("\n实验室坐标束宽 dσ=4σ\n");
  text += QStringLiteral("  Dx (µm):     %1\n").arg(fmt(m.diameterX_um));
  text += QStringLiteral("  Dy (µm):     %1\n").arg(fmt(m.diameterY_um));
  text += QStringLiteral("\n主轴束宽\n");
  text += QStringLiteral("  D_major:     %1\n").arg(fmt(m.diameterMajor_um));
  text += QStringLiteral("  D_minor:     %1\n").arg(fmt(m.diameterMinor_um));
  text += QStringLiteral("  方位角 (°):  %1\n").arg(fmt(oriDeg));
  text += QStringLiteral("  椭圆度 ε:    %1\n").arg(fmt(m.ellipticity));

  if (power_ && power_->valid) {
    text += QStringLiteral("\n— 功率 / 能量 —\n");
    text += QStringLiteral("Σ counts:     %1\n").arg(fmt(power_->sumCounts, 6));
    text += QStringLiteral("峰值 counts:  %1\n").arg(fmt(power_->peakCounts, 6));
    text += QStringLiteral("均值 counts:  %1\n").arg(fmt(power_->meanCounts, 6));
    if (power_->calibrated) {
      text += QStringLiteral("功率 (W):     %1\n").arg(fmt(power_->power_W, 6));
      text += QStringLiteral("能量 (J):     %1\n").arg(fmt(power_->energy_J, 6));
    } else {
      text += QStringLiteral("功率:         未标定（设置 counts→W）\n");
    }
  }

  if (roi_ && useRoiCheck_->isChecked()) {
    text += QStringLiteral("\nROI: [%1,%2]–[%3,%4]\n")
                .arg(roi_->x0)
                .arg(roi_->y0)
                .arg(roi_->x1)
                .arg(roi_->y1);
  }
  if (crossX_ >= 0) {
    text += QStringLiteral("剖面十字线: (%1, %2)\n").arg(crossX_).arg(crossY_);
  }

  text += QStringLiteral("\n像素尺寸: %1 × %2 µm")
              .arg(fmt(rawFrame_->pixelSizeX_um))
              .arg(fmt(rawFrame_->pixelSizeY_um));
  metricsView_->setPlainText(text);
}

bool MainWindow::ensureMetrics() const {
  return rawFrame_.has_value() && metrics_.has_value() && metrics_->valid;
}

void MainWindow::exportJson() {
  if (!ensureMetrics()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 JSON"), QStringLiteral("beam_metrics.json"),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta;
  meta.sourcePath = sourcePath_.toStdString();
  meta.note = "Beam Analyzer Phase 2";
  if (!beam::io::exportMetricsJson(path.toStdString(), *metrics_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出 JSON: %1").arg(path));
}

void MainWindow::exportCsv() {
  if (!ensureMetrics()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 CSV"), QStringLiteral("beam_metrics.csv"),
      QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta;
  meta.sourcePath = sourcePath_.toStdString();
  meta.note = "Beam Analyzer Phase 2";
  if (!beam::io::exportMetricsCsv(path.toStdString(), *metrics_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出 CSV: %1").arg(path));
}

void MainWindow::exportPower() {
  if (!power_ || !power_->valid) {
    QMessageBox::information(this, QStringLiteral("导出功率"),
                             QStringLiteral("无有效功率数据。"));
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出功率 JSON"), QStringLiteral("beam_power.json"),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta;
  meta.sourcePath = sourcePath_.toStdString();
  meta.note = "Beam Analyzer Phase 2 power";
  if (!beam::io::exportPowerJson(path.toStdString(), *power_, currentCalibration(), meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出功率: %1").arg(path));
}

void MainWindow::exportHProfile() {
  if (!hProfile_.valid) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出水平剖面"), QStringLiteral("profile_h.csv"),
      QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta{sourcePath_.toStdString(), "horizontal profile"};
  if (!beam::io::exportProfileCsv(path.toStdString(), hProfile_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出水平剖面: %1").arg(path));
}

void MainWindow::exportVProfile() {
  if (!vProfile_.valid) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出垂直剖面"), QStringLiteral("profile_v.csv"),
      QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta{sourcePath_.toStdString(), "vertical profile"};
  if (!beam::io::exportProfileCsv(path.toStdString(), vProfile_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出垂直剖面: %1").arg(path));
}

void MainWindow::onColorMapChanged(int index) {
  colorMap_ = static_cast<ColorMap>(colorMapBox_->itemData(index).toInt());
  refreshImage();
}

void MainWindow::onRoiModeToggled(bool on) {
  imageView_->setInteractionMode(on ? ImageView::InteractionMode::DrawRoi
                                     : ImageView::InteractionMode::None);
  statusLabel_->setText(on ? QStringLiteral("拖拽框选 ROI…") : QStringLiteral("就绪"));
}

void MainWindow::onClearRoi() {
  roi_.reset();
  imageView_->setRoi(std::nullopt);
  useRoiCheck_->setChecked(false);
  if (roiModeAction_->isChecked()) {
    roiModeAction_->setChecked(false);
  }
  onRecompute();
}

void MainWindow::onRoiChanged(beam::core::RectRoi roi) {
  roi_ = roi;
  useRoiCheck_->setChecked(true);
  if (roiModeAction_->isChecked()) {
    roiModeAction_->setChecked(false);
  }
  onRecompute();
}

void MainWindow::onCrosshairChanged(int x, int y) {
  crossX_ = x;
  crossY_ = y;
  refreshProfiles();
  refreshMetricsPanel();
}

void MainWindow::onBackgroundModeChanged(int) {
  const auto mode = static_cast<beam::core::BackgroundMode>(
      backgroundBox_->currentData().toInt());
  bgConstantSpin_->setEnabled(mode == beam::core::BackgroundMode::Constant);
  onRecompute();
}

void MainWindow::onCalibrationChanged() {
  if (!analysisFrame_) {
    return;
  }
  power_ = beam::core::computePowerMetrics(*analysisFrame_, currentCalibration());
  refreshMetricsPanel();
}

void MainWindow::onRecompute() {
  refreshAnalysis();
}

void MainWindow::openM2Scan() {
  M2ScanDialog dlg(this);
  dlg.setDefaultWavelength(wavelengthSpin_->value());
  if (metrics_ && metrics_->valid) {
    dlg.seedFromCurrentFrame(metrics_->diameterX_um, metrics_->diameterY_um, sourcePath_);
  }
  dlg.exec();
}

void MainWindow::saveSettings() {
  QSettings s(QStringLiteral("BeamAnalyzer"), QStringLiteral("BeamAnalyzer"));
  s.setValue(QStringLiteral("colorMap"), static_cast<int>(colorMap_));
  s.setValue(QStringLiteral("backgroundMode"), backgroundBox_->currentIndex());
  s.setValue(QStringLiteral("bgConstant"), bgConstantSpin_->value());
  s.setValue(QStringLiteral("countsToWatts"), countsToWattsSpin_->value());
  s.setValue(QStringLiteral("exposure_s"), exposureSpin_->value());
  s.setValue(QStringLiteral("wavelength_nm"), wavelengthSpin_->value());
  s.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::loadSettings() {
  QSettings s(QStringLiteral("BeamAnalyzer"), QStringLiteral("BeamAnalyzer"));
  const int cm = s.value(QStringLiteral("colorMap"), static_cast<int>(ColorMap::FalseColor)).toInt();
  const int cmIndex = colorMapBox_->findData(cm);
  if (cmIndex >= 0) {
    colorMapBox_->setCurrentIndex(cmIndex);
  }
  backgroundBox_->setCurrentIndex(s.value(QStringLiteral("backgroundMode"), 0).toInt());
  bgConstantSpin_->setValue(s.value(QStringLiteral("bgConstant"), 0.0).toDouble());
  countsToWattsSpin_->setValue(s.value(QStringLiteral("countsToWatts"), 0.0).toDouble());
  exposureSpin_->setValue(s.value(QStringLiteral("exposure_s"), 1.0).toDouble());
  wavelengthSpin_->setValue(s.value(QStringLiteral("wavelength_nm"), 1064.0).toDouble());
  restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
}

}  // namespace beam::ui
