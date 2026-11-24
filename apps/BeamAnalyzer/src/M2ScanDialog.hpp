#pragma once

#include "beam/core/M2Scan.hpp"
#include "beam/core/MotorScan.hpp"

#include <QDialog>
#include <vector>

class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QLineEdit;
class QCheckBox;
class QTabWidget;

namespace beam::ui {

class ProfilePlot;

/// Multi-point M² workstation: dual-axis caustic, residuals, coarse→fine, report export.
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
  void generatePlan();
  void runSimulatedScan();
  void runFit();
  void exportMarkdown();
  void exportJson();
  void exportDzCsv();
  void exportResidualsCsv();
  void exportReportBundle();

 private:
  void refreshTable();
  void updateDzPlots();
  void refreshResidualTable();
  void updateResultSummary();
  [[nodiscard]] bool ensureFit();
  [[nodiscard]] std::vector<beam::core::M2ScanPoint> collectPoints() const;
  [[nodiscard]] beam::core::ScanSessionConfig sessionConfig() const;

  QDoubleSpinBox* wavelengthSpin_ = nullptr;
  QDoubleSpinBox* zSpin_ = nullptr;
  QDoubleSpinBox* dxSpin_ = nullptr;
  QDoubleSpinBox* dySpin_ = nullptr;
  QLineEdit* sourceEdit_ = nullptr;

  QDoubleSpinBox* coarseStartSpin_ = nullptr;
  QDoubleSpinBox* coarseEndSpin_ = nullptr;
  QDoubleSpinBox* coarseStepSpin_ = nullptr;
  QDoubleSpinBox* fineHalfSpin_ = nullptr;
  QDoubleSpinBox* fineStepSpin_ = nullptr;
  QCheckBox* runFineCheck_ = nullptr;
  QDoubleSpinBox* synthM2Spin_ = nullptr;
  QDoubleSpinBox* synthD0Spin_ = nullptr;

  QTableWidget* table_ = nullptr;
  QTableWidget* residualTable_ = nullptr;
  QLabel* resultLabel_ = nullptr;
  QLabel* summaryMx_ = nullptr;
  QLabel* summaryMy_ = nullptr;
  QLabel* summaryComb_ = nullptr;
  QLabel* workflowLabel_ = nullptr;
  ProfilePlot* dzPlotX_ = nullptr;
  ProfilePlot* dzPlotY_ = nullptr;
  ProfilePlot* residualPlot_ = nullptr;
  QTabWidget* viewTabs_ = nullptr;

  std::vector<beam::core::M2ScanPoint> points_;
  beam::core::M2ScanResult lastResult_;
  double seedDx_ = 0.0;
  double seedDy_ = 0.0;
  QString seedSource_;
  bool hasSeed_ = false;
};

}  // namespace beam::ui
