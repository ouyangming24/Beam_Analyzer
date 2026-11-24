#pragma once

#include "beam/core/M2Scan.hpp"

#include <QDialog>
#include <vector>

class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QLineEdit;

namespace beam::ui {

/// Multi-point M² scan: add stations (z + Dx/Dy), fit, export report.
class M2ScanDialog final : public QDialog {
  Q_OBJECT
 public:
  explicit M2ScanDialog(QWidget* parent = nullptr);

  void setDefaultWavelength(double nm);
  void seedFromCurrentFrame(double diameterX_um,
                            double diameterY_um,
                            const QString& sourceLabel);

  [[nodiscard]] beam::core::M2ScanResult lastResult() const { return lastResult_; }

 private slots:
  void addPoint();
  void addFromSeed();
  void removeSelected();
  void clearPoints();
  void runFit();
  void exportMarkdown();
  void exportJson();

 private:
  void refreshTable();
  [[nodiscard]] std::vector<beam::core::M2ScanPoint> collectPoints() const;

  QDoubleSpinBox* wavelengthSpin_ = nullptr;
  QDoubleSpinBox* zSpin_ = nullptr;
  QDoubleSpinBox* dxSpin_ = nullptr;
  QDoubleSpinBox* dySpin_ = nullptr;
  QLineEdit* sourceEdit_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* resultLabel_ = nullptr;

  std::vector<beam::core::M2ScanPoint> points_;
  beam::core::M2ScanResult lastResult_;
  double seedDx_ = 0.0;
  double seedDy_ = 0.0;
  QString seedSource_;
  bool hasSeed_ = false;
};

}  // namespace beam::ui
