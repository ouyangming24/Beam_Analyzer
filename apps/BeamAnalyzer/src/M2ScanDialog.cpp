#include "M2ScanDialog.hpp"

#include "ProfilePlot.hpp"
#include "beam/core/M2Fit.hpp"
#include "beam/io/ResultExporter.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>

namespace beam::ui {

M2ScanDialog::M2ScanDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("M² 扫描 / 电机粗扫→精扫"));
  resize(860, 720);

  auto* root = new QVBoxLayout(this);

  auto* form = new QFormLayout;
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
  sourceEdit_->setPlaceholderText(QStringLiteral("来源标签（可选）"));
  form->addRow(QStringLiteral("来源"), sourceEdit_);
  root->addLayout(form);

  auto* motorBox = new QGroupBox(QStringLiteral("电机扫描规划（粗扫 → 精扫）"));
  auto* motorForm = new QFormLayout(motorBox);
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
  root->addWidget(motorBox);

  auto* row = new QHBoxLayout;
  auto* addBtn = new QPushButton(QStringLiteral("添加测点"));
  auto* seedBtn = new QPushButton(QStringLiteral("用当前帧束宽"));
  auto* removeBtn = new QPushButton(QStringLiteral("删除选中"));
  auto* clearBtn = new QPushButton(QStringLiteral("清空"));
  auto* planBtn = new QPushButton(QStringLiteral("生成位置列表"));
  auto* simBtn = new QPushButton(QStringLiteral("运行仿真扫描"));
  row->addWidget(addBtn);
  row->addWidget(seedBtn);
  row->addWidget(removeBtn);
  row->addWidget(clearBtn);
  row->addWidget(planBtn);
  row->addWidget(simBtn);
  row->addStretch();
  root->addLayout(row);

  table_ = new QTableWidget(0, 5);
  table_->setHorizontalHeaderLabels(
      {QStringLiteral("z (mm)"), QStringLiteral("Dx (µm)"), QStringLiteral("Dy (µm)"),
       QStringLiteral("有效"), QStringLiteral("来源")});
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  root->addWidget(table_, 1);

  dzPlot_ = new ProfilePlot;
  dzPlot_->setMinimumHeight(160);
  root->addWidget(dzPlot_);

  auto* fitRow = new QHBoxLayout;
  auto* fitBtn = new QPushButton(QStringLiteral("拟合 M²"));
  auto* mdBtn = new QPushButton(QStringLiteral("导出报告 Markdown…"));
  auto* jsonBtn = new QPushButton(QStringLiteral("导出报告 JSON…"));
  auto* csvBtn = new QPushButton(QStringLiteral("导出 D(z) CSV…"));
  fitRow->addWidget(fitBtn);
  fitRow->addWidget(mdBtn);
  fitRow->addWidget(jsonBtn);
  fitRow->addWidget(csvBtn);
  fitRow->addStretch();
  root->addLayout(fitRow);

  resultLabel_ = new QLabel(
      QStringLiteral("添加 ≥3 个轴向测点，或配置粗/精扫后运行仿真扫描。详见 docs/ALGORITHM.md。"));
  resultLabel_->setWordWrap(true);
  resultLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(resultLabel_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  root->addWidget(buttons);

  connect(addBtn, &QPushButton::clicked, this, &M2ScanDialog::addPoint);
  connect(seedBtn, &QPushButton::clicked, this, &M2ScanDialog::addFromSeed);
  connect(removeBtn, &QPushButton::clicked, this, &M2ScanDialog::removeSelected);
  connect(clearBtn, &QPushButton::clicked, this, &M2ScanDialog::clearPoints);
  connect(planBtn, &QPushButton::clicked, this, &M2ScanDialog::generatePlan);
  connect(simBtn, &QPushButton::clicked, this, &M2ScanDialog::runSimulatedScan);
  connect(fitBtn, &QPushButton::clicked, this, &M2ScanDialog::runFit);
  connect(mdBtn, &QPushButton::clicked, this, &M2ScanDialog::exportMarkdown);
  connect(jsonBtn, &QPushButton::clicked, this, &M2ScanDialog::exportJson);
  connect(csvBtn, &QPushButton::clicked, this, &M2ScanDialog::exportDzCsv);
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

void M2ScanDialog::refreshTable() {
  table_->setRowCount(static_cast<int>(points_.size()));
  for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
    const auto& p = points_[static_cast<std::size_t>(i)];
    table_->setItem(i, 0, new QTableWidgetItem(QString::number(p.z_mm, 'g', 8)));
    table_->setItem(i, 1, new QTableWidgetItem(QString::number(p.diameterX_um, 'g', 8)));
    table_->setItem(i, 2, new QTableWidgetItem(QString::number(p.diameterY_um, 'g', 8)));
    table_->setItem(i, 3, new QTableWidgetItem(p.valid ? QStringLiteral("是") : QStringLiteral("否")));
    table_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(p.sourceLabel)));
  }
}

void M2ScanDialog::updateDzPlot() {
  std::vector<double> z;
  std::vector<double> d;
  for (const auto& p : lastResult_.points.empty() ? points_ : lastResult_.points) {
    if (!p.valid) {
      continue;
    }
    z.push_back(p.z_mm);
    d.push_back(std::sqrt(0.5 * (p.diameterX_um * p.diameterX_um + p.diameterY_um * p.diameterY_um)));
  }
  dzPlot_->setSeries(z, d, QStringLiteral("D(z) 合成直径"), QStringLiteral("mm"),
                     QStringLiteral("µm"));
  if (lastResult_.fitCombined.valid && z.size() >= 2) {
    const double z0 = *std::min_element(z.begin(), z.end());
    const double z1 = *std::max_element(z.begin(), z.end());
    std::vector<double> fz;
    std::vector<double> fd;
    constexpr int n = 80;
    for (int i = 0; i < n; ++i) {
      const double zz = z0 + (z1 - z0) * static_cast<double>(i) / (n - 1);
      fz.push_back(zz);
      fd.push_back(beam::core::hyperbolicDiameter_um(
          zz, lastResult_.fitCombined.waistDiameter_um, lastResult_.fitCombined.waistZ_mm,
          lastResult_.fitCombined.m2, lastResult_.wavelength_nm));
    }
    dzPlot_->setOverlaySeries(fz, fd);
  }
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
}

void M2ScanDialog::clearPoints() {
  points_.clear();
  lastResult_ = {};
  refreshTable();
  dzPlot_->clearProfile();
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
  updateDzPlot();

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
  lastResult_ = beam::core::analyzeM2Scan(collectPoints(), wavelengthSpin_->value());
  updateDzPlot();
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
  text += QStringLiteral("M²_comb = %1  relRMS=%2")
              .arg(lastResult_.fitCombined.m2, 0, 'g', 4)
              .arg(lastResult_.fitCombined.relativeRms, 0, 'g', 3);
  resultLabel_->setText(text);
}

void M2ScanDialog::exportMarkdown() {
  if (!lastResult_.valid) {
    runFit();
  }
  if (!lastResult_.valid) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 M² 报告"), QStringLiteral("m2_report.md"),
      QStringLiteral("Markdown (*.md);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta;
  meta.note = "Beam Analyzer M2 scan";
  if (!beam::io::exportM2ReportMarkdown(path.toStdString(), lastResult_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

void M2ScanDialog::exportJson() {
  if (!lastResult_.valid) {
    runFit();
  }
  if (!lastResult_.valid) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 M² JSON"), QStringLiteral("m2_report.json"),
      QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  beam::io::ExportMeta meta;
  meta.note = "Beam Analyzer M2 scan";
  if (!beam::io::exportM2ReportJson(path.toStdString(), lastResult_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

void M2ScanDialog::exportDzCsv() {
  if (!lastResult_.valid) {
    runFit();
  }
  if (!lastResult_.valid) {
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

}  // namespace beam::ui
