#include "M2ScanDialog.hpp"

#include "ProfilePlot.hpp"
#include "beam/core/CausticCsv.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/io/ResultExporter.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>

namespace beam::ui {
namespace {

std::vector<double> fitCurveZ(const std::vector<double>& zSamples, double padFrac = 0.08) {
  if (zSamples.size() < 2) {
    return {};
  }
  double z0 = *std::min_element(zSamples.begin(), zSamples.end());
  double z1 = *std::max_element(zSamples.begin(), zSamples.end());
  const double pad = padFrac * std::max(1e-6, z1 - z0);
  z0 -= pad;
  z1 += pad;
  std::vector<double> fz;
  constexpr int n = 96;
  fz.reserve(n);
  for (int i = 0; i < n; ++i) {
    fz.push_back(z0 + (z1 - z0) * static_cast<double>(i) / (n - 1));
  }
  return fz;
}

QString fmtG(double v, int digits = 4) {
  if (!std::isfinite(v)) {
    return QStringLiteral("—");
  }
  return QString::number(v, 'g', digits);
}

}  // namespace

M2ScanDialog::M2ScanDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("M² 工作站 — 双轴焦散 / 残差 / 报告"));
  resize(1180, 820);
  setMinimumSize(940, 680);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  workflowLabel_ = new QLabel(QStringLiteral(
      "工作流：① 元信息与波长 → ② 添加测点 / 导入双轴 CSV / 仿真 → ③ 拟合 → ④ 审残差与覆盖 → "
      "⑤ 导出报告（分析记录，非计量证书）"));
  workflowLabel_->setObjectName(QStringLiteral("SectionHeader"));
  workflowLabel_->setWordWrap(true);
  root->addWidget(workflowLabel_);

  auto* mainSplit = new QSplitter(Qt::Horizontal);
  mainSplit->setChildrenCollapsible(false);

  auto* left = new QWidget;
  auto* leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(6);

  auto* metaBox = new QGroupBox(QStringLiteral("报告元信息"));
  auto* metaForm = new QFormLayout(metaBox);
  metaForm->setSpacing(3);
  metaForm->setContentsMargins(8, 10, 8, 6);
  sampleEdit_ = new QLineEdit;
  sampleEdit_->setPlaceholderText(QStringLiteral("样品 / 测量名称"));
  operatorEdit_ = new QLineEdit;
  operatorEdit_->setPlaceholderText(QStringLiteral("操作员"));
  deviceEdit_ = new QLineEdit;
  deviceEdit_->setPlaceholderText(QStringLiteral("仪器 / 设备标识"));
  calibrationEdit_ = new QLineEdit;
  calibrationEdit_->setPlaceholderText(QStringLiteral("标定记录 ID"));
  environmentEdit_ = new QLineEdit;
  environmentEdit_->setPlaceholderText(QStringLiteral("环境 / 曝光 / 衰减备注"));
  metaForm->addRow(QStringLiteral("样品"), sampleEdit_);
  metaForm->addRow(QStringLiteral("操作员"), operatorEdit_);
  metaForm->addRow(QStringLiteral("设备"), deviceEdit_);
  metaForm->addRow(QStringLiteral("标定"), calibrationEdit_);
  metaForm->addRow(QStringLiteral("环境"), environmentEdit_);
  leftLayout->addWidget(metaBox);

  auto* formBox = new QGroupBox(QStringLiteral("测点输入"));
  auto* form = new QFormLayout(formBox);
  form->setSpacing(4);
  form->setContentsMargins(8, 12, 8, 6);
  wavelengthSpin_ = new QDoubleSpinBox;
  wavelengthSpin_->setRange(1.0, 1e6);
  wavelengthSpin_->setDecimals(3);
  wavelengthSpin_->setSuffix(QStringLiteral(" nm"));
  wavelengthSpin_->setValue(1064.0);
  form->addRow(QStringLiteral("波长"), wavelengthSpin_);

  zSpin_ = new QDoubleSpinBox;
  zSpin_->setRange(-1e6, 1e6);
  zSpin_->setDecimals(4);
  zSpin_->setSuffix(QStringLiteral(" mm"));
  form->addRow(QStringLiteral("z（手动）"), zSpin_);

  dxSpin_ = new QDoubleSpinBox;
  dxSpin_->setRange(1e-6, 1e9);
  dxSpin_->setDecimals(4);
  dxSpin_->setSuffix(QStringLiteral(" µm"));
  dxSpin_->setValue(100.0);
  form->addRow(QStringLiteral("Dx"), dxSpin_);

  dySpin_ = new QDoubleSpinBox;
  dySpin_->setRange(1e-6, 1e9);
  dySpin_->setDecimals(4);
  dySpin_->setSuffix(QStringLiteral(" µm"));
  dySpin_->setValue(100.0);
  form->addRow(QStringLiteral("Dy"), dySpin_);

  sourceEdit_ = new QLineEdit;
  sourceEdit_->setPlaceholderText(QStringLiteral("来源 / 排除原因（可选）"));
  form->addRow(QStringLiteral("来源"), sourceEdit_);
  leftLayout->addWidget(formBox);

  auto* motorBox = new QGroupBox(QStringLiteral("电机规划（粗扫 → 精扫）"));
  auto* motorForm = new QFormLayout(motorBox);
  motorForm->setSpacing(4);
  motorForm->setContentsMargins(8, 12, 8, 6);
  coarseStartSpin_ = new QDoubleSpinBox;
  coarseStartSpin_->setRange(-1e6, 1e6);
  coarseStartSpin_->setDecimals(3);
  coarseStartSpin_->setSuffix(QStringLiteral(" mm"));
  coarseStartSpin_->setValue(-40.0);
  motorForm->addRow(QStringLiteral("粗扫起点"), coarseStartSpin_);

  coarseEndSpin_ = new QDoubleSpinBox;
  coarseEndSpin_->setRange(-1e6, 1e6);
  coarseEndSpin_->setDecimals(3);
  coarseEndSpin_->setSuffix(QStringLiteral(" mm"));
  coarseEndSpin_->setValue(40.0);
  motorForm->addRow(QStringLiteral("粗扫终点"), coarseEndSpin_);

  coarseStepSpin_ = new QDoubleSpinBox;
  coarseStepSpin_->setRange(1e-6, 1e6);
  coarseStepSpin_->setDecimals(3);
  coarseStepSpin_->setSuffix(QStringLiteral(" mm"));
  coarseStepSpin_->setValue(10.0);
  motorForm->addRow(QStringLiteral("粗扫步长"), coarseStepSpin_);

  fineHalfSpin_ = new QDoubleSpinBox;
  fineHalfSpin_->setRange(1e-6, 1e6);
  fineHalfSpin_->setDecimals(3);
  fineHalfSpin_->setSuffix(QStringLiteral(" mm"));
  fineHalfSpin_->setValue(8.0);
  motorForm->addRow(QStringLiteral("精扫半程 ±"), fineHalfSpin_);

  fineStepSpin_ = new QDoubleSpinBox;
  fineStepSpin_->setRange(1e-6, 1e6);
  fineStepSpin_->setDecimals(3);
  fineStepSpin_->setSuffix(QStringLiteral(" mm"));
  fineStepSpin_->setValue(1.0);
  motorForm->addRow(QStringLiteral("精扫步长"), fineStepSpin_);

  runFineCheck_ = new QCheckBox(QStringLiteral("粗扫后自动精扫（腰斑附近）"));
  runFineCheck_->setChecked(true);
  motorForm->addRow(runFineCheck_);

  synthM2Spin_ = new QDoubleSpinBox;
  synthM2Spin_->setRange(0.5, 100.0);
  synthM2Spin_->setDecimals(3);
  synthM2Spin_->setValue(1.0);
  motorForm->addRow(QStringLiteral("仿真 M²"), synthM2Spin_);

  synthD0Spin_ = new QDoubleSpinBox;
  synthD0Spin_->setRange(1e-3, 1e6);
  synthD0Spin_->setDecimals(3);
  synthD0Spin_->setSuffix(QStringLiteral(" µm"));
  synthD0Spin_->setValue(100.0);
  motorForm->addRow(QStringLiteral("仿真腰斑直径"), synthD0Spin_);
  leftLayout->addWidget(motorBox);

  auto* row = new QHBoxLayout;
  row->setSpacing(4);
  auto* addBtn = new QPushButton(QStringLiteral("添加"));
  auto* seedBtn = new QPushButton(QStringLiteral("当前帧"));
  auto* importBtn = new QPushButton(QStringLiteral("导入 CSV"));
  auto* removeBtn = new QPushButton(QStringLiteral("删除"));
  auto* clearBtn = new QPushButton(QStringLiteral("清空"));
  auto* planBtn = new QPushButton(QStringLiteral("位置列表"));
  auto* simBtn = new QPushButton(QStringLiteral("仿真扫描"));
  simBtn->setObjectName(QStringLiteral("AccentButton"));
  importBtn->setToolTip(QStringLiteral("支持 z_mm,Dx_um,Dy_um 或 z_mm,wx_um,wy_um[,included,note]"));
  row->addWidget(addBtn);
  row->addWidget(seedBtn);
  row->addWidget(importBtn);
  row->addWidget(removeBtn);
  row->addWidget(clearBtn);
  row->addWidget(planBtn);
  row->addWidget(simBtn);
  leftLayout->addLayout(row);

  table_ = new QTableWidget(0, 5);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("z (mm)"), QStringLiteral("Dx (µm)"), QStringLiteral("Dy (µm)"),
       QStringLiteral("采用"), QStringLiteral("来源/备注")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table_->setAlternatingRowColors(true);
  table_->verticalHeader()->setVisible(false);
  table_->setMinimumHeight(140);
  leftLayout->addWidget(table_, 1);
  connect(table_, &QTableWidget::cellChanged, this, &M2ScanDialog::onTableCellChanged);

  mainSplit->addWidget(left);

  auto* right = new QWidget;
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(6);

  auto* summaryBox = new QGroupBox(QStringLiteral("双轴 M² 摘要"));
  auto* summaryLay = new QHBoxLayout(summaryBox);
  summaryLay->setSpacing(12);
  auto makeSummary = [](const QString& title) {
    auto* col = new QVBoxLayout;
    auto* t = new QLabel(title);
    t->setObjectName(QStringLiteral("SectionHeader"));
    auto* v = new QLabel(QStringLiteral("—"));
    v->setObjectName(QStringLiteral("MetricValue"));
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setWordWrap(true);
    col->addWidget(t);
    col->addWidget(v);
    col->addStretch(1);
    auto* w = new QWidget;
    w->setLayout(col);
    return std::pair<QWidget*, QLabel*>{w, v};
  };
  auto sx = makeSummary(QStringLiteral("M²_X / 腰斑"));
  auto sy = makeSummary(QStringLiteral("M²_Y / 腰斑"));
  auto sc = makeSummary(QStringLiteral("合成 / 质量"));
  summaryMx_ = sx.second;
  summaryMy_ = sy.second;
  summaryComb_ = sc.second;
  summaryLay->addWidget(sx.first, 1);
  summaryLay->addWidget(sy.first, 1);
  summaryLay->addWidget(sc.first, 1);
  rightLayout->addWidget(summaryBox);

  viewTabs_ = new QTabWidget;
  auto* overlayPage = new QWidget;
  auto* overlayLay = new QVBoxLayout(overlayPage);
  overlayLay->setContentsMargins(4, 4, 4, 4);
  dzPlotOverlay_ = new ProfilePlot;
  dzPlotOverlay_->setMinimumHeight(220);
  overlayLay->addWidget(dzPlotOverlay_, 1);
  auto* overlayHint = new QLabel(
      QStringLiteral("叠图：蓝=Dx 测 / 青虚线=Dx 拟合；绿=Dy 测 / 粉虚线=Dy 拟合（虚线含外推）"));
  overlayHint->setWordWrap(true);
  overlayHint->setStyleSheet(QStringLiteral("color:#8892a4;font-size:11px;"));
  overlayLay->addWidget(overlayHint);
  viewTabs_->addTab(overlayPage, QStringLiteral("双轴叠图"));

  auto* causticPage = new QWidget;
  auto* causticLay = new QVBoxLayout(causticPage);
  causticLay->setContentsMargins(4, 4, 4, 4);
  causticLay->setSpacing(4);
  dzPlotX_ = new ProfilePlot;
  dzPlotY_ = new ProfilePlot;
  dzPlotX_->setMinimumHeight(140);
  dzPlotY_->setMinimumHeight(140);
  causticLay->addWidget(dzPlotX_, 1);
  causticLay->addWidget(dzPlotY_, 1);
  viewTabs_->addTab(causticPage, QStringLiteral("分轴焦散"));

  auto* residualPage = new QWidget;
  auto* residualLay = new QVBoxLayout(residualPage);
  residualLay->setContentsMargins(4, 4, 4, 4);
  residualLay->setSpacing(4);
  residualPlot_ = new ProfilePlot;
  residualPlot_->setMinimumHeight(120);
  residualPlot_->setZeroLineEnabled(true);
  residualLay->addWidget(residualPlot_, 1);
  residualTable_ = new QTableWidget(0, 7);
  residualTable_->setHorizontalHeaderLabels(
      {QStringLiteral("z (mm)"), QStringLiteral("Dx 测"), QStringLiteral("Dx 拟合"),
       QStringLiteral("Δx"), QStringLiteral("Dy 测"), QStringLiteral("Dy 拟合"),
       QStringLiteral("Δy")});
  residualTable_->horizontalHeader()->setStretchLastSection(true);
  residualTable_->setAlternatingRowColors(true);
  residualTable_->verticalHeader()->setVisible(false);
  residualTable_->setMinimumHeight(120);
  residualLay->addWidget(residualTable_, 1);
  viewTabs_->addTab(residualPage, QStringLiteral("残差"));

  auto* diagPage = new QWidget;
  auto* diagLay = new QVBoxLayout(diagPage);
  diagnosticsEdit_ = new QPlainTextEdit;
  diagnosticsEdit_->setReadOnly(true);
  diagnosticsEdit_->setPlaceholderText(QStringLiteral("拟合后显示采样覆盖与工程诊断…"));
  diagLay->addWidget(diagnosticsEdit_, 1);
  viewTabs_->addTab(diagPage, QStringLiteral("覆盖诊断"));

  auto* detailPage = new QWidget;
  auto* detailLay = new QVBoxLayout(detailPage);
  resultLabel_ = new QLabel(
      QStringLiteral("添加 ≥3 个轴向测点，导入双轴 CSV，或配置粗/精扫后运行仿真。"
                     "详见 docs/ALGORITHM.md。"));
  resultLabel_->setWordWrap(true);
  resultLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  resultLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  detailLay->addWidget(resultLabel_, 1);
  viewTabs_->addTab(detailPage, QStringLiteral("详细结果"));
  rightLayout->addWidget(viewTabs_, 1);

  auto* fitRow = new QHBoxLayout;
  fitRow->setSpacing(4);
  auto* fitBtn = new QPushButton(QStringLiteral("拟合 M²"));
  fitBtn->setObjectName(QStringLiteral("AccentButton"));
  auto* mdBtn = new QPushButton(QStringLiteral("Markdown…"));
  auto* htmlBtn = new QPushButton(QStringLiteral("HTML…"));
  auto* jsonBtn = new QPushButton(QStringLiteral("JSON…"));
  auto* csvBtn = new QPushButton(QStringLiteral("D(z) CSV…"));
  auto* resCsvBtn = new QPushButton(QStringLiteral("残差 CSV…"));
  auto* bundleBtn = new QPushButton(QStringLiteral("导出报告包…"));
  fitRow->addWidget(fitBtn);
  fitRow->addWidget(mdBtn);
  fitRow->addWidget(htmlBtn);
  fitRow->addWidget(jsonBtn);
  fitRow->addWidget(csvBtn);
  fitRow->addWidget(resCsvBtn);
  fitRow->addWidget(bundleBtn);
  fitRow->addStretch();
  rightLayout->addLayout(fitRow);

  mainSplit->addWidget(right);
  mainSplit->setStretchFactor(0, 2);
  mainSplit->setStretchFactor(1, 3);
  mainSplit->setSizes({440, 700});
  root->addWidget(mainSplit, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  root->addWidget(buttons);

  connect(addBtn, &QPushButton::clicked, this, &M2ScanDialog::addPoint);
  connect(seedBtn, &QPushButton::clicked, this, &M2ScanDialog::addFromSeed);
  connect(importBtn, &QPushButton::clicked, this, &M2ScanDialog::importCsv);
  connect(removeBtn, &QPushButton::clicked, this, &M2ScanDialog::removeSelected);
  connect(clearBtn, &QPushButton::clicked, this, &M2ScanDialog::clearPoints);
  connect(planBtn, &QPushButton::clicked, this, &M2ScanDialog::generatePlan);
  connect(simBtn, &QPushButton::clicked, this, &M2ScanDialog::runSimulatedScan);
  connect(fitBtn, &QPushButton::clicked, this, &M2ScanDialog::runFit);
  connect(mdBtn, &QPushButton::clicked, this, &M2ScanDialog::exportMarkdown);
  connect(htmlBtn, &QPushButton::clicked, this, &M2ScanDialog::exportHtml);
  connect(jsonBtn, &QPushButton::clicked, this, &M2ScanDialog::exportJson);
  connect(csvBtn, &QPushButton::clicked, this, &M2ScanDialog::exportDzCsv);
  connect(resCsvBtn, &QPushButton::clicked, this, &M2ScanDialog::exportResidualsCsv);
  connect(bundleBtn, &QPushButton::clicked, this, &M2ScanDialog::exportReportBundle);
}

void M2ScanDialog::setDefaultWavelength(double nm) {
  wavelengthSpin_->setValue(nm);
}

void M2ScanDialog::seedFromCurrentFrame(double diameterX_um,
                                        double diameterY_um,
                                        const QString& sourceLabel) {
  seedDx_ = diameterX_um;
  seedDy_ = diameterY_um;
  seedSource_ = sourceLabel;
  hasSeed_ = diameterX_um > 0.0 && diameterY_um > 0.0;
  if (hasSeed_) {
    dxSpin_->setValue(diameterX_um);
    dySpin_->setValue(diameterY_um);
    sourceEdit_->setText(sourceLabel);
  }
}

beam::core::ScanSessionConfig M2ScanDialog::sessionConfig() const {
  beam::core::ScanSessionConfig cfg;
  cfg.wavelength_nm = wavelengthSpin_->value();
  cfg.coarse.zStart_mm = coarseStartSpin_->value();
  cfg.coarse.zEnd_mm = coarseEndSpin_->value();
  cfg.coarse.step_mm = coarseStepSpin_->value();
  cfg.fine.halfRange_mm = fineHalfSpin_->value();
  cfg.fine.step_mm = fineStepSpin_->value();
  cfg.runFineAfterCoarse = runFineCheck_->isChecked();
  return cfg;
}

beam::io::ExportMeta M2ScanDialog::reportMeta() const {
  beam::io::ExportMeta meta;
  meta.note = "Beam Analyzer M2 scan";
  meta.sampleName = sampleEdit_->text().trimmed().toStdString();
  meta.operatorName = operatorEdit_->text().trimmed().toStdString();
  meta.deviceId = deviceEdit_->text().trimmed().toStdString();
  meta.calibrationId = calibrationEdit_->text().trimmed().toStdString();
  meta.environmentNote = environmentEdit_->text().trimmed().toStdString();
  return meta;
}

void M2ScanDialog::refreshTable() {
  updatingTable_ = true;
  const QSignalBlocker b(table_);
  table_->setRowCount(static_cast<int>(points_.size()));
  for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
    const auto& p = points_[static_cast<std::size_t>(i)];
    table_->setItem(i, 0, new QTableWidgetItem(QString::number(p.z_mm, 'g', 8)));
    table_->setItem(i, 1, new QTableWidgetItem(QString::number(p.diameterX_um, 'g', 8)));
    table_->setItem(i, 2, new QTableWidgetItem(QString::number(p.diameterY_um, 'g', 8)));
    auto* validItem = new QTableWidgetItem(p.valid ? QStringLiteral("1") : QStringLiteral("0"));
    validItem->setFlags(validItem->flags() | Qt::ItemIsEditable);
    table_->setItem(i, 3, validItem);
    table_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(p.sourceLabel)));
  }
  updatingTable_ = false;
}

void M2ScanDialog::syncPointsFromTable() {
  if (updatingTable_) {
    return;
  }
  for (int i = 0; i < table_->rowCount() && i < static_cast<int>(points_.size()); ++i) {
    auto* zItem = table_->item(i, 0);
    auto* dxItem = table_->item(i, 1);
    auto* dyItem = table_->item(i, 2);
    auto* vItem = table_->item(i, 3);
    auto* sItem = table_->item(i, 4);
    auto& p = points_[static_cast<std::size_t>(i)];
    if (zItem) {
      p.z_mm = zItem->text().toDouble();
    }
    if (dxItem) {
      p.diameterX_um = dxItem->text().toDouble();
    }
    if (dyItem) {
      p.diameterY_um = dyItem->text().toDouble();
    }
    if (vItem) {
      const QString t = vItem->text().trimmed().toLower();
      p.valid = !(t == QLatin1String("0") || t == QLatin1String("false") ||
                  t == QLatin1String("否") || t == QLatin1String("n"));
    }
    if (sItem) {
      p.sourceLabel = sItem->text().toStdString();
    }
  }
}

void M2ScanDialog::onTableCellChanged(int, int) {
  if (updatingTable_) {
    return;
  }
  syncPointsFromTable();
  lastResult_ = {};
  updateDzPlots();
  updateResultSummary();
  updateDiagnostics();
}

void M2ScanDialog::updateDzPlots() {
  const auto& pts = lastResult_.points.empty() ? points_ : lastResult_.points;
  std::vector<double> z;
  std::vector<double> dx;
  std::vector<double> dy;
  for (const auto& p : pts) {
    if (!p.valid) {
      continue;
    }
    z.push_back(p.z_mm);
    dx.push_back(p.diameterX_um);
    dy.push_back(p.diameterY_um);
  }

  std::vector<PlotSeries> sx;
  std::vector<PlotSeries> sy;
  std::vector<PlotSeries> so;
  if (z.size() >= 2) {
    PlotSeries mx;
    mx.x = z;
    mx.y = dx;
    mx.color = QColor(33, 117, 181);
    mx.markers = true;
    mx.penWidth = 1.5;
    mx.legend = QStringLiteral("Dx 测");
    sx.push_back(mx);
    so.push_back(mx);

    PlotSeries my;
    my.x = z;
    my.y = dy;
    my.color = QColor(209, 132, 37);
    my.markers = true;
    my.penWidth = 1.5;
    my.legend = QStringLiteral("Dy 测");
    sy.push_back(my);
    so.push_back(my);

    const auto fz = fitCurveZ(z);
    if (lastResult_.fitX.valid && !fz.empty()) {
      PlotSeries fx;
      fx.x = fz;
      fx.y.reserve(fz.size());
      for (double zz : fz) {
        fx.y.push_back(beam::core::hyperbolicDiameter_um(
            zz, lastResult_.fitX.waistDiameter_um, lastResult_.fitX.waistZ_mm,
            lastResult_.fitX.m2, lastResult_.wavelength_nm));
      }
      fx.color = QColor(64, 180, 220);
      fx.penStyle = Qt::DashLine;
      fx.penWidth = 1.4;
      fx.legend = QStringLiteral("Dx 拟合");
      sx.push_back(fx);
      so.push_back(std::move(fx));
    }
    if (lastResult_.fitY.valid && !fz.empty()) {
      PlotSeries fy;
      fy.x = fz;
      fy.y.reserve(fz.size());
      for (double zz : fz) {
        fy.y.push_back(beam::core::hyperbolicDiameter_um(
            zz, lastResult_.fitY.waistDiameter_um, lastResult_.fitY.waistZ_mm,
            lastResult_.fitY.m2, lastResult_.wavelength_nm));
      }
      fy.color = QColor(255, 120, 160);
      fy.penStyle = Qt::DashLine;
      fy.penWidth = 1.4;
      fy.legend = QStringLiteral("Dy 拟合");
      sy.push_back(fy);
      so.push_back(std::move(fy));
    }
  }

  dzPlotOverlay_->setMultiSeries(so, QStringLiteral("双轴焦散叠图 D(z)"), QStringLiteral("mm"),
                                 QStringLiteral("µm"));
  dzPlotX_->setMultiSeries(sx, QStringLiteral("焦散 X — Dx(z)"), QStringLiteral("mm"),
                           QStringLiteral("µm"));
  dzPlotY_->setMultiSeries(sy, QStringLiteral("焦散 Y — Dy(z)"), QStringLiteral("mm"),
                           QStringLiteral("µm"));
  refreshResidualTable();
}

void M2ScanDialog::refreshResidualTable() {
  std::vector<PlotSeries> resSeries;
  residualTable_->setRowCount(0);

  const auto& rx = lastResult_.fitX.residuals;
  const auto& ry = lastResult_.fitY.residuals;
  const std::size_t n = std::max(rx.size(), ry.size());
  residualPlot_->setZeroLineEnabled(true);
  if (n == 0) {
    residualPlot_->clearProfile();
    residualPlot_->setZeroLineEnabled(true);
    return;
  }

  residualTable_->setRowCount(static_cast<int>(n));
  std::vector<double> zx;
  std::vector<double> resX;
  std::vector<double> zy;
  std::vector<double> resY;

  for (std::size_t i = 0; i < n; ++i) {
    const bool hasX = i < rx.size();
    const bool hasY = i < ry.size();
    const double z = hasX ? rx[i].z_mm : (hasY ? ry[i].z_mm : 0.0);
    residualTable_->setItem(static_cast<int>(i), 0,
                            new QTableWidgetItem(QString::number(z, 'g', 6)));
    if (hasX) {
      residualTable_->setItem(static_cast<int>(i), 1,
                              new QTableWidgetItem(fmtG(rx[i].measured_um)));
      residualTable_->setItem(static_cast<int>(i), 2,
                              new QTableWidgetItem(fmtG(rx[i].fitted_um)));
      residualTable_->setItem(static_cast<int>(i), 3,
                              new QTableWidgetItem(fmtG(rx[i].residual_um)));
      zx.push_back(rx[i].z_mm);
      resX.push_back(rx[i].residual_um);
    } else {
      for (int c = 1; c <= 3; ++c) {
        residualTable_->setItem(static_cast<int>(i), c, new QTableWidgetItem(QStringLiteral("—")));
      }
    }
    if (hasY) {
      residualTable_->setItem(static_cast<int>(i), 4,
                              new QTableWidgetItem(fmtG(ry[i].measured_um)));
      residualTable_->setItem(static_cast<int>(i), 5,
                              new QTableWidgetItem(fmtG(ry[i].fitted_um)));
      residualTable_->setItem(static_cast<int>(i), 6,
                              new QTableWidgetItem(fmtG(ry[i].residual_um)));
      zy.push_back(ry[i].z_mm);
      resY.push_back(ry[i].residual_um);
    } else {
      for (int c = 4; c <= 6; ++c) {
        residualTable_->setItem(static_cast<int>(i), c, new QTableWidgetItem(QStringLiteral("—")));
      }
    }
  }

  if (zx.size() >= 2) {
    PlotSeries sx;
    sx.x = zx;
    sx.y = resX;
    sx.color = QColor(33, 117, 181);
    sx.markers = true;
    sx.legend = QStringLiteral("ΔDx");
    resSeries.push_back(std::move(sx));
  }
  if (zy.size() >= 2) {
    PlotSeries sy;
    sy.x = zy;
    sy.y = resY;
    sy.color = QColor(209, 132, 37);
    sy.markers = true;
    sy.legend = QStringLiteral("ΔDy");
    resSeries.push_back(std::move(sy));
  }
  residualPlot_->setMultiSeries(resSeries, QStringLiteral("残差 residual = 测 − 拟合（零线参考）"),
                                QStringLiteral("mm"), QStringLiteral("µm"));
  residualPlot_->setZeroLineEnabled(true);
}

void M2ScanDialog::updateResultSummary() {
  if (!lastResult_.valid) {
    summaryMx_->setText(QStringLiteral("—"));
    summaryMy_->setText(QStringLiteral("—"));
    summaryComb_->setText(QStringLiteral("未拟合"));
    return;
  }
  summaryMx_->setText(QStringLiteral("M² = %1 ± %2\nD0 = %3 µm\nz0 = %4 mm\nzR = %5 mm")
                          .arg(fmtG(lastResult_.fitX.m2))
                          .arg(fmtG(lastResult_.fitX.m2Uncertainty, 3))
                          .arg(fmtG(lastResult_.fitX.waistDiameter_um))
                          .arg(fmtG(lastResult_.fitX.waistZ_mm))
                          .arg(fmtG(lastResult_.fitX.rayleighRange_mm)));
  summaryMy_->setText(QStringLiteral("M² = %1 ± %2\nD0 = %3 µm\nz0 = %4 mm\nzR = %5 mm")
                          .arg(fmtG(lastResult_.fitY.m2))
                          .arg(fmtG(lastResult_.fitY.m2Uncertainty, 3))
                          .arg(fmtG(lastResult_.fitY.waistDiameter_um))
                          .arg(fmtG(lastResult_.fitY.waistZ_mm))
                          .arg(fmtG(lastResult_.fitY.rayleighRange_mm)));
  summaryComb_->setText(
      QStringLiteral("M²_c = %1\nrelRMS_X = %2\nrelRMS_Y = %3\nflags: %4")
          .arg(fmtG(lastResult_.fitCombined.m2))
          .arg(fmtG(lastResult_.fitX.relativeRms, 3))
          .arg(fmtG(lastResult_.fitY.relativeRms, 3))
          .arg(QString::fromStdString(
              beam::core::describeQualityFlags(lastResult_.fitX.qualityFlags |
                                               lastResult_.fitY.qualityFlags))));
}

void M2ScanDialog::updateDiagnostics() {
  if (!diagnosticsEdit_) {
    return;
  }
  if (!lastResult_.valid) {
    diagnosticsEdit_->setPlainText(QStringLiteral("等待有效拟合…"));
    return;
  }
  const auto dx = beam::core::diagnoseM2Axis(lastResult_.fitX, lastResult_.points, false);
  const auto dy = beam::core::diagnoseM2Axis(lastResult_.fitY, lastResult_.points, true);
  QString text;
  text += QStringLiteral("【工程覆盖诊断 — 非 ISO 符合性判定】\n\n");
  text += QStringLiteral("X：采用 %1 · 左/右 %2/%3 · 近/远 %4/%5 · θ≈%6 mrad · BPP≈%7\n")
              .arg(dx.includedCount)
              .arg(dx.leftOfWaist)
              .arg(dx.rightOfWaist)
              .arg(dx.nearWaist)
              .arg(dx.farField)
              .arg(fmtG(dx.divergenceHalf_mrad, 4))
              .arg(fmtG(dx.bpp_mm_mrad, 4));
  text += QStringLiteral("Y：采用 %1 · 左/右 %2/%3 · 近/远 %4/%5 · θ≈%6 mrad · BPP≈%7\n\n")
              .arg(dy.includedCount)
              .arg(dy.leftOfWaist)
              .arg(dy.rightOfWaist)
              .arg(dy.nearWaist)
              .arg(dy.farField)
              .arg(fmtG(dy.divergenceHalf_mrad, 4))
              .arg(fmtG(dy.bpp_mm_mrad, 4));
  text += QStringLiteral("警告：\n");
  if (dx.warnings.empty() && dy.warnings.empty()) {
    text += QStringLiteral("  （无）未触发当前启发式提示。\n");
  } else {
    for (const auto& w : dx.warnings) {
      text += QStringLiteral("  X — %1\n").arg(QString::fromStdString(w));
    }
    for (const auto& w : dy.warnings) {
      text += QStringLiteral("  Y — %1\n").arg(QString::fromStdString(w));
    }
  }
  diagnosticsEdit_->setPlainText(text);
}

std::vector<beam::core::M2ScanPoint> M2ScanDialog::collectPoints() const {
  return points_;
}

void M2ScanDialog::addPoint() {
  beam::core::M2ScanPoint p;
  p.z_mm = zSpin_->value();
  p.diameterX_um = dxSpin_->value();
  p.diameterY_um = dySpin_->value();
  p.sourceLabel = sourceEdit_->text().toStdString();
  p.valid = p.diameterX_um > 0.0 && p.diameterY_um > 0.0;
  points_.push_back(p);
  refreshTable();
  updateDzPlots();
}

void M2ScanDialog::addFromSeed() {
  if (!hasSeed_) {
    QMessageBox::information(this, QStringLiteral("M² 扫描"),
                             QStringLiteral("请先在主窗口加载帧并计算有效束宽。"));
    return;
  }
  dxSpin_->setValue(seedDx_);
  dySpin_->setValue(seedDy_);
  sourceEdit_->setText(seedSource_);
  addPoint();
}

void M2ScanDialog::importCsv() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("导入双轴焦散 CSV"), QString(),
      QStringLiteral("CSV (*.csv);;All (*)"));
  if (path.isEmpty()) {
    return;
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法打开文件。"));
    return;
  }
  const QByteArray bytes = f.readAll();
  const auto parsed = beam::core::parseCausticCsv(std::string(bytes.constData(), bytes.size()));
  if (!parsed.ok) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QString::fromStdString(parsed.message));
    return;
  }
  points_ = parsed.points;
  lastResult_ = {};
  refreshTable();
  updateDzPlots();
  updateResultSummary();
  updateDiagnostics();
  resultLabel_->setText(
      QStringLiteral("已导入 %1 个测点（%2）。请点击「拟合 M²」。\n文件：%3")
          .arg(points_.size())
          .arg(parsed.radiiInput ? QStringLiteral("半径列已×2→直径")
                                 : QStringLiteral("直径列"))
          .arg(path));
  viewTabs_->setCurrentIndex(4);
}

void M2ScanDialog::removeSelected() {
  const auto rows = table_->selectionModel()->selectedRows();
  if (rows.isEmpty()) {
    return;
  }
  std::vector<int> indices;
  indices.reserve(static_cast<std::size_t>(rows.size()));
  for (const auto& idx : rows) {
    indices.push_back(idx.row());
  }
  std::sort(indices.begin(), indices.end(), std::greater<int>());
  for (int r : indices) {
    if (r >= 0 && r < static_cast<int>(points_.size())) {
      points_.erase(points_.begin() + r);
    }
  }
  refreshTable();
  updateDzPlots();
}

void M2ScanDialog::clearPoints() {
  points_.clear();
  lastResult_ = {};
  refreshTable();
  dzPlotOverlay_->clearProfile();
  dzPlotX_->clearProfile();
  dzPlotY_->clearProfile();
  residualPlot_->clearProfile();
  residualPlot_->setZeroLineEnabled(true);
  residualTable_->setRowCount(0);
  updateResultSummary();
  updateDiagnostics();
  resultLabel_->setText(QStringLiteral("已清空。"));
}

void M2ScanDialog::generatePlan() {
  const auto cfg = sessionConfig();
  const auto plan =
      beam::core::planMotorScan(cfg.coarse, cfg.fine, 0.0, cfg.runFineAfterCoarse);
  QString msg = QStringLiteral("粗扫位置 %1 个").arg(plan.coarsePositions_mm.size());
  if (plan.finePlanned) {
    msg += QStringLiteral("；精扫模板 %1 个（腰斑暂按 0 mm，仿真运行时会重定中心）")
               .arg(plan.finePositions_mm.size());
  }
  msg += QStringLiteral("。\n位置：");
  for (std::size_t i = 0; i < plan.allPositions_mm.size() && i < 24; ++i) {
    if (i) {
      msg += QStringLiteral(", ");
    }
    msg += QString::number(plan.allPositions_mm[i], 'g', 4);
  }
  if (plan.allPositions_mm.size() > 24) {
    msg += QStringLiteral(", …");
  }
  resultLabel_->setText(msg);
  viewTabs_->setCurrentIndex(4);
}

void M2ScanDialog::runSimulatedScan() {
  const auto cfg = sessionConfig();
  beam::core::SyntheticBeamModel model;
  model.wavelength_nm = cfg.wavelength_nm;
  model.m2X = synthM2Spin_->value();
  model.m2Y = synthM2Spin_->value();
  model.waistDiameterX_um = synthD0Spin_->value();
  model.waistDiameterY_um = synthD0Spin_->value();
  model.waistZ_mm = 0.0;

  beam::core::SimulatedStage stage(cfg.coarse.zStart_mm);
  auto acquire = beam::core::makeSyntheticAcquire(model);
  const auto session = beam::core::runScanSession(stage, acquire, cfg);
  points_ = session.points;
  lastResult_ = session.full;
  refreshTable();
  updateDzPlots();
  updateResultSummary();
  updateDiagnostics();
  viewTabs_->setCurrentIndex(0);

  if (!session.valid) {
    resultLabel_->setText(QStringLiteral("仿真扫描拟合失败：%1")
                              .arg(QString::fromStdString(session.message)));
    return;
  }
  QString text;
  text += QStringLiteral("仿真扫描完成（台架：%1，测点 %2）\n")
              .arg(QString::fromStdString(stage.name()))
              .arg(points_.size());
  text += QStringLiteral("M²_X = %1 ± %2  flags=%3\n")
              .arg(lastResult_.fitX.m2, 0, 'g', 4)
              .arg(lastResult_.fitX.m2Uncertainty, 0, 'g', 3)
              .arg(QString::fromStdString(
                  beam::core::describeQualityFlags(lastResult_.fitX.qualityFlags)));
  text += QStringLiteral("M²_Y = %1 ± %2\n")
              .arg(lastResult_.fitY.m2, 0, 'g', 4)
              .arg(lastResult_.fitY.m2Uncertainty, 0, 'g', 3);
  text += QStringLiteral("腰斑 z ≈ %1 mm，zR_X ≈ %2 mm")
              .arg(lastResult_.fitX.waistZ_mm, 0, 'g', 4)
              .arg(lastResult_.fitX.rayleighRange_mm, 0, 'g', 4);
  resultLabel_->setText(text);
}

void M2ScanDialog::runFit() {
  syncPointsFromTable();
  lastResult_ = beam::core::analyzeM2Scan(collectPoints(), wavelengthSpin_->value());
  updateDzPlots();
  updateResultSummary();
  updateDiagnostics();
  viewTabs_->setCurrentIndex(0);
  if (!lastResult_.valid) {
    resultLabel_->setText(QStringLiteral("拟合失败：%1")
                              .arg(QString::fromStdString(lastResult_.message)));
    return;
  }
  QString text;
  text += QStringLiteral("M²_X = %1 ± %2  (腰斑 Dx0=%3 µm @ z=%4 mm, zR=%5 mm)\n")
              .arg(lastResult_.fitX.m2, 0, 'g', 4)
              .arg(lastResult_.fitX.m2Uncertainty, 0, 'g', 3)
              .arg(lastResult_.fitX.waistDiameter_um, 0, 'g', 4)
              .arg(lastResult_.fitX.waistZ_mm, 0, 'g', 4)
              .arg(lastResult_.fitX.rayleighRange_mm, 0, 'g', 4);
  text += QStringLiteral("M²_Y = %1 ± %2  flags=%3\n")
              .arg(lastResult_.fitY.m2, 0, 'g', 4)
              .arg(lastResult_.fitY.m2Uncertainty, 0, 'g', 3)
              .arg(QString::fromStdString(
                  beam::core::describeQualityFlags(lastResult_.fitY.qualityFlags)));
  text += QStringLiteral("M²_comb = %1  relRMS=%2  残差点数 X=%3 Y=%4")
              .arg(lastResult_.fitCombined.m2, 0, 'g', 4)
              .arg(lastResult_.fitCombined.relativeRms, 0, 'g', 3)
              .arg(lastResult_.fitX.residuals.size())
              .arg(lastResult_.fitY.residuals.size());
  resultLabel_->setText(text);
}

bool M2ScanDialog::ensureFit() {
  if (!lastResult_.valid) {
    runFit();
  }
  return lastResult_.valid;
}

void M2ScanDialog::exportMarkdown() {
  if (!ensureFit()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 M² 报告"), QStringLiteral("m2_report.md"),
      QStringLiteral("Markdown (*.md);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  if (!beam::io::exportM2ReportMarkdown(path.toStdString(), lastResult_, reportMeta())) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

void M2ScanDialog::exportHtml() {
  if (!ensureFit()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 HTML 报告"), QStringLiteral("m2_report.html"),
      QStringLiteral("HTML (*.html);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  if (!beam::io::exportM2ReportHtml(path.toStdString(), lastResult_, reportMeta())) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出 HTML: %1").arg(path));
}

void M2ScanDialog::exportJson() {
  if (!ensureFit()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 M² JSON"), QStringLiteral("m2_report.json"),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  if (!beam::io::exportM2ReportJson(path.toStdString(), lastResult_, reportMeta())) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

void M2ScanDialog::exportDzCsv() {
  if (!ensureFit()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 D(z) CSV"), QStringLiteral("m2_dz.csv"),
      QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }
  std::ofstream out(path.toStdString());
  if (!out) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  out << beam::core::formatDzCurveCsv(lastResult_);
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

void M2ScanDialog::exportResidualsCsv() {
  if (!ensureFit()) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出残差 CSV"), QStringLiteral("m2_residuals.csv"),
      QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }
  if (!beam::io::exportM2ResidualsCsv(path.toStdString(), lastResult_, reportMeta())) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出残差: %1").arg(path));
  viewTabs_->setCurrentIndex(2);
}

void M2ScanDialog::exportReportBundle() {
  if (!ensureFit()) {
    return;
  }
  const QString prefix = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出报告包（选择前缀）"), QStringLiteral("m2_report"),
      QStringLiteral("Prefix (*)"));
  if (prefix.isEmpty()) {
    return;
  }
  const auto meta = reportMeta();
  const std::string base = prefix.toStdString();
  const bool okMd = beam::io::exportM2ReportMarkdown(base + ".md", lastResult_, meta);
  const bool okHtml = beam::io::exportM2ReportHtml(base + ".html", lastResult_, meta);
  const bool okJson = beam::io::exportM2ReportJson(base + ".json", lastResult_, meta);
  const bool okRes = beam::io::exportM2ResidualsCsv(base + "_residuals.csv", lastResult_, meta);
  std::ofstream dz(base + "_dz.csv");
  const bool okDz = static_cast<bool>(dz);
  if (okDz) {
    dz << beam::core::formatDzCurveCsv(lastResult_);
  }
  if (!(okMd && okHtml && okJson && okRes && okDz)) {
    QMessageBox::warning(this, QStringLiteral("导出部分失败"),
                         QStringLiteral("部分文件未能写入，请检查路径权限。"));
  } else {
    QMessageBox::information(
        this, QStringLiteral("报告包"),
        QStringLiteral("已写出：\n%1.md\n%1.html\n%1.json\n%1_dz.csv\n%1_residuals.csv")
            .arg(prefix));
  }
  resultLabel_->setText(resultLabel_->text() +
                        QStringLiteral("\n已导出报告包前缀: %1").arg(prefix));
}

}  // namespace beam::ui
