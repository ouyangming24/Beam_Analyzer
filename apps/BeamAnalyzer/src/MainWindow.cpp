#include "MainWindow.hpp"

#include "AnalysisDialog.hpp"
#include "IntensitySurface3D.hpp"
#include "M2ScanDialog.hpp"
#include "MetricsPanel.hpp"
#include "ProfilePlot.hpp"

#include "beam/io/CsvImporter.hpp"
#include "beam/io/ImageExporter.hpp"
#include "beam/io/ImageImporter.hpp"
#include "beam/io/ResultExporter.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace beam::ui {
namespace {

QString fmt(double v, int digits = 4) {
  return QString::number(v, 'g', digits);
}

bool isImageExt(const QString& ext) {
  const QString e = ext.toLower();
  return e == QLatin1String("png") || e == QLatin1String("bmp") ||
         e == QLatin1String("jpg") || e == QLatin1String("jpeg") ||
         e == QLatin1String("tif") || e == QLatin1String("tiff");
}

beam::core::BeamFrame qImageToFrame(const QImage& src, double px, double py) {
  QImage img = src;
  if (img.format() != QImage::Format_Grayscale8 &&
      img.format() != QImage::Format_Grayscale16 &&
      img.format() != QImage::Format_RGB32 &&
      img.format() != QImage::Format_ARGB32 &&
      img.format() != QImage::Format_RGB888) {
    img = img.convertToFormat(QImage::Format_RGB32);
  }

  beam::io::ImageImportOptions opt;
  opt.pixelSizeX_um = px;
  opt.pixelSizeY_um = py;

  if (img.format() == QImage::Format_Grayscale16) {
    std::vector<std::uint16_t> buf(static_cast<std::size_t>(img.width() * img.height()));
    for (int y = 0; y < img.height(); ++y) {
      const auto* line = reinterpret_cast<const std::uint16_t*>(img.constScanLine(y));
      std::copy(line, line + img.width(),
                buf.begin() + static_cast<std::size_t>(y * img.width()));
    }
    return beam::io::frameFromGray16(static_cast<std::size_t>(img.width()),
                                     static_cast<std::size_t>(img.height()), buf.data(),
                                     opt)
        .frame;
  }

  if (img.format() == QImage::Format_Grayscale8) {
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(img.width() * img.height()));
    for (int y = 0; y < img.height(); ++y) {
      const auto* line = img.constScanLine(y);
      std::copy(line, line + img.width(),
                buf.begin() + static_cast<std::size_t>(y * img.width()));
    }
    return beam::io::frameFromGray8(static_cast<std::size_t>(img.width()),
                                    static_cast<std::size_t>(img.height()), buf.data(),
                                    opt)
        .frame;
  }

  img = img.convertToFormat(QImage::Format_RGB888);
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(img.width() * img.height() * 3));
  for (int y = 0; y < img.height(); ++y) {
    const auto* line = img.constScanLine(y);
    std::copy(line, line + img.width() * 3,
              rgb.begin() + static_cast<std::size_t>(y * img.width() * 3));
  }
  return beam::io::frameFromRgb24(static_cast<std::size_t>(img.width()),
                                  static_cast<std::size_t>(img.height()), rgb.data(),
                                  opt)
      .frame;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Beam Analyzer"));
  resize(1400, 900);
  setAcceptDrops(true);
  beam::camera::registerAllBuiltInBackends();
  rebuildUi();
  refreshCameraList();
  loadSettings();
  setTheme(theme_);
}

MainWindow::~MainWindow() {
  onCameraDisconnect();
  saveSettings();
}

void MainWindow::captureDemoShots(const QString& outDir) {
  QDir().mkpath(outDir);
  loadSimulatedFrame();
  onAutoAoi();
  // Allow paint/layout to settle.
  QApplication::processEvents();
  QApplication::processEvents();

  const QString mainPath = outDir + QStringLiteral("/cinogy-ui-main.png");
  if (!grab().save(mainPath, "PNG")) {
    statusLabel_->setText(QStringLiteral("截图失败: main"));
  }

  if (surface3d_) {
    // Emulate a slight rotation so the mountain reads clearly.
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(80, 80), QPointF(80, 80),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, QPointF(140, 100), QPointF(140, 100),
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(140, 100), QPointF(140, 100),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(surface3d_, &press);
    QApplication::sendEvent(surface3d_, &move);
    QApplication::sendEvent(surface3d_, &release);
    QApplication::processEvents();
    surface3d_->grab().save(outDir + QStringLiteral("/cinogy-ui-3d.png"), "PNG");
  }

  openAnalysis();
  QApplication::processEvents();
  if (analysisDialog_) {
    if (auto* tabs = analysisDialog_->findChild<QTabWidget*>()) {
      tabs->setCurrentIndex(1);  // 二维/三维能量
    }
    analysisDialog_->resize(1100, 720);
    QApplication::processEvents();
    analysisDialog_->grab().save(outDir + QStringLiteral("/cinogy-ui-analysis.png"), "PNG");
  }
}

void MainWindow::rebuildUi() {
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  auto* openAct = fileMenu->addAction(QStringLiteral("打开…"));
  openAct->setShortcut(QKeySequence::Open);
  connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
  fileMenu->addAction(QStringLiteral("打开 CSV…"), this, &MainWindow::openCsv);
  fileMenu->addAction(QStringLiteral("打开图像…"), this, &MainWindow::openImage);
  recentMenu_ = fileMenu->addMenu(QStringLiteral("最近打开"));
  rebuildRecentMenu();
  fileMenu->addSeparator();
  exportJsonAction_ = fileMenu->addAction(QStringLiteral("导出指标 JSON…"));
  connect(exportJsonAction_, &QAction::triggered, this, &MainWindow::exportJson);
  exportCsvAction_ = fileMenu->addAction(QStringLiteral("导出指标 CSV…"));
  connect(exportCsvAction_, &QAction::triggered, this, &MainWindow::exportCsv);
  exportImageAction_ = fileMenu->addAction(QStringLiteral("导出标注图像…"));
  connect(exportImageAction_, &QAction::triggered, this, &MainWindow::exportAnnotatedImage);
  exportJsonAction_->setEnabled(false);
  exportCsvAction_->setEnabled(false);
  exportImageAction_->setEnabled(false);
  fileMenu->addSeparator();
  auto* quitAct = fileMenu->addAction(QStringLiteral("退出"));
  quitAct->setShortcut(QKeySequence::Quit);
  connect(quitAct, &QAction::triggered, this, &QWidget::close);

  auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
  themeAction_ = viewMenu->addAction(QStringLiteral("切换浅色/深色主题"));
  connect(themeAction_, &QAction::triggered, this, &MainWindow::toggleTheme);

  auto* tool = addToolBar(QStringLiteral("主工具栏"));
  tool->setObjectName(QStringLiteral("MainToolBar"));
  tool->setMovable(false);
  tool->setToolButtonStyle(Qt::ToolButtonTextOnly);
  tool->setIconSize({16, 16});

  tool->addAction(QStringLiteral("打开"), this, &MainWindow::openFile);
  tool->addAction(QStringLiteral("模拟帧"), this, &MainWindow::loadSimulatedFrame);
  tool->addSeparator();
  tool->addAction(exportJsonAction_);
  tool->addAction(exportCsvAction_);
  tool->addAction(exportImageAction_);
  tool->addAction(QStringLiteral("导出功率…"), this, &MainWindow::exportPower);
  tool->addSeparator();
  tool->addWidget(new QLabel(QStringLiteral(" AOI ")));
  aoiShapeBox_ = new QComboBox;
  aoiShapeBox_->addItem(QStringLiteral("矩形"), static_cast<int>(beam::core::AoiShape::Rect));
  aoiShapeBox_->addItem(QStringLiteral("圆形"), static_cast<int>(beam::core::AoiShape::Circle));
  aoiShapeBox_->addItem(QStringLiteral("椭圆"), static_cast<int>(beam::core::AoiShape::Ellipse));
  connect(aoiShapeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onAoiShapeChanged);
  tool->addWidget(aoiShapeBox_);
  roiModeAction_ = tool->addAction(QStringLiteral("绘制 AOI"));
  roiModeAction_->setCheckable(true);
  connect(roiModeAction_, &QAction::toggled, this, &MainWindow::onRoiModeToggled);
  tool->addAction(QStringLiteral("自动 AOI"), this, &MainWindow::onAutoAoi);
  tool->addAction(QStringLiteral("清除 AOI"), this, &MainWindow::onClearRoi);
  tool->addSeparator();
  auto* analysisAct =
      tool->addAction(QStringLiteral("完整分析…"), this, &MainWindow::openAnalysis);
  analysisAct->setObjectName(QStringLiteral("PrimaryAction"));
  auto* m2Act = tool->addAction(QStringLiteral("M² 工作站…"), this, &MainWindow::openM2Scan);
  tool->addSeparator();
  tool->addWidget(new QLabel(QStringLiteral(" 色图 ")));
  colorMapBox_ = new QComboBox;
  colorMapBox_->addItem(QStringLiteral("假彩色"), static_cast<int>(ColorMap::FalseColor));
  colorMapBox_->addItem(QStringLiteral("灰度"), static_cast<int>(ColorMap::Grayscale));
  connect(colorMapBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onColorMapChanged);
  tool->addWidget(colorMapBox_);
  tool->addWidget(new QLabel(QStringLiteral(" 映射 ")));
  intensityScaleBox_ = new QComboBox;
  intensityScaleBox_->addItem(QStringLiteral("线性"), static_cast<int>(IntensityScale::Linear));
  intensityScaleBox_->addItem(QStringLiteral("对数"), static_cast<int>(IntensityScale::Log));
  connect(intensityScaleBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onIntensityScaleChanged);
  tool->addWidget(intensityScaleBox_);
  tool->addSeparator();
  tool->addAction(themeAction_);

  // Host widget so the central area always expands to the full QMainWindow
  // client width (avoids orphan dark gutter past the metrics dock on maximize).
  auto* centralHost = new QWidget;
  centralHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* centralOuter = new QVBoxLayout(centralHost);
  centralOuter->setContentsMargins(0, 0, 0, 0);
  centralOuter->setSpacing(0);

  auto* statusStrip = new QWidget;
  statusStrip->setObjectName(QStringLiteral("StatusStrip"));
  statusStrip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto* stripLay = new QHBoxLayout(statusStrip);
  stripLay->setContentsMargins(10, 4, 10, 4);
  stripLay->setSpacing(14);
  statusStripMode_ = new QLabel(QStringLiteral("离线文件测量"));
  statusStripMode_->setObjectName(QStringLiteral("StatusStripMode"));
  statusStripSource_ = new QLabel(QStringLiteral("尚未载入帧"));
  statusStripValidity_ = new QLabel(QStringLiteral("NO DATA"));
  statusStripValidity_->setObjectName(QStringLiteral("StatusValidityChip"));
  statusStripHint_ = new QLabel(
      QStringLiteral("打开 CSV/图像，或连接模拟相机 · 显示映射不影响 ISO 数值"));
  statusStripHint_->setObjectName(QStringLiteral("StatusStripHint"));
  stripLay->addWidget(statusStripMode_);
  stripLay->addWidget(statusStripSource_, 1);
  stripLay->addWidget(statusStripValidity_);
  stripLay->addWidget(statusStripHint_);
  centralOuter->addWidget(statusStrip);

  auto* centralLayout = new QHBoxLayout;
  centralLayout->setContentsMargins(0, 0, 0, 0);
  centralLayout->setSpacing(0);

  auto* mainSplit = new QSplitter(Qt::Horizontal, centralHost);
  mainSplit->setObjectName(QStringLiteral("MainSplitter"));
  mainSplit->setChildrenCollapsible(false);
  mainSplit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* leftSplit = new QSplitter(Qt::Vertical);
  leftSplit->setObjectName(QStringLiteral("LeftSplitter"));
  leftSplit->setChildrenCollapsible(false);
  leftSplit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* imageChrome = new QWidget;
  imageChrome->setObjectName(QStringLiteral("ImageChrome"));
  imageChrome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto* chromeLay = new QHBoxLayout(imageChrome);
  chromeLay->setContentsMargins(8, 3, 8, 3);
  chromeLay->setSpacing(8);
  imageChromeLabel_ = new QLabel(QStringLiteral("强度图 · 假彩色 / 十字线剖面 · 无帧"));
  imageChromeLabel_->setObjectName(QStringLiteral("SectionHeader"));
  chromeLay->addWidget(imageChromeLabel_, 1);
  colorLegendLabel_ = new QLabel(QStringLiteral("▣ 0 → 峰值"));
  colorLegendLabel_->setObjectName(QStringLiteral("ColorLegend"));
  colorLegendLabel_->setToolTip(QStringLiteral("显示色标（显示层；对数仅影响假彩映射）"));
  chromeLay->addWidget(colorLegendLabel_);
  leftSplit->addWidget(imageChrome);

  imageView_ = new ImageView;
  connect(imageView_, &ImageView::aoiChanged, this, &MainWindow::onAoiChanged);
  connect(imageView_, &ImageView::roiChanged, this, &MainWindow::onRoiChanged);
  connect(imageView_, &ImageView::crosshairChanged, this, &MainWindow::onCrosshairChanged);
  imageView_->clearFrame();

  auto* viewSplit = new QSplitter(Qt::Horizontal);
  viewSplit->setObjectName(QStringLiteral("ViewSplit2D3D"));
  viewSplit->setChildrenCollapsible(false);
  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  scroll->setWidget(imageView_);
  surface3d_ = new IntensitySurface3D;
  viewSplit->addWidget(scroll);
  viewSplit->addWidget(surface3d_);
  viewSplit->setStretchFactor(0, 3);
  viewSplit->setStretchFactor(1, 2);
  viewSplit->setSizes({640, 420});
  leftSplit->addWidget(viewSplit);

  auto* scrubRow = new QWidget;
  scrubRow->setObjectName(QStringLiteral("PlayerBar"));
  scrubRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto* scrubLayout = new QHBoxLayout(scrubRow);
  scrubLayout->setContentsMargins(8, 3, 8, 3);
  scrubLayout->setSpacing(4);
  scrubLayout->addWidget(new QLabel(QStringLiteral("帧播放")));
  auto* firstBtn = new QPushButton(QStringLiteral("⏮"));
  auto* prevBtn = new QPushButton(QStringLiteral("◀"));
  playBtn_ = new QPushButton(QStringLiteral("▶"));
  auto* nextBtn = new QPushButton(QStringLiteral("▶|"));
  auto* lastBtn = new QPushButton(QStringLiteral("⏭"));
  for (QPushButton* b : {firstBtn, prevBtn, playBtn_, nextBtn, lastBtn}) {
    b->setFixedWidth(32);
    b->setToolTip(QStringLiteral("帧历史播放（离线 / 暂停采集后可用）"));
  }
  playBtn_->setObjectName(QStringLiteral("AccentButton"));
  connect(firstBtn, &QPushButton::clicked, this, &MainWindow::onFrameFirst);
  connect(prevBtn, &QPushButton::clicked, this, &MainWindow::onFramePrev);
  connect(playBtn_, &QPushButton::clicked, this, &MainWindow::onPlayToggle);
  connect(nextBtn, &QPushButton::clicked, this, &MainWindow::onFrameNext);
  connect(lastBtn, &QPushButton::clicked, this, &MainWindow::onFrameLast);
  scrubLayout->addWidget(firstBtn);
  scrubLayout->addWidget(prevBtn);
  scrubLayout->addWidget(playBtn_);
  scrubLayout->addWidget(nextBtn);
  scrubLayout->addWidget(lastBtn);
  frameScrubber_ = new QSlider(Qt::Horizontal);
  frameScrubber_->setEnabled(false);
  frameScrubber_->setMinimum(0);
  frameScrubber_->setMaximum(0);
  connect(frameScrubber_, &QSlider::valueChanged, this, &MainWindow::onFrameScrub);
  scrubLayout->addWidget(frameScrubber_, 1);
  frameScrubLabel_ = new QLabel(QStringLiteral("0 / 0"));
  scrubLayout->addWidget(frameScrubLabel_);
  leftSplit->addWidget(scrubRow);

  playTimer_ = new QTimer(this);
  playTimer_->setInterval(120);
  connect(playTimer_, &QTimer::timeout, this, &MainWindow::onPlayTick);

  auto* profiles = new QWidget;
  profiles->setObjectName(QStringLiteral("ProfilePanel"));
  profiles->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto* profilesOuter = new QVBoxLayout(profiles);
  profilesOuter->setContentsMargins(0, 0, 0, 0);
  profilesOuter->setSpacing(0);
  auto* profilesHeader = new QLabel(QStringLiteral("线剖面 · 水平 / 垂直（与十字线同步）"));
  profilesHeader->setObjectName(QStringLiteral("SectionHeader"));
  profilesHeader->setContentsMargins(8, 4, 8, 2);
  profilesOuter->addWidget(profilesHeader);
  auto* profilesRow = new QWidget;
  auto* profilesLayout = new QHBoxLayout(profilesRow);
  profilesLayout->setContentsMargins(0, 0, 0, 0);
  profilesLayout->setSpacing(2);
  hProfilePlot_ = new ProfilePlot;
  vProfilePlot_ = new ProfilePlot;
  hProfilePlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  vProfilePlot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  profilesLayout->addWidget(hProfilePlot_, 1);
  profilesLayout->addWidget(vProfilePlot_, 1);
  profilesOuter->addWidget(profilesRow, 1);
  leftSplit->addWidget(profiles);
  leftSplit->setStretchFactor(0, 0);
  leftSplit->setStretchFactor(1, 5);
  leftSplit->setStretchFactor(2, 0);
  leftSplit->setStretchFactor(3, 1);
  mainSplit->addWidget(leftSplit);

  auto* right = new QWidget;
  right->setObjectName(QStringLiteral("ControlColumn"));
  // Prefer a stable control width; image column absorbs extra horizontal space.
  right->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  right->setMinimumWidth(260);
  right->setMaximumWidth(440);
  auto* rightOuter = new QVBoxLayout(right);
  rightOuter->setContentsMargins(0, 0, 0, 0);
  rightOuter->setSpacing(0);
  auto* rightScroll = new QScrollArea;
  rightScroll->setWidgetResizable(true);
  rightScroll->setFrameShape(QFrame::NoFrame);
  rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* rightInner = new QWidget;
  rightInner->setObjectName(QStringLiteral("ControlColumn"));
  auto* rightLayout = new QVBoxLayout(rightInner);
  rightLayout->setContentsMargins(6, 6, 6, 6);
  rightLayout->setSpacing(6);

  auto* camBox = new QGroupBox(QStringLiteral("相机采集"));
  auto* camForm = new QFormLayout(camBox);
  camForm->setSpacing(4);
  camForm->setContentsMargins(8, 12, 8, 6);

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
  connect(camExposureSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("曝光"), camExposureSpin_);

  camGainSpin_ = new QDoubleSpinBox;
  camGainSpin_->setRange(-20.0, 40.0);
  camGainSpin_->setDecimals(2);
  camGainSpin_->setValue(0.0);
  camGainSpin_->setSuffix(QStringLiteral(" dB"));
  connect(camGainSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("增益"), camGainSpin_);

  camFpsSpin_ = new QDoubleSpinBox;
  camFpsSpin_->setRange(0.1, 120.0);
  camFpsSpin_->setDecimals(1);
  camFpsSpin_->setValue(15.0);
  camFpsSpin_->setSuffix(QStringLiteral(" fps"));
  connect(camFpsSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("目标帧率"), camFpsSpin_);

  triggerModeBox_ = new QComboBox;
  triggerModeBox_->addItem(QStringLiteral("连续 (FreeRun)"),
                           static_cast<int>(beam::camera::TriggerMode::FreeRun));
  triggerModeBox_->addItem(QStringLiteral("软触发 (Software)"),
                           static_cast<int>(beam::camera::TriggerMode::Software));
  triggerModeBox_->addItem(QStringLiteral("硬触发 (Hardware)"),
                           static_cast<int>(beam::camera::TriggerMode::Hardware));
  connect(triggerModeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onCameraControlsChanged);
  camForm->addRow(QStringLiteral("触发"), triggerModeBox_);

  softwareTriggerBtn_ = new QPushButton(QStringLiteral("软触发一次"));
  connect(softwareTriggerBtn_, &QPushButton::clicked, this, &MainWindow::onSoftwareTrigger);
  camForm->addRow(softwareTriggerBtn_);
  rightLayout->addWidget(camBox);

  liveTimer_ = new QTimer(this);
  connect(liveTimer_, &QTimer::timeout, this, &MainWindow::onLiveTick);

  auto* toolsBox = new QGroupBox(QStringLiteral("分析工具"));
  auto* toolsForm = new QFormLayout(toolsBox);
  toolsForm->setSpacing(4);
  toolsForm->setContentsMargins(8, 12, 8, 6);

  useRoiCheck_ = new QCheckBox(QStringLiteral("启用 AOI"));
  connect(useRoiCheck_, &QCheckBox::toggled, this, [this](bool) { onRecompute(); });
  toolsForm->addRow(useRoiCheck_);

  backgroundBox_ = new QComboBox;
  backgroundBox_->addItem(QStringLiteral("无"), static_cast<int>(beam::core::BackgroundMode::None));
  backgroundBox_->addItem(QStringLiteral("常数"),
                          static_cast<int>(beam::core::BackgroundMode::Constant));
  backgroundBox_->addItem(QStringLiteral("边框均值"),
                          static_cast<int>(beam::core::BackgroundMode::BorderMean));
  backgroundBox_->addItem(QStringLiteral("背景 ROI 均值"),
                          static_cast<int>(beam::core::BackgroundMode::RoiMean));
  connect(backgroundBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::onBackgroundModeChanged);
  toolsForm->addRow(QStringLiteral("背景"), backgroundBox_);

  bgConstantSpin_ = new QDoubleSpinBox;
  bgConstantSpin_->setRange(-1e9, 1e9);
  bgConstantSpin_->setDecimals(4);
  bgConstantSpin_->setEnabled(false);
  connect(bgConstantSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { onRecompute(); });
  toolsForm->addRow(QStringLiteral("常数背景"), bgConstantSpin_);

  countsToWattsSpin_ = new QDoubleSpinBox;
  countsToWattsSpin_->setRange(0.0, 1e3);
  countsToWattsSpin_->setDecimals(10);
  countsToWattsSpin_->setSingleStep(1e-6);
  countsToWattsSpin_->setSpecialValueText(QStringLiteral("未标定"));
  connect(countsToWattsSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCalibrationChanged);
  toolsForm->addRow(QStringLiteral("counts→W"), countsToWattsSpin_);

  exposureSpin_ = new QDoubleSpinBox;
  exposureSpin_->setRange(1e-9, 1e3);
  exposureSpin_->setDecimals(6);
  exposureSpin_->setValue(1.0);
  exposureSpin_->setSuffix(QStringLiteral(" s"));
  connect(exposureSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCalibrationChanged);
  toolsForm->addRow(QStringLiteral("分析曝光"), exposureSpin_);

  wavelengthSpin_ = new QDoubleSpinBox;
  wavelengthSpin_->setRange(1.0, 1e6);
  wavelengthSpin_->setDecimals(3);
  wavelengthSpin_->setValue(1064.0);
  wavelengthSpin_->setSuffix(QStringLiteral(" nm"));
  connect(wavelengthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onCalibrationChanged);
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

  auto* hint = new QLabel(
      QStringLiteral("提示：拖放 PNG/BMP/TIFF/JPEG/CSV 到窗口即可打开。"
                     "实时预览时指标会动态刷新；暂停后可用帧历史滑条回看。"));
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color: #8892a4; font-size: 11px;"));
  rightLayout->addWidget(hint);
  rightLayout->addStretch(1);
  rightScroll->setWidget(rightInner);
  rightOuter->addWidget(rightScroll, 1);

  mainSplit->addWidget(right);
  // Image/profile column takes all extra width; control column stays near preferred size.
  mainSplit->setStretchFactor(0, 1);
  mainSplit->setStretchFactor(1, 0);
  mainSplit->setSizes({1000, 320});
  centralLayout->addWidget(mainSplit, /*stretch=*/1);
  centralOuter->addLayout(centralLayout, 1);
  setCentralWidget(centralHost);

  metricsPanel_ = new MetricsPanel;
  metricsPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  metricsPanel_->setMinimumWidth(240);
  metricsDock_ = new QDockWidget(QStringLiteral("测量指标"), this);
  metricsDock_->setObjectName(QStringLiteral("MetricsDock"));
  metricsDock_->setWidget(metricsPanel_);
  metricsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  metricsDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                            QDockWidget::DockWidgetClosable);
  metricsDock_->setMinimumWidth(260);
  addDockWidget(Qt::RightDockWidgetArea, metricsDock_);
  resizeDocks({metricsDock_}, {300}, Qt::Horizontal);
  viewMenu->addAction(metricsDock_->toggleViewAction());

  statusModeChip_ = new QLabel(QStringLiteral("离线"));
  statusModeChip_->setObjectName(QStringLiteral("StatusModeChip"));
  statusLiveChip_ = new QLabel(QStringLiteral("IDLE"));
  statusLiveChip_->setObjectName(QStringLiteral("StatusLiveChip"));
  statusLiveChip_->setVisible(false);
  statusLabel_ = new QLabel(QStringLiteral("就绪 — 打开帧或连接模拟相机开始测量"));
  statusMetricsBrief_ = new QLabel(QStringLiteral("—"));
  statusBar()->addWidget(statusModeChip_);
  statusBar()->addWidget(statusLiveChip_);
  statusBar()->addWidget(statusLabel_, 2);
  statusBar()->addPermanentWidget(statusMetricsBrief_, 1);

  setCameraUiConnected(false);
  updateMeasurementChrome();
}

void MainWindow::rebuildRecentMenu() {
  if (!recentMenu_) {
    return;
  }
  recentMenu_->clear();
  if (recentFiles_.isEmpty()) {
    auto* empty = recentMenu_->addAction(QStringLiteral("（无）"));
    empty->setEnabled(false);
  } else {
    for (const QString& path : recentFiles_) {
      auto* act = recentMenu_->addAction(path);
      act->setData(path);
      connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
  }
  recentMenu_->addSeparator();
  recentMenu_->addAction(QStringLiteral("清除列表"), this, &MainWindow::clearRecentFiles);
}

void MainWindow::addRecentFile(const QString& path) {
  if (path.isEmpty() || path.startsWith(QLatin1Char('<'))) {
    return;
  }
  recentFiles_.removeAll(path);
  recentFiles_.prepend(path);
  while (recentFiles_.size() > 12) {
    recentFiles_.removeLast();
  }
  rebuildRecentMenu();
}

void MainWindow::openRecentFile() {
  auto* act = qobject_cast<QAction*>(sender());
  if (!act) {
    return;
  }
  loadPath(act->data().toString());
}

void MainWindow::clearRecentFiles() {
  recentFiles_.clear();
  rebuildRecentMenu();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent* event) {
  const auto urls = event->mimeData()->urls();
  if (urls.isEmpty()) {
    return;
  }
  loadPath(urls.front().toLocalFile());
}

bool MainWindow::loadPath(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }
  const QFileInfo fi(path);
  const QString ext = fi.suffix().toLower();
  if (ext == QLatin1String("csv")) {
    return loadCsvPath(path);
  }
  if (isImageExt(ext)) {
    return loadImagePath(path);
  }
  QMessageBox::information(this, QStringLiteral("打开"),
                           QStringLiteral("不支持的文件类型：%1").arg(ext));
  return false;
}

void MainWindow::openFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开帧数据"), QString(),
      QStringLiteral(
          "支持的文件 (*.csv *.png *.bmp *.jpg *.jpeg *.tif *.tiff);;"
          "CSV (*.csv);;Images (*.png *.bmp *.jpg *.jpeg *.tif *.tiff);;All (*)"));
  if (!path.isEmpty()) {
    loadPath(path);
  }
}

void MainWindow::openCsv() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开光束帧 CSV"), QString(),
      QStringLiteral("CSV (*.csv);;All files (*)"));
  if (!path.isEmpty()) {
    loadCsvPath(path);
  }
}

void MainWindow::openImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开图像"), QString(),
      QStringLiteral("Images (*.png *.bmp *.jpg *.jpeg *.tif *.tiff);;All (*)"));
  if (!path.isEmpty()) {
    loadImagePath(path);
  }
}

bool MainWindow::loadCsvPath(const QString& path) {
  beam::io::CsvImportOptions opt;
  opt.pixelSizeX_um = 5.0;
  opt.pixelSizeY_um = 5.0;
  const auto result = beam::io::loadFrameCsv(path.toStdString(), opt);
  if (!result.ok) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QString::fromStdString(result.message));
    return false;
  }
  applyFrame(result.frame, path);
  addRecentFile(path);
  return true;
}

bool MainWindow::loadImagePath(const QString& path) {
  const QFileInfo fi(path);
  const QString ext = fi.suffix().toLower();

  // Prefer portable BMP path for parity with library tests; else Qt codecs.
  if (ext == QLatin1String("bmp")) {
    beam::io::ImageImportOptions opt;
    opt.pixelSizeX_um = 5.0;
    opt.pixelSizeY_um = 5.0;
    const auto result = beam::io::loadFrameBmp(path.toStdString(), opt);
    if (result.ok) {
      applyFrame(result.frame, path);
      addRecentFile(path);
      return true;
    }
  }

  QImage img(path);
  if (img.isNull()) {
    QMessageBox::warning(
        this, QStringLiteral("导入失败"),
        QStringLiteral("无法解码图像（TIFF 可能需要 Qt imageformats 插件）：\n%1")
            .arg(path));
    return false;
  }
  auto frame = qImageToFrame(img, 5.0, 5.0);
  if (frame.empty()) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QStringLiteral("图像转换失败。"));
    return false;
  }
  applyFrame(std::move(frame), path);
  addRecentFile(path);
  return true;
}

beam::camera::CameraControls MainWindow::readCameraControls() const {
  beam::camera::CameraControls c;
  c.exposure_us = camExposureSpin_->value();
  c.gain_db = camGainSpin_->value();
  c.targetFps = camFpsSpin_->value();
  c.trigger = static_cast<beam::camera::TriggerMode>(triggerModeBox_->currentData().toInt());
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
  liveActive_ = false;
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
  if (metricsPanel_) {
    metricsPanel_->setLive(false);
  }
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
    liveActive_ = true;
    statusLabel_->setText(QStringLiteral("实时预览中…"));
  } else {
    liveTimer_->stop();
    activeCamera_->stopAcquisition();
    liveActive_ = false;
    if (metricsPanel_) {
      metricsPanel_->setLive(false);
    }
    statusLabel_->setText(QStringLiteral("实时预览已停止 — 可用帧历史回看"));
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
    // FreeRun: grab once below
  }
  const auto cap = activeCamera_->grab();
  if (!cap.ok) {
    statusLabel_->setText(QString::fromStdString(cap.message));
    return;
  }
  applyFrame(cap.frame, QStringLiteral("<camera:%1>")
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
    return;
  }
  const auto cap = activeCamera_->grab();
  if (!cap.ok) {
    return;
  }
  applyFrame(cap.frame, QStringLiteral("<live:%1>")
                            .arg(QString::fromStdString(activeCamera_->info().id)));
}

void MainWindow::seekHistory(int index) {
  if (frameHistory_.empty()) {
    return;
  }
  index = std::clamp(index, 0, static_cast<int>(frameHistory_.size()) - 1);
  if (frameScrubber_->value() != index) {
    const QSignalBlocker b(frameScrubber_);
    frameScrubber_->setValue(index);
  }
  scrubbing_ = true;
  applyFrame(frameHistory_[static_cast<std::size_t>(index)],
             frameHistoryLabels_[static_cast<std::size_t>(index)],
             /*pushHistory=*/false);
  scrubbing_ = false;
  frameScrubLabel_->setText(
      QStringLiteral("%1 / %2").arg(index + 1).arg(frameHistory_.size()));
}

void MainWindow::stopHistoryPlayback() {
  historyPlaying_ = false;
  if (playTimer_) {
    playTimer_->stop();
  }
  if (playBtn_) {
    playBtn_->setText(QStringLiteral("▶"));
  }
}

void MainWindow::onFrameScrub(int index) {
  if (scrubbing_ || frameHistory_.empty()) {
    return;
  }
  seekHistory(index);
}

void MainWindow::onPlayToggle() {
  if (liveActive_) {
    statusLabel_->setText(QStringLiteral("实时采集中 — 请先停止预览再回放帧历史"));
    return;
  }
  if (frameHistory_.size() < 2) {
    statusLabel_->setText(QStringLiteral("帧历史不足，无法播放"));
    return;
  }
  historyPlaying_ = !historyPlaying_;
  if (historyPlaying_) {
    playBtn_->setText(QStringLiteral("❚❚"));
    playTimer_->start();
  } else {
    stopHistoryPlayback();
  }
}

void MainWindow::onPlayTick() {
  if (!historyPlaying_ || frameHistory_.size() < 2) {
    stopHistoryPlayback();
    return;
  }
  int next = frameScrubber_->value() + 1;
  if (next > frameScrubber_->maximum()) {
    next = 0;
  }
  seekHistory(next);
}

void MainWindow::onFrameFirst() {
  stopHistoryPlayback();
  seekHistory(0);
}

void MainWindow::onFramePrev() {
  stopHistoryPlayback();
  seekHistory(frameScrubber_->value() - 1);
}

void MainWindow::onFrameNext() {
  stopHistoryPlayback();
  seekHistory(frameScrubber_->value() + 1);
}

void MainWindow::onFrameLast() {
  stopHistoryPlayback();
  seekHistory(static_cast<int>(frameHistory_.size()) - 1);
}

void MainWindow::onIntensityScaleChanged(int index) {
  intensityScale_ =
      static_cast<IntensityScale>(intensityScaleBox_->itemData(index).toInt());
  refreshImage();
  updateMeasurementChrome();
}

beam::core::AnalysisPrep MainWindow::currentPrep() const {
  beam::core::AnalysisPrep prep;
  const bool aoiOn = useRoiCheck_->isChecked() && aoi_.has_value() && !aoi_->empty();
  if (aoiOn) {
    // Prefer mask path so centroids stay in full-frame coordinates.
    prep.useAoi = true;
    prep.aoiBounds = aoi_->bounds;
    prep.aoiShape = static_cast<int>(aoi_->shape);
    prep.useRoi = false;
  }
  prep.background.mode =
      static_cast<beam::core::BackgroundMode>(backgroundBox_->currentData().toInt());
  prep.background.constantValue = bgConstantSpin_->value();
  prep.background.clipNegative = true;
  if (prep.background.mode == beam::core::BackgroundMode::RoiMean) {
    if (rawFrame_) {
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

void MainWindow::applyFrame(beam::core::BeamFrame frame, const QString& sourceLabel,
                            bool pushHistory) {
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
  exportImageAction_->setEnabled(true);

  if (pushHistory && !scrubbing_) {
    frameHistory_.push_back(*rawFrame_);
    frameHistoryLabels_.push_back(sourceLabel);
    while (static_cast<int>(frameHistory_.size()) > kMaxHistory) {
      frameHistory_.pop_front();
      frameHistoryLabels_.pop_front();
    }
    const QSignalBlocker b(frameScrubber_);
    frameScrubber_->setEnabled(frameHistory_.size() > 1);
    frameScrubber_->setMaximum(static_cast<int>(frameHistory_.size()) - 1);
    frameScrubber_->setValue(frameScrubber_->maximum());
    frameScrubLabel_->setText(QStringLiteral("%1 / %2")
                                  .arg(frameHistory_.size())
                                  .arg(frameHistory_.size()));
  }

  refreshAnalysis();
  statusLabel_->setText(QStringLiteral("已加载 %1×%2 — %3")
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
  iso13694_ = beam::core::computeIso13694(*analysisFrame_, 0.5);
  power_ = beam::core::computePowerMetrics(*analysisFrame_, currentCalibration());
  if (metrics_ && metrics_->valid && analysisDialog_ && analysisDialog_->isVisible()) {
    analysisDialog_->setFrame(*rawFrame_, *metrics_, power_, aoi_, wavelengthSpin_->value());
    analysisDialog_->pushStabilitySample(stabilityT_, metrics_->centroidX_um,
                                         metrics_->centroidY_um, metrics_->totalPower);
    stabilityT_ += 1.0 / std::max(1.0, camFpsSpin_->value());
  }
  refreshImage();
  refreshProfiles();
  refreshMetricsPanel();
  updateStatusBarDetails();
}

void MainWindow::refreshImage() {
  if (!rawFrame_) {
    return;
  }
  double mn = 0;
  double mx = 0;
  const QImage displayBase =
      frameToImage(*rawFrame_, colorMap_, &mn, &mx, intensityScale_);
  const QImage displayScaled =
      displayBase.scaled(std::max(displayBase.width(), 480),
                         std::max(displayBase.height(), 360), Qt::KeepAspectRatio,
                         Qt::FastTransformation);
  imageView_->setDisplayImage(displayScaled, static_cast<int>(rawFrame_->width),
                              static_cast<int>(rawFrame_->height));
  if (aoi_) {
    imageView_->setAoi(aoi_);
  } else {
    imageView_->setAoi(std::nullopt);
  }
  imageView_->setBeamOverlay(metrics_, metrics_ && metrics_->valid, rawFrame_->pixelSizeX_um,
                             rawFrame_->pixelSizeY_um);
  if (surface3d_) {
    surface3d_->setFrame(analysisFrame_ ? *analysisFrame_ : *rawFrame_);
    surface3d_->setAoi(aoi_);
    surface3d_->setBeamOverlay(metrics_);
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
  // AOI mask path keeps full-frame size — prefer analysis frame directly.
  if (analysisFrame_ && analysisFrame_->width == rawFrame_->width &&
      analysisFrame_->height == rawFrame_->height) {
    src = analysisFrame_.operator->();
  }

  const auto cross = beam::core::extractCrosshair(*src, cx, cy);
  hProfile_ = cross.horizontal;
  vProfile_ = cross.vertical;
  hProfilePlot_->setProfile(hProfile_, QStringLiteral("水平剖面 (行 %1)").arg(cy));
  vProfilePlot_->setProfile(vProfile_, QStringLiteral("垂直剖面 (列 %1)").arg(cx));
  imageView_->setCrosshair(crossX_, crossY_);
}

void MainWindow::refreshMetricsPanel() {
  if (!metricsPanel_) {
    return;
  }
  metricsPanel_->setSource(sourcePath_);
  metricsPanel_->setLive(liveActive_, liveActive_ ? camFpsSpin_->value() : 0.0);
  if (!metrics_ || !metrics_->valid) {
    metricsPanel_->clear();
    metricsPanel_->setSource(sourcePath_);
    return;
  }
  std::optional<beam::camera::CameraControls> camCopy;
  if (activeCamera_ && activeCamera_->isOpen()) {
    camCopy = activeCamera_->controls();
  }
  std::optional<beam::core::RectRoi> roiBounds;
  if (aoi_) {
    roiBounds = aoi_->bounds;
  }
  metricsPanel_->updateMetrics(*metrics_, power_, rawFrame_ ? &(*rawFrame_) : nullptr,
                               roiBounds, useRoiCheck_->isChecked(), crossX_, crossY_,
                               camCopy ? &(*camCopy) : nullptr);
}

void MainWindow::updateStatusBarDetails() {
  updateMeasurementChrome();
  if (!metrics_ || !metrics_->valid) {
    statusMetricsBrief_->setText(QStringLiteral("指标: —"));
    return;
  }
  statusMetricsBrief_->setText(
      QStringLiteral("Cx %1 µm | Cy %2 µm | Dx %3 µm | Dy %4 µm")
          .arg(fmt(metrics_->centroidX_um, 4))
          .arg(fmt(metrics_->centroidY_um, 4))
          .arg(fmt(metrics_->diameterX_um, 4))
          .arg(fmt(metrics_->diameterY_um, 4)));
}

void MainWindow::updateStatusStrip() {
  if (!statusStripMode_) {
    return;
  }
  const bool hasFrame = rawFrame_.has_value();
  const bool camOpen = activeCamera_ && activeCamera_->isOpen();
  if (liveActive_) {
    statusStripMode_->setText(QStringLiteral("实时采集"));
  } else if (camOpen) {
    statusStripMode_->setText(QStringLiteral("相机已连接 · 未采集"));
  } else if (hasFrame) {
    statusStripMode_->setText(QStringLiteral("离线文件测量"));
  } else {
    statusStripMode_->setText(QStringLiteral("离线 · 待命"));
  }

  if (!hasFrame) {
    statusStripSource_->setText(QStringLiteral("尚未载入帧 · 可拖放 CSV/PNG/BMP/TIFF/JPEG"));
  } else {
    statusStripSource_->setText(
        QStringLiteral("%1×%2 · %3")
            .arg(rawFrame_->width)
            .arg(rawFrame_->height)
            .arg(sourcePath_));
  }

  if (!hasFrame) {
    statusStripValidity_->setText(QStringLiteral("NO DATA"));
    statusStripValidity_->setProperty("validity", QStringLiteral("neutral"));
  } else if (metrics_ && metrics_->valid) {
    statusStripValidity_->setText(QStringLiteral("VALID"));
    statusStripValidity_->setProperty("validity", QStringLiteral("good"));
  } else {
    statusStripValidity_->setText(QStringLiteral("INVALID"));
    statusStripValidity_->setProperty("validity", QStringLiteral("bad"));
  }
  statusStripValidity_->style()->unpolish(statusStripValidity_);
  statusStripValidity_->style()->polish(statusStripValidity_);

  if (liveActive_) {
    statusStripHint_->setText(QStringLiteral("LIVE · 指标随帧刷新 · 暂停后可用帧播放条回看"));
  } else if (historyPlaying_) {
    statusStripHint_->setText(QStringLiteral("帧历史回放中 · 剖面与指标随 scrub 同步"));
  } else {
    statusStripHint_->setText(
        QStringLiteral("显示映射（线性/对数）不影响 ISO 数值 · M² 工作站可导入双轴 CSV"));
  }
}

void MainWindow::updateMeasurementChrome() {
  const bool hasFrame = rawFrame_.has_value();
  const bool camOpen = activeCamera_ && activeCamera_->isOpen();
  if (liveActive_) {
    statusModeChip_->setText(QStringLiteral("采集"));
    statusLiveChip_->setVisible(true);
    statusLiveChip_->setText(QStringLiteral("LIVE"));
  } else if (camOpen) {
    statusModeChip_->setText(QStringLiteral("相机"));
    statusLiveChip_->setVisible(false);
  } else if (hasFrame) {
    statusModeChip_->setText(QStringLiteral("分析"));
    statusLiveChip_->setVisible(false);
  } else {
    statusModeChip_->setText(QStringLiteral("离线"));
    statusLiveChip_->setVisible(false);
  }

  if (imageChromeLabel_) {
    const QString cmap =
        (colorMap_ == ColorMap::Grayscale) ? QStringLiteral("灰度") : QStringLiteral("假彩色");
    const QString scale =
        (intensityScale_ == IntensityScale::Log) ? QStringLiteral("对数") : QStringLiteral("线性");
    if (!hasFrame) {
      imageChromeLabel_->setText(
          QStringLiteral("强度图 · %1 · %2 · 十字线剖面 · 无帧").arg(cmap, scale));
    } else {
      imageChromeLabel_->setText(
          QStringLiteral("强度图 · %1/%2 · %3×%4 · 十字 (%5,%6) · %7")
              .arg(cmap)
              .arg(scale)
              .arg(rawFrame_->width)
              .arg(rawFrame_->height)
              .arg(crossX_)
              .arg(crossY_)
              .arg(sourcePath_));
    }
  }
  if (colorLegendLabel_) {
    colorLegendLabel_->setText(intensityScale_ == IntensityScale::Log
                                   ? QStringLiteral("▣ log 低 → 峰值")
                                   : QStringLiteral("▣ 0 → 峰值"));
  }
  updateStatusStrip();
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
  meta.note = "Beam Analyzer";
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
  meta.note = "Beam Analyzer";
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
  meta.note = "Beam Analyzer power";
  if (!beam::io::exportPowerJson(path.toStdString(), *power_, currentCalibration(), meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出功率: %1").arg(path));
}

void MainWindow::exportAnnotatedImage() {
  if (!rawFrame_) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出标注图像"), QStringLiteral("beam_annotated.png"),
      QStringLiteral("PNG (*.png);;BMP (*.bmp);;JPEG (*.jpg);;TIFF (*.tif)"));
  if (path.isEmpty()) {
    return;
  }

  beam::io::AnnotatedExportOptions opt;
  opt.colorMap = (colorMap_ == ColorMap::Grayscale) ? beam::io::ExportColorMap::Grayscale
                                                    : beam::io::ExportColorMap::FalseColor;
  opt.crossX = crossX_;
  opt.crossY = crossY_;
  opt.hasRoi = aoi_.has_value() && useRoiCheck_->isChecked() && !aoi_->empty();
  if (opt.hasRoi) {
    opt.roi = aoi_->bounds;
  }
  if (metrics_ && metrics_->valid) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "Cx:%.2f um Cy:%.2f um Dx:%.2f um Dy:%.2f um",
                  metrics_->centroidX_um, metrics_->centroidY_um, metrics_->diameterX_um,
                  metrics_->diameterY_um);
    opt.overlayLines.emplace_back(buf);
  }

  const QFileInfo fi(path);
  const QString ext = fi.suffix().toLower();
  if (ext == QLatin1String("bmp")) {
    if (!beam::io::exportAnnotatedBmp(path.toStdString(), *rawFrame_,
                                      metrics_ ? &(*metrics_) : nullptr, opt)) {
      QMessageBox::warning(this, QStringLiteral("导出失败"),
                           QStringLiteral("无法写入 BMP。"));
      return;
    }
  } else {
    // Qt path for PNG/JPEG/TIFF with richer overlay text.
    const auto rgb = beam::io::renderAnnotatedRgb(*rawFrame_, metrics_ ? &(*metrics_) : nullptr,
                                                  opt);
    QImage img(rgb.rgb.data(), static_cast<int>(rgb.width), static_cast<int>(rgb.height),
               static_cast<int>(rgb.width * 3), QImage::Format_RGB888);
    QImage copy = img.copy();  // detach from temporary buffer
    if (!copy.save(path)) {
      QMessageBox::warning(this, QStringLiteral("导出失败"),
                           QStringLiteral("无法写入图像（检查扩展名/插件）。"));
      return;
    }
  }
  statusLabel_->setText(QStringLiteral("已导出标注图像: %1").arg(path));
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
  updateMeasurementChrome();
}

void MainWindow::onRoiModeToggled(bool on) {
  imageView_->setAoiShape(aoiShape_);
  imageView_->setInteractionMode(on ? ImageView::InteractionMode::DrawAoi
                                    : ImageView::InteractionMode::None);
  statusLabel_->setText(on ? QStringLiteral("拖拽绘制/拖动 AOI…") : QStringLiteral("就绪"));
}

void MainWindow::onClearRoi() {
  aoi_.reset();
  imageView_->setAoi(std::nullopt);
  useRoiCheck_->setChecked(false);
  if (roiModeAction_->isChecked()) {
    roiModeAction_->setChecked(false);
  }
  onRecompute();
}

void MainWindow::onRoiChanged(beam::core::RectRoi roi) {
  beam::core::AoiRegion a = beam::core::aoiFromRect(roi);
  a.shape = aoiShape_;
  onAoiChanged(a);
}

void MainWindow::onAoiChanged(beam::core::AoiRegion aoi) {
  aoi.shape = aoiShape_;
  aoi_ = aoi;
  useRoiCheck_->setChecked(true);
  if (roiModeAction_->isChecked()) {
    roiModeAction_->setChecked(false);
  }
  onRecompute();
}

void MainWindow::onAoiShapeChanged(int index) {
  aoiShape_ = static_cast<beam::core::AoiShape>(aoiShapeBox_->itemData(index).toInt());
  imageView_->setAoiShape(aoiShape_);
  if (aoi_) {
    aoi_->shape = aoiShape_;
    imageView_->setAoi(aoi_);
    if (useRoiCheck_->isChecked()) {
      onRecompute();
    }
  }
}

void MainWindow::onAutoAoi() {
  if (!rawFrame_) {
    QMessageBox::information(this, QStringLiteral("自动 AOI"),
                             QStringLiteral("请先加载一帧。"));
    return;
  }
  // Compute on full frame first, then place AOI.
  const auto m = beam::core::computeIso11146(*rawFrame_);
  if (!m.valid) {
    QMessageBox::warning(this, QStringLiteral("自动 AOI"),
                         QStringLiteral("无法从当前帧估计质心/直径。"));
    return;
  }
  aoi_ = beam::core::autoAoiFromCentroid(m.centroidX_um, m.centroidY_um, m.diameterX_um,
                                         m.diameterY_um, *rawFrame_, aoiShape_, 2.0);
  imageView_->setAoi(aoi_);
  useRoiCheck_->setChecked(true);
  onRecompute();
  statusLabel_->setText(QStringLiteral("已自动放置 AOI（≈ ±dσ）"));
}

void MainWindow::onCrosshairChanged(int x, int y) {
  crossX_ = x;
  crossY_ = y;
  refreshProfiles();
  refreshMetricsPanel();
  updateMeasurementChrome();
}

void MainWindow::onBackgroundModeChanged(int) {
  const auto mode =
      static_cast<beam::core::BackgroundMode>(backgroundBox_->currentData().toInt());
  bgConstantSpin_->setEnabled(mode == beam::core::BackgroundMode::Constant);
  onRecompute();
}

void MainWindow::onCalibrationChanged() {
  if (!analysisFrame_) {
    return;
  }
  power_ = beam::core::computePowerMetrics(*analysisFrame_, currentCalibration());
  refreshMetricsPanel();
  updateStatusBarDetails();
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

void MainWindow::openAnalysis() {
  if (!rawFrame_ || !metrics_ || !metrics_->valid) {
    QMessageBox::information(this, QStringLiteral("完整分析"),
                             QStringLiteral("请先加载有效光斑帧。"));
    return;
  }
  if (!analysisDialog_) {
    analysisDialog_ = new AnalysisDialog(this);
  }
  analysisDialog_->setFrame(*rawFrame_, *metrics_, power_, aoi_, wavelengthSpin_->value());
  analysisDialog_->show();
  analysisDialog_->raise();
  analysisDialog_->activateWindow();
}

void MainWindow::toggleTheme() {
  setTheme(theme_ == AppTheme::Dark ? AppTheme::Light : AppTheme::Dark);
}

void MainWindow::setTheme(AppTheme theme) {
  theme_ = theme;
  qApp->setStyleSheet(themeStyleSheet(theme_));
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
  s.setValue(QStringLiteral("theme"), static_cast<int>(theme_));
  s.setValue(QStringLiteral("recentFiles"), recentFiles_);
  s.setValue(QStringLiteral("geometry"), saveGeometry());
  s.setValue(QStringLiteral("windowState"), saveState());
}

void MainWindow::loadSettings() {
  QSettings s(QStringLiteral("BeamAnalyzer"), QStringLiteral("BeamAnalyzer"));
  const int cm =
      s.value(QStringLiteral("colorMap"), static_cast<int>(ColorMap::FalseColor)).toInt();
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
  theme_ = static_cast<AppTheme>(
      s.value(QStringLiteral("theme"), static_cast<int>(AppTheme::Dark)).toInt());
  recentFiles_ = s.value(QStringLiteral("recentFiles")).toStringList();
  rebuildRecentMenu();
  restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
  restoreState(s.value(QStringLiteral("windowState")).toByteArray());
  // Clamp a corrupted/legacy dock width so maximize never leaves an empty gutter.
  if (metricsDock_ && !metricsDock_->isFloating() && metricsDock_->isVisible()) {
    const int w = metricsDock_->width();
    if (w < 200 || w > 640) {
      resizeDocks({metricsDock_}, {300}, Qt::Horizontal);
    }
  }
}

}  // namespace beam::ui
