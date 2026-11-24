#pragma once

#include "beam/camera/ICamera.hpp"
#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Profile.hpp"
#include "beam/core/Roi.hpp"

#include "FalseColor.hpp"
#include "ImageView.hpp"

#include <QMainWindow>
#include <QImage>

#include <memory>
#include <optional>

class QLabel;
class QComboBox;
class QTextEdit;
class QAction;
class QDoubleSpinBox;
class QCheckBox;

namespace beam::ui {

class ProfilePlot;

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void openCsv();
  void loadSimulatedFrame();
  void exportJson();
  void exportCsv();
  void exportPower();
  void exportHProfile();
  void exportVProfile();
  void onColorMapChanged(int index);
  void onRoiModeToggled(bool on);
  void onClearRoi();
  void onRoiChanged(beam::core::RectRoi roi);
  void onCrosshairChanged(int x, int y);
  void onBackgroundModeChanged(int index);
  void onCalibrationChanged();
  void onRecompute();
  void openM2Scan();
  void saveSettings();
  void loadSettings();

 private:
  void rebuildUi();
  void applyFrame(beam::core::BeamFrame frame, const QString& sourceLabel);
  void refreshAnalysis();
  void refreshImage();
  void refreshProfiles();
  void refreshMetricsPanel();
  bool ensureMetrics() const;
  [[nodiscard]] beam::core::AnalysisPrep currentPrep() const;
  [[nodiscard]] beam::core::PowerCalibration currentCalibration() const;

  ImageView* imageView_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QComboBox* colorMapBox_ = nullptr;
  QComboBox* backgroundBox_ = nullptr;
  QDoubleSpinBox* bgConstantSpin_ = nullptr;
  QDoubleSpinBox* countsToWattsSpin_ = nullptr;
  QDoubleSpinBox* exposureSpin_ = nullptr;
  QDoubleSpinBox* wavelengthSpin_ = nullptr;
  QCheckBox* useRoiCheck_ = nullptr;
  QTextEdit* metricsView_ = nullptr;
  ProfilePlot* hProfilePlot_ = nullptr;
  ProfilePlot* vProfilePlot_ = nullptr;
  QAction* exportJsonAction_ = nullptr;
  QAction* exportCsvAction_ = nullptr;
  QAction* roiModeAction_ = nullptr;

  std::optional<beam::core::BeamFrame> rawFrame_;
  std::optional<beam::core::BeamFrame> analysisFrame_;
  std::optional<beam::core::Iso11146Metrics> metrics_;
  std::optional<beam::core::PowerMetrics> power_;
  std::optional<beam::core::RectRoi> roi_;
  beam::core::LineProfile hProfile_;
  beam::core::LineProfile vProfile_;
  int crossX_ = -1;
  int crossY_ = -1;
  QString sourcePath_;
  ColorMap colorMap_ = ColorMap::FalseColor;
  std::unique_ptr<beam::camera::ICamera> simCamera_;
};

}  // namespace beam::ui
