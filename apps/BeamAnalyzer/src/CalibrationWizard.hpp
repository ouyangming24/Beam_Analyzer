#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLineEdit;
class QLabel;

namespace beam::ui {

struct CalibrationResult {
  double pixelSizeX_um = 5.0;
  double pixelSizeY_um = 5.0;
  double countsToWatts = 0.0;
  double exposure_s = 1.0;
  double wavelength_nm = 1064.0;
  QString sampleName;
  QString operatorName;
  QString deviceId;
  QString calibrationId;
  bool accepted = false;
};

/// Guided pixel-size + power calibration (works with simulated cameras).
class CalibrationWizard final : public QDialog {
  Q_OBJECT
 public:
  explicit CalibrationWizard(QWidget* parent = nullptr);

  void setInitial(const CalibrationResult& seed);
  [[nodiscard]] CalibrationResult result() const;

 private:
  QDoubleSpinBox* pxSpin_ = nullptr;
  QDoubleSpinBox* pySpin_ = nullptr;
  QDoubleSpinBox* countsSpin_ = nullptr;
  QDoubleSpinBox* exposureSpin_ = nullptr;
  QDoubleSpinBox* wavelengthSpin_ = nullptr;
  QLineEdit* sampleEdit_ = nullptr;
  QLineEdit* operatorEdit_ = nullptr;
  QLineEdit* deviceEdit_ = nullptr;
  QLineEdit* calIdEdit_ = nullptr;
};

}  // namespace beam::ui
