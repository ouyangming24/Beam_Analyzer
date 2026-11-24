#pragma once

#include "beam/camera/ICamera.hpp"
#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Roi.hpp"

#include <QWidget>

#include <optional>

class QLabel;
class QFormLayout;

namespace beam::ui {

/// Dense, live-updating metrics dock content (ISO 11146 + power + context).
class MetricsPanel final : public QWidget {
  Q_OBJECT
 public:
  explicit MetricsPanel(QWidget* parent = nullptr);

  void clear();
  void setSource(const QString& source);
  void setLive(bool live, double fpsHint = 0.0);
  void updateMetrics(const beam::core::Iso11146Metrics& metrics,
                     const std::optional<beam::core::PowerMetrics>& power,
                     const beam::core::BeamFrame* frame,
                     const std::optional<beam::core::RectRoi>& roi,
                     bool roiEnabled,
                     int crossX,
                     int crossY,
                     const beam::camera::CameraControls* camControls);

 private:
  QLabel* addRow(QFormLayout* form, const QString& name);
  void setValue(QLabel* label, const QString& text, const QString& unit = {});

  QLabel* sourceLabel_ = nullptr;
  QLabel* liveLabel_ = nullptr;
  QLabel* totalPower_ = nullptr;
  QLabel* centroidX_ = nullptr;
  QLabel* centroidY_ = nullptr;
  QLabel* dx_ = nullptr;
  QLabel* dy_ = nullptr;
  QLabel* dMajor_ = nullptr;
  QLabel* dMinor_ = nullptr;
  QLabel* orientation_ = nullptr;
  QLabel* ellipticity_ = nullptr;
  QLabel* sumCounts_ = nullptr;
  QLabel* peakCounts_ = nullptr;
  QLabel* meanCounts_ = nullptr;
  QLabel* powerW_ = nullptr;
  QLabel* energyJ_ = nullptr;
  QLabel* roiLabel_ = nullptr;
  QLabel* crossLabel_ = nullptr;
  QLabel* pixelSize_ = nullptr;
  QLabel* camLabel_ = nullptr;
};

}  // namespace beam::ui
