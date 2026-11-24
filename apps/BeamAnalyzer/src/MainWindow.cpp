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
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
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
  setWindowTitle(QStringLiteral("Beam Analyzer — Phase 3"));
  resize(1320, 860);
  beam::camera::registerAllBuiltInBackends();
  rebuildUi();
  refreshCameraList();
  loadSettings();
}

MainWindow::~MainWindow() {
  onCameraDisconnect();
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

  auto* camBox = new QGroupBox(QStringLiteral("相机采集"));
  auto* camForm = new QFormLayout(camBox);

  cameraSourceBox_ = new QComboBox;
  camForm->addRow(QStringLiteral("源"), cameraSourceBox_);

  auto* camBtnRow = new QHBoxLayout;
  cameraConnectBtn_ = new QPushButton(QStringLiteral("连接"));
  cameraDisconnectBtn_ = new QPushButton(QStringLiteral("断开"));
  auto* refreshCamBtn = new QPushButton(QStringLiteral("刷新"));
  connect(cameraConnectBtn_, &QPushButton::clicked, this, &MainWindow::onCameraConnect);
  connect(cameraDisconnectBtn_, &QPushButton::clicked, this, &MainWindow::onCameraDisconnect);
  connect(refreshCamBtn, &QPushButton::clicked, this, &MainWindow::refreshCameraList);
  camBtnRow->addWidget(cameraConnectBtn_);
  camBtnRow->addWidget(cameraDisconnectBtn_);
  camBtnRow->addWidget(refreshCamBtn);
  camForm->addRow(camBtnRow);

  livePreviewCheck_ = new QCheckBox(QStringLiteral("实时预览"));
  connect(livePreviewCheck_, &QCheckBox::toggled, this, &MainWindow::onLivePreviewToggled);
  camForm->addRow(livePreviewCheck_);

  camExposureSpin_ = new QDoubleSpinBox;
  camExposureSpin_->setRange(1.0, 1e7);
  camExposureSpin_->setDecimals(1);
  camExposureSpin_->setValue(1000.0);
  camExposureSpin_->setSuffix(QStringLiteral(" µs"));
  connect(camExposureSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("曝光"), camExposureSpin_);

  camGainSpin_ = new QDoubleSpinBox;
  camGainSpin_->setRange(-20.0, 40.0);
  camGainSpin_->setDecimals(2);
  camGainSpin_->setValue(0.0);
  camGainSpin_->setSuffix(QStringLiteral(" dB"));
  connect(camGainSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("增益"), camGainSpin_);

  camFpsSpin_ = new QDoubleSpinBox;
  camFpsSpin_->setRange(0.1, 120.0);
  camFpsSpin_->setDecimals(1);
  camFpsSpin_->setValue(15.0);
  camFpsSpin_->setSuffix(QStringLiteral(" fps"));
  connect(camFpsSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("目标帧率"), camFpsSpin_);

  triggerModeBox_ = new QComboBox;
  triggerModeBox_->addItem(QStringLiteral("连续 (FreeRun)"),
                           static_cast<int>(beam::camera::TriggerMode::FreeRun));
  triggerModeBox_->addItem(QStringLiteral("软触发 (Software)"),
                           static_cast<int>(beam::camera::TriggerMode::Software));
  triggerModeBox_->addItem(QStringLiteral("硬触发 (Hardware)"),
                           static_cast<int>(beam::camera::TriggerMode::Hardware));
  connect(triggerModeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("触发"), triggerModeBox_);

  softwareTriggerBtn_ = new QPushButton(QStringLiteral("软触发一次"));
  connect(softwareTriggerBtn_, &QPushButton::clicked, this, &MainWindow::onSoftwareTrigger);
  camForm->addRow(softwareTriggerBtn_);

  rightLayout->addWidget(camBox);

  liveTimer_ = new QTimer(this);
  connect(liveTimer_, &QTimer::timeout, this, &MainWindow::onLiveTick);

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
  toolsForm->addRow(QStringLiteral("分析曝光"), exposureSpin_);

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
  metricsView_->setPlainText(QStringLiteral("尚无数据。连接模拟相机或打开 CSV。"));
  rightLayout->addWidget(metricsView_, 1);

  auto* hint = new QLabel(
      QStringLiteral("Phase 3：相机插件层（GenTL/UVC 桩 + Simulated 实时）。\n"
                     "未做真实硬件联调；专有 SDK 见 README 插件说明。\n"
                     "可选源 → 连接 → 实时预览；软触发模式下点「软触发一次」。"));
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color: #666;"));
  rightLayout->addWidget(hint);

  mainSplit->addWidget(right);
  mainSplit->setStretchFactor(0, 3);
  mainSplit->setStretchFactor(1, 1);
  setCentralWidget(mainSplit);

  statusLabel_ = new QLabel(QStringLiteral("就绪"));
  statusBar()->addWidget(statusLabel_, 1);

  setCameraUiConnected(false);
}

beam::camera::CameraControls MainWindow::readCameraControls() const {
  beam::camera::CameraControls c;
  c.exposure_us = camExposureSpin_->value();
  c.gain_db = camGainSpin_->value();
  c.targetFps = camFpsSpin_->value();
  c.trigger = static_cast<beam::camera::TriggerMode>(
      triggerModeBox_->currentData().toInt());
  return c;
}

void MainWindow::applyCameraControlsToUi(const beam::camera::CameraControls& c) {
  const QSignalBlocker b1(camExposureSpin_);
  const QSignalBlocker b2(camGainSpin_);
  const QSignalBlocker b3(camFpsSpin_);
  const QSignalBlocker b4(triggerModeBox_);
  camExposureSpin_->setValue(c.exposure_us);
  camGainSpin_->setValue(c.gain_db);
  camFpsSpin_->setValue(c.targetFps);
  const int idx = triggerModeBox_->findData(static_cast<int>(c.trigger));
  if (idx >= 0) {
    triggerModeBox_->setCurrentIndex(idx);
  }
}

void MainWindow::setCameraUiConnected(bool connected) {
  cameraConnectBtn_->setEnabled(!connected);
  cameraDisconnectBtn_->setEnabled(connected);
  livePreviewCheck_->setEnabled(connected);
  camExposureSpin_->setEnabled(connected);
  camGainSpin_->setEnabled(connected);
  camFpsSpin_->setEnabled(connected);
  triggerModeBox_->setEnabled(connected);
  softwareTriggerBtn_->setEnabled(connected);
  if (!connected) {
    livePreviewCheck_->setChecked(false);
  }
}

void MainWindow::refreshCameraList() {
  beam::camera::registerAllBuiltInBackends();
  discoveredCameras_ = beam::camera::CameraRegistry::instance().discoverAll();
  const QString previous = cameraSourceBox_->currentData().toString();
  cameraSourceBox_->clear();
  for (const auto& d : discoveredCameras_) {
    QString label = QString::fromStdString(d.name);
    label += QStringLiteral(" [%1]").arg(QString::fromStdString(d.transport));
    if (!d.available) {
      label += QStringLiteral(" (不可用)");
    }
    cameraSourceBox_->addItem(label, QString::fromStdString(d.id));
  }
  const int idx = cameraSourceBox_->findData(previous);
  if (idx >= 0) {
    cameraSourceBox_->setCurrentIndex(idx);
  }
  statusLabel_->setText(
      QStringLiteral("已发现 %1 个相机源").arg(discoveredCameras_.size()));
}

void MainWindow::onCameraConnect() {
  if (activeCamera_) {
    onCameraDisconnect();
  }
  const QString id = cameraSourceBox_->currentData().toString();
  if (id.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("相机"),
                             QStringLiteral("请先选择相机源。"));
    return;
  }

  auto cam = beam::camera::CameraRegistry::instance().create(id.toStdString());
  if (!cam) {
    QMessageBox::warning(this, QStringLiteral("相机"),
                         QStringLiteral("无法创建相机实例。"));
    return;
  }
  const auto info = cam->info();
  if (!cam->open()) {
    QMessageBox::warning(
        this, QStringLiteral("连接失败"),
        QStringLiteral("%1\n（桩后端在无 SDK/CTI/OpenCV 时无法打开，属预期行为）")
            .arg(QString::fromStdString(info.name)));
    return;
  }

  activeCamera_ = std::move(cam);
  activeCamera_->setControls(readCameraControls());
  applyCameraControlsToUi(activeCamera_->controls());
  setCameraUiConnected(true);
  statusLabel_->setText(
      QStringLiteral("已连接: %1").arg(QString::fromStdString(info.name)));
}

void MainWindow::onCameraDisconnect() {
  if (liveTimer_) {
    liveTimer_->stop();
  }
  if (livePreviewCheck_) {
    const QSignalBlocker b(livePreviewCheck_);
    livePreviewCheck_->setChecked(false);
  }
  if (activeCamera_) {
    activeCamera_->stopAcquisition();
    activeCamera_->close();
    activeCamera_.reset();
  }
  setCameraUiConnected(false);
  statusLabel_->setText(QStringLiteral("相机已断开"));
}

void MainWindow::onLivePreviewToggled(bool on) {
  if (!activeCamera_) {
    return;
  }
  if (on) {
    activeCamera_->setControls(readCameraControls());
    if (!activeCamera_->startAcquisition()) {
      livePreviewCheck_->setChecked(false);
      QMessageBox::warning(this, QStringLiteral("实时预览"),
                           QStringLiteral("无法启动采集。"));
      return;
    }
    const int intervalMs =
        static_cast<int>(std::max(10.0, 1000.0 / std::max(1.0, camFpsSpin_->value())));
    liveTimer_->start(intervalMs);
    statusLabel_->setText(QStringLiteral("实时预览中…"));
  } else {
    liveTimer_->stop();
    activeCamera_->stopAcquisition();
    statusLabel_->setText(QStringLiteral("实时预览已停止"));
  }
}

void MainWindow::onCameraControlsChanged() {
  if (!activeCamera_ || !activeCamera_->isOpen()) {
    return;
  }
  const auto ctrl = readCameraControls();
  activeCamera_->setControls(ctrl);
  softwareTriggerBtn_->setEnabled(ctrl.trigger == beam::camera::TriggerMode::Software ||
                                  ctrl.trigger == beam::camera::TriggerMode::FreeRun);
  if (livePreviewCheck_->isChecked()) {
    const int intervalMs =
        static_cast<int>(std::max(10.0, 1000.0 / std::max(1.0, ctrl.targetFps)));
    liveTimer_->setInterval(intervalMs);
  }
}

void MainWindow::onSoftwareTrigger() {
  if (!activeCamera_ || !activeCamera_->isOpen()) {
    return;
  }
  if (!activeCamera_->isAcquiring()) {
    if (!activeCamera_->startAcquisition()) {
      QMessageBox::warning(this, QStringLiteral("软触发"),
                           QStringLiteral("无法启动采集。"));
      return;
    }
  }
  if (!activeCamera_->softwareTrigger()) {
    // FreeRun: just grab once below
  }
  const auto cap = activeCamera_->grab();
  if (!cap.ok) {
    statusLabel_->setText(QString::fromStdString(cap.message));
    return;
  }
  applyFrame(cap.frame,
             QStringLiteral("<camera:%1>")
                 .arg(QString::fromStdString(activeCamera_->info().id)));
}

void MainWindow::onLiveTick() {
  if (!activeCamera_ || !activeCamera_->isAcquiring()) {
    return;
  }
  const auto mode = static_cast<beam::camera::TriggerMode>(
      triggerModeBox_->currentData().toInt());
  if (mode == beam::camera::TriggerMode::Software ||
      mode == beam::camera::TriggerMode::Hardware) {
    // Wait for explicit trigger / external line — do not block UI on timeout.
    return;
  }
  const auto cap = activeCamera_->grab();
  if (!cap.ok) {
    return;
  }
  applyFrame(cap.frame,
             QStringLiteral("<live:%1>")
                 .arg(QString::fromStdString(activeCamera_->info().id)));
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
  if (prep.background.mode == beam::core::BackgroundMode::RoiMean) {
    if (roi_ && !roi_->empty() && rawFrame_) {
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
  auto cam = beam::camera::CameraRegistry::instance().createFromBackend("simulated");
  if (!cam) {
    cam = beam::camera::createSimulatedCamera(160, 120, 5.0);
  }
  if (!cam->isOpen()) {
    cam->open();
  }
  const auto cap = cam->grab();
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
  if (crossX_ < 0 || crossY_ < 0 ||
      crossX_ >= static_cast<int>(rawFrame_->width) ||
      crossY_ >= static_cast<int>(rawFrame_->height)) {
    crossX_ = static_cast<int>(rawFrame_->width / 2);
    crossY_ = static_cast<int>(rawFrame_->height / 2);
  }
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
  double mn = 0;
  double mx = 0;
  const QImage displayBase = frameToImage(*rawFrame_, colorMap_, &mn, &mx);
  const QImage displayScaled = displayBase.scaled(
      std::max(displayBase.width(), 480), std::max(displayBase.height(), 360),
      Qt::KeepAspectRatio, Qt::FastTransformation);
  imageView_->setDisplayImage(displayScaled, static_cast<int>(rawFrame_->width),
                              static_cast<int>(rawFrame_->height));
  if (roi_) {
    imageView_->setRoi(roi_);
  }
}

void MainWindow::refreshProfiles() {
  if (!rawFrame_ || crossX_ < 0 || crossY_ < 0) {
    hProfilePlot_->clearProfile();
    vProfilePlot_->clearProfile();
    return;
  }
  const beam::core::BeamFrame* src = rawFrame_.operator->();
  int cx = crossX_;
  int cy = crossY_;
  if (analysisFrame_ && analysisFrame_->width == rawFrame_->width &&
      analysisFrame_->height == rawFrame_->height) {
    src = analysisFrame_.operator->();
  } else if (useRoiCheck_->isChecked() && roi_ && analysisFrame_) {
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

  if (activeCamera_) {
    const auto c = activeCamera_->controls();
    text += QStringLiteral("\n— 相机控件 —\n");
    text += QStringLiteral("触发: %1\n").arg(QString::fromUtf8(beam::camera::toString(c.trigger)));
    text += QStringLiteral("曝光: %1 µs\n").arg(fmt(c.exposure_us));
    text += QStringLiteral("增益: %1 dB\n").arg(fmt(c.gain_db));
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
  meta.note = "Beam Analyzer Phase 3";
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
  meta.note = "Beam Analyzer Phase 3";
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
  meta.note = "Beam Analyzer Phase 3 power";
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
  s.setValue(QStringLiteral("camExposure_us"), camExposureSpin_->value());
  s.setValue(QStringLiteral("camGain_db"), camGainSpin_->value());
  s.setValue(QStringLiteral("camFps"), camFpsSpin_->value());
  s.setValue(QStringLiteral("triggerMode"), triggerModeBox_->currentIndex());
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
  camExposureSpin_->setValue(s.value(QStringLiteral("camExposure_us"), 1000.0).toDouble());
  camGainSpin_->setValue(s.value(QStringLiteral("camGain_db"), 0.0).toDouble());
  camFpsSpin_->setValue(s.value(QStringLiteral("camFps"), 15.0).toDouble());
  triggerModeBox_->setCurrentIndex(s.value(QStringLiteral("triggerMode"), 0).toInt());
  restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
}

}  // namespace beam::ui
