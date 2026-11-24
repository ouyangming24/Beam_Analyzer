#include "M2ScanDialog.hpp"

#include "beam/io/ResultExporter.hpp"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace beam::ui {

M2ScanDialog::M2ScanDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("M² 多点扫描"));
  resize(720, 520);

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
  form->addRow(QStringLiteral("z"), zSpin_);

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

  auto* row = new QHBoxLayout;
  auto* addBtn = new QPushButton(QStringLiteral("添加测点"));
  auto* seedBtn = new QPushButton(QStringLiteral("用当前帧束宽"));
  auto* removeBtn = new QPushButton(QStringLiteral("删除选中"));
  auto* clearBtn = new QPushButton(QStringLiteral("清空"));
  row->addWidget(addBtn);
  row->addWidget(seedBtn);
  row->addWidget(removeBtn);
  row->addWidget(clearBtn);
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

  auto* fitRow = new QHBoxLayout;
  auto* fitBtn = new QPushButton(QStringLiteral("拟合 M²"));
  auto* mdBtn = new QPushButton(QStringLiteral("导出报告 Markdown…"));
  auto* jsonBtn = new QPushButton(QStringLiteral("导出报告 JSON…"));
  fitRow->addWidget(fitBtn);
  fitRow->addWidget(mdBtn);
  fitRow->addWidget(jsonBtn);
  fitRow->addStretch();
  root->addLayout(fitRow);

  resultLabel_ = new QLabel(QStringLiteral("添加 ≥3 个轴向测点后拟合。"));
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
  connect(fitBtn, &QPushButton::clicked, this, &M2ScanDialog::runFit);
  connect(mdBtn, &QPushButton::clicked, this, &M2ScanDialog::exportMarkdown);
  connect(jsonBtn, &QPushButton::clicked, this, &M2ScanDialog::exportJson);
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
  resultLabel_->setText(QStringLiteral("已清空。"));
}

void M2ScanDialog::runFit() {
  lastResult_ = beam::core::analyzeM2Scan(collectPoints(), wavelengthSpin_->value());
  if (!lastResult_.valid) {
    resultLabel_->setText(QStringLiteral("拟合失败：%1")
                              .arg(QString::fromStdString(lastResult_.message)));
    return;
  }
  QString text;
  text += QStringLiteral("M²_X = %1  (腰斑 Dx0=%2 µm @ z=%3 mm)\n")
              .arg(lastResult_.fitX.m2, 0, 'g', 4)
              .arg(lastResult_.fitX.waistDiameter_um, 0, 'g', 4)
              .arg(lastResult_.fitX.waistZ_mm, 0, 'g', 4);
  text += QStringLiteral("M²_Y = %1  (腰斑 Dy0=%2 µm @ z=%3 mm)\n")
              .arg(lastResult_.fitY.m2, 0, 'g', 4)
              .arg(lastResult_.fitY.waistDiameter_um, 0, 'g', 4)
              .arg(lastResult_.fitY.waistZ_mm, 0, 'g', 4);
  text += QStringLiteral("M²_comb = %1")
              .arg(lastResult_.fitCombined.m2, 0, 'g', 4);
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
  meta.note = "Beam Analyzer Phase 2 M2 scan";
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
  meta.note = "Beam Analyzer Phase 2 M2 scan";
  if (!beam::io::exportM2ReportJson(path.toStdString(), lastResult_, meta)) {
    QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件。"));
    return;
  }
  resultLabel_->setText(resultLabel_->text() + QStringLiteral("\n已导出: %1").arg(path));
}

}  // namespace beam::ui
