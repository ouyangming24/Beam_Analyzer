#pragma once

#include "beam/camera/CameraRegistry.hpp"
#include "beam/camera/ICamera.hpp"
#include "beam/core/Aoi.hpp"
#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/Iso13694.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Profile.hpp"
#include "beam/core/Roi.hpp"

#include "FalseColor.hpp"
#include "ImageView.hpp"
#include "Theme.hpp"

#include <QMainWindow>
#include <QImage>

#include <deque>
#include <memory>
#include <optional>
#include <vector>

class QLabel;
class QComboBox;
class QAction;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QTimer;
class QSlider;
class QDockWidget;
class QMenu;
class QDragEnterEvent;
class QDropEvent;

namespace beam::ui {

class ProfilePlot;
class MetricsPanel;
class IntensitySurface3D;
class AnalysisDialog;

 class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  /// Load a simulated frame, open analysis, and write PNG shots into outDir.
  /// Used for demo / CI visual capture (`--capture-shots <dir>`).
  void captureDemoShots(const QString& outDir);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private slots:
  void openFile();
  void openCsv();
  void openImage();
  void openRecentFile();
  void clearRecentFiles();
  void loadSimulatedFrame();
  void exportJson();
  void exportCsv();
  void exportPower();
  void exportHProfile();
  void exportVProfile();
  void exportAnnotatedImage();
  void onColorMapChanged(int index);
  void onRoiModeToggled(bool on);
  void onClearRoi();
  void onRoiChanged(beam::core::RectRoi roi);
  void onAoiChanged(beam::core::AoiRegion aoi);
  void onAoiShapeChanged(int index);
  void onAutoAoi();
  void onCrosshairChanged(int x, int y);
  void onBackgroundModeChanged(int index);
  void onCalibrationChanged();
  void onRecompute();
  void openM2Scan();
  void openAnalysis();
  void saveSettings();
  void loadSettings();
  void toggleTheme();
  void setTheme(AppTheme theme);

  void refreshCameraList();
  void onCameraConnect();
  void onCameraDisconnect();
  void onLivePreviewToggled(bool on);
  void onCameraControlsChanged();
  void onSoftwareTrigger();
  void onLiveTick();
  void onFrameScrub(int index);
  void onPlayToggle();
  void onPlayTick();
  void onFrameFirst();
  void onFramePrev();
  void onFrameNext();
  void onFrameLast();
  void onIntensityScaleChanged(int index);

 private:
  void rebuildUi();
  void rebuildRecentMenu();
  void addRecentFile(const QString& path);
  bool loadPath(const QString& path);
  bool loadCsvPath(const QString& path);
  bool loadImagePath(const QString& path);
  void applyFrame(beam::core::BeamFrame frame, const QString& sourceLabel,
                  bool pushHistory = true);
  void refreshAnalysis();
  void refreshImage();
  void refreshProfiles();
  void refreshMetricsPanel();
  bool ensureMetrics() const;
  [[nodiscard]] beam::core::AnalysisPrep currentPrep() const;
  [[nodiscard]] beam::core::PowerCalibration currentCalibration() const;
  [[nodiscard]] beam::camera::CameraControls readCameraControls() const;
  void applyCameraControlsToUi(const beam::camera::CameraControls& c);
  void setCameraUiConnected(bool connected);
  void updateStatusBarDetails();
  void updateMeasurementChrome();
  void updateStatusStrip();
  void seekHistory(int index);
  void stopHistoryPlayback();

  ImageView* imageView_ = nullptr;
  IntensitySurface3D* surface3d_ = nullptr;
  MetricsPanel* metricsPanel_ = nullptr;
  QDockWidget* metricsDock_ = nullptr;
  AnalysisDialog* analysisDialog_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* statusMetricsBrief_ = nullptr;
  QLabel* statusModeChip_ = nullptr;
  QLabel* statusLiveChip_ = nullptr;
  QLabel* statusStripMode_ = nullptr;
  QLabel* statusStripSource_ = nullptr;
  QLabel* statusStripValidity_ = nullptr;
  QLabel* statusStripHint_ = nullptr;
  QLabel* imageChromeLabel_ = nullptr;
  QLabel* colorLegendLabel_ = nullptr;
  QComboBox* colorMapBox_ = nullptr;
  QComboBox* intensityScaleBox_ = nullptr;
  QComboBox* aoiShapeBox_ = nullptr;
  QPushButton* playBtn_ = nullptr;
  QTimer* playTimer_ = nullptr;
  QComboBox* backgroundBox_ = nullptr;
  QDoubleSpinBox* bgConstantSpin_ = nullptr;
  QDoubleSpinBox* countsToWattsSpin_ = nullptr;
  QDoubleSpinBox* exposureSpin_ = nullptr;
  QDoubleSpinBox* wavelengthSpin_ = nullptr;
  QCheckBox* useRoiCheck_ = nullptr;
  ProfilePlot* hProfilePlot_ = nullptr;
  ProfilePlot* vProfilePlot_ = nullptr;
  QAction* exportJsonAction_ = nullptr;
  QAction* exportCsvAction_ = nullptr;
  QAction* exportImageAction_ = nullptr;
  QAction* roiModeAction_ = nullptr;
  QAction* themeAction_ = nullptr;
  QMenu* recentMenu_ = nullptr;
  QSlider* frameScrubber_ = nullptr;
  QLabel* frameScrubLabel_ = nullptr;

  QComboBox* cameraSourceBox_ = nullptr;
  QPushButton* cameraConnectBtn_ = nullptr;
  QPushButton* cameraDisconnectBtn_ = nullptr;
  QCheckBox* livePreviewCheck_ = nullptr;
  QDoubleSpinBox* camExposureSpin_ = nullptr;
  QDoubleSpinBox* camGainSpin_ = nullptr;
  QDoubleSpinBox* camFpsSpin_ = nullptr;
  QComboBox* triggerModeBox_ = nullptr;
  QPushButton* softwareTriggerBtn_ = nullptr;
  QTimer* liveTimer_ = nullptr;

  std::optional<beam::core::BeamFrame> rawFrame_;
  std::optional<beam::core::BeamFrame> analysisFrame_;
  std::optional<beam::core::Iso11146Metrics> metrics_;
  std::optional<beam::core::Iso13694Metrics> iso13694_;
  std::optional<beam::core::PowerMetrics> power_;
  std::optional<beam::core::AoiRegion> aoi_;
  beam::core::AoiShape aoiShape_ = beam::core::AoiShape::Rect;
  beam::core::LineProfile hProfile_;
  beam::core::LineProfile vProfile_;
  int crossX_ = -1;
  int crossY_ = -1;
  QString sourcePath_;
  ColorMap colorMap_ = ColorMap::FalseColor;
  IntensityScale intensityScale_ = IntensityScale::Linear;
  AppTheme theme_ = AppTheme::Dark;
  std::unique_ptr<beam::camera::ICamera> activeCamera_;
  std::vector<beam::camera::CameraInfo> discoveredCameras_;
  QStringList recentFiles_;
  std::deque<beam::core::BeamFrame> frameHistory_;
  std::deque<QString> frameHistoryLabels_;
  static constexpr int kMaxHistory = 120;
  bool scrubbing_ = false;
  bool liveActive_ = false;
  bool historyPlaying_ = false;
  double stabilityT_ = 0.0;
};

}  // namespace beam::ui
