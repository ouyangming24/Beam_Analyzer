#include "CalibrationWizard.hpp"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace beam::ui {

CalibrationWizard::CalibrationWizard(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("像素尺寸与功率标定向导"));
  resize(440, 420);
  auto* root = new QVBoxLayout(this);

  auto* notice = new QLabel(
      QStringLiteral("概念实现：填写工程标定参数。不构成计量溯源证书；"
                     "真实功率计联调仍属后续硬件阶段。"));
  notice->setWordWrap(true);
  notice->setStyleSheet(QStringLiteral("color:#fbbf24;"));
  root->addWidget(notice);

  auto* form = new QFormLayout;
  pxSpin_ = new QDoubleSpinBox;
  pxSpin_->setRange(1e-3, 1e4);
  pxSpin_->setDecimals(4);
  pxSpin_->setValue(5.0);
  pxSpin_->setSuffix(QStringLiteral(" µm"));
  pySpin_ = new QDoubleSpinBox;
  pySpin_->setRange(1e-3, 1e4);
  pySpin_->setDecimals(4);
  pySpin_->setValue(5.0);
  pySpin_->setSuffix(QStringLiteral(" µm"));
  form->addRow(QStringLiteral("像素尺寸 X"), pxSpin_);
  form->addRow(QStringLiteral("像素尺寸 Y"), pySpin_);

  countsSpin_ = new QDoubleSpinBox;
  countsSpin_->setRange(0.0, 1e3);
  countsSpin_->setDecimals(12);
  countsSpin_->setSingleStep(1e-6);
  countsSpin_->setSpecialValueText(QStringLiteral("未标定 (0)"));
  form->addRow(QStringLiteral("counts → W"), countsSpin_);

  exposureSpin_ = new QDoubleSpinBox;
  exposureSpin_->setRange(1e-9, 1e3);
  exposureSpin_->setDecimals(6);
  exposureSpin_->setValue(1.0);
  exposureSpin_->setSuffix(QStringLiteral(" s"));
  form->addRow(QStringLiteral("曝光 / 积分时间"), exposureSpin_);

  wavelengthSpin_ = new QDoubleSpinBox;
  wavelengthSpin_->setRange(1.0, 1e6);
  wavelengthSpin_->setDecimals(3);
  wavelengthSpin_->setValue(1064.0);
  wavelengthSpin_->setSuffix(QStringLiteral(" nm"));
  form->addRow(QStringLiteral("波长"), wavelengthSpin_);

  sampleEdit_ = new QLineEdit;
  operatorEdit_ = new QLineEdit;
  deviceEdit_ = new QLineEdit;
  calIdEdit_ = new QLineEdit;
  form->addRow(QStringLiteral("样品名"), sampleEdit_);
  form->addRow(QStringLiteral("操作员"), operatorEdit_);
  form->addRow(QStringLiteral("设备 ID"), deviceEdit_);
  form->addRow(QStringLiteral("标定记录 ID"), calIdEdit_);
  root->addLayout(form);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);
}

void CalibrationWizard::setInitial(const CalibrationResult& seed) {
  pxSpin_->setValue(seed.pixelSizeX_um);
  pySpin_->setValue(seed.pixelSizeY_um);
  countsSpin_->setValue(seed.countsToWatts);
  exposureSpin_->setValue(seed.exposure_s);
  wavelengthSpin_->setValue(seed.wavelength_nm);
  sampleEdit_->setText(seed.sampleName);
  operatorEdit_->setText(seed.operatorName);
  deviceEdit_->setText(seed.deviceId);
  calIdEdit_->setText(seed.calibrationId);
}

CalibrationResult CalibrationWizard::result() const {
  CalibrationResult r;
  r.accepted = true;
  r.pixelSizeX_um = pxSpin_->value();
  r.pixelSizeY_um = pySpin_->value();
  r.countsToWatts = countsSpin_->value();
  r.exposure_s = exposureSpin_->value();
  r.wavelength_nm = wavelengthSpin_->value();
  r.sampleName = sampleEdit_->text();
  r.operatorName = operatorEdit_->text();
  r.deviceId = deviceEdit_->text();
  r.calibrationId = calIdEdit_->text();
  return r;
}

}  // namespace beam::ui
