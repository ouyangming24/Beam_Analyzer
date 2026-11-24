#pragma once

#include "beam/core/Aoi.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/Roi.hpp"

#include <QLabel>
#include <QPoint>
#include <optional>

class QMouseEvent;
class QPaintEvent;

namespace beam::ui {

/// Displays a QPixmap with interactive AOI drag, centroid cross, and beam ellipse.
class ImageView final : public QLabel {
  Q_OBJECT
 public:
  enum class InteractionMode {
    None,
    DrawAoi,
    PickCrosshair,
  };

  explicit ImageView(QWidget* parent = nullptr);

  void setDisplayImage(const QImage& image, int frameW, int frameH);
  void clearFrame();

  void setInteractionMode(InteractionMode mode);
  [[nodiscard]] InteractionMode interactionMode() const { return mode_; }

  void setAoiShape(beam::core::AoiShape shape);
  [[nodiscard]] beam::core::AoiShape aoiShape() const { return aoiShape_; }

  void setAoi(const std::optional<beam::core::AoiRegion>& aoi);
  [[nodiscard]] std::optional<beam::core::AoiRegion> aoi() const { return aoi_; }

  /// Legacy rect ROI accessors (maps to rectangular AOI).
  void setRoi(const std::optional<beam::core::RectRoi>& roi);
  [[nodiscard]] std::optional<beam::core::RectRoi> roi() const;

  void setCrosshair(int x, int y);
  [[nodiscard]] QPoint crosshair() const { return crosshair_; }
  [[nodiscard]] bool hasCrosshair() const { return hasCrosshair_; }

  void setShowCrosshair(bool on);
  void setProfileLine(int x0, int y0, int x1, int y1, bool enabled);

  /// Overlay second-moment centroid cross + principal-axis beam ellipse.
  /// Metrics are in µm; pass detector pixel pitches for conversion to pixels.
  void setBeamOverlay(const std::optional<beam::core::Iso11146Metrics>& metrics,
                      bool enabled,
                      double pixelSizeX_um = 1.0,
                      double pixelSizeY_um = 1.0);

 signals:
  void aoiChanged(beam::core::AoiRegion aoi);
  void roiChanged(beam::core::RectRoi roi);  ///< Emitted for rect AOI (compat)
  void crosshairChanged(int x, int y);
  void profileLineChanged(int x0, int y0, int x1, int y1);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  enum class DragMode { None, Create, Move, Crosshair };

  [[nodiscard]] QPoint frameFromWidget(const QPoint& widgetPos) const;
  [[nodiscard]] QRect widgetRectFromRoi(const beam::core::RectRoi& roi) const;
  [[nodiscard]] QPoint widgetFromFrame(int fx, int fy) const;
  [[nodiscard]] QPointF widgetFromFrameF(double fx, double fy) const;
  void paintAoi(QPainter& p) const;
  void paintBeamOverlay(QPainter& p) const;
  void paintOverlayLegend(QPainter& p) const;
  [[nodiscard]] bool aoiContainsWidget(const QPoint& widgetPos) const;

  QImage image_;
  int frameW_ = 0;
  int frameH_ = 0;
  InteractionMode mode_ = InteractionMode::None;
  beam::core::AoiShape aoiShape_ = beam::core::AoiShape::Rect;
  std::optional<beam::core::AoiRegion> aoi_;
  DragMode dragMode_ = DragMode::None;
  QPoint dragStartFrame_;
  beam::core::RectRoi dragStartBounds_{};
  QPoint crosshair_{0, 0};
  bool hasCrosshair_ = false;
  bool showCrosshair_ = true;
  bool showProfileLine_ = false;
  int lineX0_ = 0;
  int lineY0_ = 0;
  int lineX1_ = 0;
  int lineY1_ = 0;
  std::optional<beam::core::Iso11146Metrics> beamMetrics_;
  bool showBeamOverlay_ = false;
  double overlayPitchX_um_ = 1.0;
  double overlayPitchY_um_ = 1.0;
};

}  // namespace beam::ui
