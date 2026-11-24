#pragma once

#include "beam/core/BeamFrame.hpp"
#include "beam/core/Aoi.hpp"
#include "beam/core/Iso11146.hpp"

#include <QWidget>

#include <optional>
#include <vector>

namespace beam::ui {

/// Software-rendered rotatable / zoomable intensity surface (Qt Widgets / QPainter).
/// Large frames are downsampled for display; metrics stay on the full analysis path.
class IntensitySurface3D final : public QWidget {
  Q_OBJECT
 public:
  explicit IntensitySurface3D(QWidget* parent = nullptr);

  void setFrame(const beam::core::BeamFrame& frame);
  void clearFrame();
  void setAoi(const std::optional<beam::core::AoiRegion>& aoi);
  void setBeamOverlay(const std::optional<beam::core::Iso11146Metrics>& metrics);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  struct SampleGrid {
    int w = 0;
    int h = 0;
    double zMin = 0.0;
    double zMax = 1.0;
    std::vector<double> z;  ///< row-major, size w*h
  };

  void rebuildGrid(const beam::core::BeamFrame& frame);
  [[nodiscard]] QPointF project(double x, double y, double z, const QRect& viewport) const;
  [[nodiscard]] QColor colorForZ(double t) const;

  SampleGrid grid_;
  std::optional<beam::core::AoiRegion> aoi_;
  std::optional<beam::core::Iso11146Metrics> metrics_;
  double yaw_deg_ = 42.0;
  double pitch_deg_ = 28.0;
  double zoom_ = 1.0;
  bool dragging_ = false;
  QPoint lastPos_;
  static constexpr int kMaxDim = 72;
};

}  // namespace beam::ui
