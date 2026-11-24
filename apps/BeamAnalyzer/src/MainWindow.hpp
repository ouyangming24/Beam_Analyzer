#pragma once

#include "beam/camera/ICamera.hpp"
#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"

#include "FalseColor.hpp"

#include <QMainWindow>
#include <QImage>

#include <memory>
#include <optional>

class QLabel;
class QComboBox;
class QPushButton;
class QTextEdit;
class QAction;

namespace beam::ui {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void openCsv();
  void loadSimulatedFrame();
  void exportJson();
  void exportCsv();
  void onColorMapChanged(int index);

 private:
  void rebuildUi();
  void applyFrame(beam::core::BeamFrame frame, const QString& sourceLabel);
  void refreshImage();
  void refreshMetricsPanel();
  bool ensureMetrics() const;

  QLabel* imageLabel_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QComboBox* colorMapBox_ = nullptr;
  QTextEdit* metricsView_ = nullptr;
  QAction* exportJsonAction_ = nullptr;
  QAction* exportCsvAction_ = nullptr;

  std::optional<beam::core::BeamFrame> frame_;
  std::optional<beam::core::Iso11146Metrics> metrics_;
  QString sourcePath_;
  ColorMap colorMap_ = ColorMap::FalseColor;
  std::unique_ptr<beam::camera::ICamera> simCamera_;
};

}  // namespace beam::ui
