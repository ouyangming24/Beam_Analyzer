#include "MainWindow.hpp"

#include "beam/io/CsvImporter.hpp"
#include "beam/io/ResultExporter.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace beam::ui {
namespace {

QString fmt(double v, int digits = 4) {
  return QString::number(v, 'g', digits);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Beam Analyzer — Phase 1 (离线 CSV)"));
  resize(1100, 700);
  rebuildUi();
  simCamera_ = beam::camera::createSimulatedCamera(160, 120, 5.0);
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
  exportJsonAction_->setEnabled(false);
  exportCsvAction_->setEnabled(false);

  tool->addSeparator();
  tool->addWidget(new QLabel(QStringLiteral("  色图: ")));
  colorMapBox_ = new QComboBox;
  colorMapBox_->addItem(QStringLiteral("假彩色"), static_cast<int>(ColorMap::FalseColor));
  colorMapBox_->addItem(QStringLiteral("灰度"), static_cast<int>(ColorMap::Grayscale));
  connect(colorMapBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::onColorMapChanged);
  tool->addWidget(colorMapBox_);

  auto* splitter = new QSplitter(Qt::Horizontal);

  imageLabel_ = new QLabel(QStringLiteral("请打开 CSV 或加载模拟帧"));
  imageLabel_->setAlignment(Qt::AlignCenter);
  imageLabel_->setMinimumSize(480, 360);
  imageLabel_->setStyleSheet(QStringLiteral("QLabel { background: #1a1a1a; color: #ccc; }"));
  imageLabel_->setScaledContents(false);

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setWidget(imageLabel_);
  splitter->addWidget(scroll);

  auto* right = new QWidget;
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->addWidget(new QLabel(QStringLiteral("ISO 11146 指标")));
  metricsView_ = new QTextEdit;
  metricsView_->setReadOnly(true);
  metricsView_->setFont(QFont(QStringLiteral("Consolas"), 10));
  metricsView_->setPlainText(QStringLiteral("尚无数据。"));
  rightLayout->addWidget(metricsView_);

  auto* hint = new QLabel(
      QStringLiteral("Phase 1：离线 CSV MVP。\n"
                     "不宣称完整 Cinogy 对等。\n"
                     "相机接口已预留（Null / Simulated）。"));
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color: #666;"));
  rightLayout->addWidget(hint);

  splitter->addWidget(right);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);
  setCentralWidget(splitter);

  statusLabel_ = new QLabel(QStringLiteral("就绪"));
  statusBar()->addWidget(statusLabel_, 1);
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
  frame_ = std::move(frame);
  sourcePath_ = sourceLabel;
  metrics_ = beam::core::computeIso11146(*frame_);
  exportJsonAction_->setEnabled(true);
  exportCsvAction_->setEnabled(true);
  refreshImage();
  refreshMetricsPanel();
  statusLabel_->setText(
      QStringLiteral("已加载 %1×%2 — %3")
          .arg(frame_->width)
          .arg(frame_->height)
          .arg(sourceLabel));
}

void MainWindow::refreshImage() {
  if (!frame_) {
    return;
  }
  double mn = 0;
  double mx = 0;
  const QImage img = frameToImage(*frame_, colorMap_, &mn, &mx);
  if (img.isNull()) {
    return;
  }

  // Upscale small frames for visibility while keeping nearest-neighbor feel.
  const int targetW = std::max(img.width(), 480);
  const int targetH = std::max(img.height(), 360);
  const QImage shown = img.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::FastTransformation);
  imageLabel_->setPixmap(QPixmap::fromImage(shown));
  imageLabel_->setText({});
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
  text += QStringLiteral("\n像素尺寸: %1 × %2 µm")
              .arg(fmt(frame_->pixelSizeX_um))
              .arg(fmt(frame_->pixelSizeY_um));
  metricsView_->setPlainText(text);
}

bool MainWindow::ensureMetrics() const {
  return frame_.has_value() && metrics_.has_value() && metrics_->valid;
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
  meta.note = "Beam Analyzer Phase 1";
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
  meta.note = "Beam Analyzer Phase 1";
  if (!beam::io::exportMetricsCsv(path.toStdString(), *metrics_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  statusLabel_->setText(QStringLiteral("已导出 CSV: %1").arg(path));
}

void MainWindow::onColorMapChanged(int index) {
  colorMap_ = static_cast<ColorMap>(colorMapBox_->itemData(index).toInt());
  refreshImage();
}

}  // namespace beam::ui
