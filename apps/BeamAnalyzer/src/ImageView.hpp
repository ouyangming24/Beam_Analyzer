#pragma once

#include "beam/core/Roi.hpp"

#include <QLabel>
#include <QPoint>
#include <optional>

class QMouseEvent;
class QPaintEvent;

namespace beam::ui {

/// Displays a QPixmap with interactive ROI drag and crosshair / profile markers.
class ImageView final : public QLabel {
  Q_OBJECT
 public:
  enum class InteractionMode {
    None,
    DrawRoi,
    PickCrosshair,
  };

  explicit ImageView(QWidget* parent = nullptr);

  void setDisplayImage(const QImage& image, int frameW, int frameH);
  void clearFrame();

  void setInteractionMode(InteractionMode mode);
  [[nodiscard]] InteractionMode interactionMode() const { return mode_; }

  void setRoi(const std::optional<beam::core::RectRoi>& roi);
  [[nodiscard]] std::optional<beam::core::RectRoi> roi() const { return roi_; }

  void setCrosshair(int x, int y);
  [[nodiscard]] QPoint crosshair() const { return crosshair_; }
  [[nodiscard]] bool hasCrosshair() const { return hasCrosshair_; }

  void setShowCrosshair(bool on);
  void setProfileLine(int x0, int y0, int x1, int y1, bool enabled);

 signals:
  void roiChanged(beam::core::RectRoi roi);
  void crosshairChanged(int x, int y);
  void profileLineChanged(int x0, int y0, int x1, int y1);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  [[nodiscard]] QPoint frameFromWidget(const QPoint& widgetPos) const;
  [[nodiscard]] QRect widgetRectFromRoi(const beam::core::RectRoi& roi) const;
  [[nodiscard]] QPoint widgetFromFrame(int fx, int fy) const;

  QImage image_;
  int frameW_ = 0;
  int frameH_ = 0;
  InteractionMode mode_ = InteractionMode::None;
  std::optional<beam::core::RectRoi> roi_;
  bool dragging_ = false;
  QPoint dragStartFrame_;
  QPoint crosshair_{0, 0};
  bool hasCrosshair_ = false;
  bool showCrosshair_ = true;
  bool showProfileLine_ = false;
  int lineX0_ = 0;
  int lineY0_ = 0;
  int lineX1_ = 0;
  int lineY1_ = 0;
};

}  // namespace beam::ui
