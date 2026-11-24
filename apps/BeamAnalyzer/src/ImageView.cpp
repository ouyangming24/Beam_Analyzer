#include "ImageView.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

namespace beam::ui {

ImageView::ImageView(QWidget* parent) : QLabel(parent) {
  setAlignment(Qt::AlignCenter);
  setMinimumSize(160, 120);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setScaledContents(false);
  setStyleSheet(QStringLiteral("QLabel { background: #070b11; color: #9fb4cc; }"));
  setMouseTracking(true);
}

void ImageView::setDisplayImage(const QImage& image, int frameW, int frameH) {
  image_ = image;
  frameW_ = frameW;
  frameH_ = frameH;
  setPixmap(QPixmap::fromImage(image_));
  setText({});
  update();
}

void ImageView::clearFrame() {
  image_ = {};
  frameW_ = frameH_ = 0;
  clear();
  setText(QStringLiteral("打开图像/CSV、拖放文件，或连接模拟相机"));
}

void ImageView::setInteractionMode(InteractionMode mode) {
  mode_ = mode;
  dragMode_ = DragMode::None;
}

void ImageView::setAoiShape(beam::core::AoiShape shape) {
  aoiShape_ = shape;
  if (aoi_) {
    aoi_->shape = shape;
    update();
  }
}

void ImageView::setAoi(const std::optional<beam::core::AoiRegion>& aoi) {
  aoi_ = aoi;
  update();
}

void ImageView::setRoi(const std::optional<beam::core::RectRoi>& roi) {
  if (!roi || roi->empty()) {
    aoi_.reset();
  } else {
    aoi_ = beam::core::aoiFromRect(*roi);
    aoi_->shape = aoiShape_;
  }
  update();
}

std::optional<beam::core::RectRoi> ImageView::roi() const {
  if (!aoi_ || aoi_->empty()) {
    return std::nullopt;
  }
  return aoi_->bounds;
}

void ImageView::setCrosshair(int x, int y) {
  crosshair_ = {x, y};
  hasCrosshair_ = true;
  update();
}

void ImageView::setShowCrosshair(bool on) {
  showCrosshair_ = on;
  update();
}

void ImageView::setProfileLine(int x0, int y0, int x1, int y1, bool enabled) {
  lineX0_ = x0;
  lineY0_ = y0;
  lineX1_ = x1;
  lineY1_ = y1;
  showProfileLine_ = enabled;
  update();
}

void ImageView::setBeamOverlay(const std::optional<beam::core::Iso11146Metrics>& metrics,
                               bool enabled,
                               double pixelSizeX_um,
                               double pixelSizeY_um) {
  beamMetrics_ = metrics;
  showBeamOverlay_ = enabled && metrics && metrics->valid;
  overlayPitchX_um_ = std::max(1e-9, pixelSizeX_um);
  overlayPitchY_um_ = std::max(1e-9, pixelSizeY_um);
  update();
}

QPoint ImageView::frameFromWidget(const QPoint& widgetPos) const {
  if (frameW_ <= 0 || frameH_ <= 0 || pixmap(Qt::ReturnByValue).isNull()) {
    return {-1, -1};
  }
  const QPixmap pm = pixmap(Qt::ReturnByValue);
  const int pmW = pm.width();
  const int pmH = pm.height();
  const int ox = (width() - pmW) / 2;
  const int oy = (height() - pmH) / 2;
  const int lx = widgetPos.x() - ox;
  const int ly = widgetPos.y() - oy;
  if (lx < 0 || ly < 0 || lx >= pmW || ly >= pmH) {
    return {-1, -1};
  }
  const int fx = std::clamp(static_cast<int>(static_cast<double>(lx) * frameW_ / pmW),
                            0, frameW_ - 1);
  const int fy = std::clamp(static_cast<int>(static_cast<double>(ly) * frameH_ / pmH),
                            0, frameH_ - 1);
  return {fx, fy};
}

QPoint ImageView::widgetFromFrame(int fx, int fy) const {
  return widgetFromFrameF(fx + 0.5, fy + 0.5).toPoint();
}

QPointF ImageView::widgetFromFrameF(double fx, double fy) const {
  if (frameW_ <= 0 || frameH_ <= 0 || pixmap(Qt::ReturnByValue).isNull()) {
    return {};
  }
  const QPixmap pm = pixmap(Qt::ReturnByValue);
  const int pmW = pm.width();
  const int pmH = pm.height();
  const int ox = (width() - pmW) / 2;
  const int oy = (height() - pmH) / 2;
  const double lx = fx * pmW / frameW_;
  const double ly = fy * pmH / frameH_;
  return {ox + lx, oy + ly};
}

QRect ImageView::widgetRectFromRoi(const beam::core::RectRoi& roi) const {
  const QPointF tl = widgetFromFrameF(roi.x0, roi.y0);
  const QPointF br = widgetFromFrameF(roi.x1 + 1.0, roi.y1 + 1.0);
  return QRectF(tl, br).normalized().toRect();
}

bool ImageView::aoiContainsWidget(const QPoint& widgetPos) const {
  if (!aoi_ || aoi_->empty()) {
    return false;
  }
  const QPoint fp = frameFromWidget(widgetPos);
  if (fp.x() < 0) {
    return false;
  }
  return aoi_->containsPx(fp.x() + 0.5, fp.y() + 0.5);
}

void ImageView::mousePressEvent(QMouseEvent* event) {
  if (frameW_ <= 0) {
    QLabel::mousePressEvent(event);
    return;
  }
  const QPoint fp = frameFromWidget(event->pos());
  if (fp.x() < 0) {
    return;
  }

  // Right-click always moves the crosshair (line profiles sync to it), even in
  // Draw-AOI mode so AOI editing does not trap profile interaction.
  if (event->button() == Qt::RightButton) {
    dragMode_ = DragMode::Crosshair;
    setCrosshair(fp.x(), fp.y());
    emit crosshairChanged(fp.x(), fp.y());
    return;
  }

  if (mode_ == InteractionMode::DrawAoi && event->button() == Qt::LeftButton) {
    if (aoi_ && !aoi_->empty() && aoiContainsWidget(event->pos())) {
      dragMode_ = DragMode::Move;
      dragStartFrame_ = fp;
      dragStartBounds_ = aoi_->bounds;
    } else {
      dragMode_ = DragMode::Create;
      dragStartFrame_ = fp;
      beam::core::AoiRegion r;
      r.shape = aoiShape_;
      r.bounds = {fp.x(), fp.y(), fp.x(), fp.y()};
      aoi_ = r;
    }
    update();
    return;
  }

  if (event->button() == Qt::LeftButton &&
      (mode_ == InteractionMode::PickCrosshair || mode_ == InteractionMode::None)) {
    dragMode_ = DragMode::Crosshair;
    setCrosshair(fp.x(), fp.y());
    emit crosshairChanged(fp.x(), fp.y());
  }
}

void ImageView::mouseMoveEvent(QMouseEvent* event) {
  const QPoint fp = frameFromWidget(event->pos());
  if (dragMode_ == DragMode::Crosshair) {
    if (fp.x() >= 0) {
      setCrosshair(fp.x(), fp.y());
      emit crosshairChanged(fp.x(), fp.y());
    }
    return;
  }

  if (dragMode_ == DragMode::None || mode_ != InteractionMode::DrawAoi) {
    QLabel::mouseMoveEvent(event);
    return;
  }
  if (fp.x() < 0 || !aoi_) {
    return;
  }

  if (dragMode_ == DragMode::Create) {
    beam::core::RectRoi r;
    r.x0 = std::min(dragStartFrame_.x(), fp.x());
    r.y0 = std::min(dragStartFrame_.y(), fp.y());
    r.x1 = std::max(dragStartFrame_.x(), fp.x());
    r.y1 = std::max(dragStartFrame_.y(), fp.y());
    aoi_->bounds = r;
    aoi_->shape = aoiShape_;
  } else if (dragMode_ == DragMode::Move) {
    const int dx = fp.x() - dragStartFrame_.x();
    const int dy = fp.y() - dragStartFrame_.y();
    beam::core::RectRoi r = dragStartBounds_;
    r.x0 += dx;
    r.x1 += dx;
    r.y0 += dy;
    r.y1 += dy;
    // Clamp into frame
    const int w = r.width();
    const int h = r.height();
    r.x0 = std::clamp(r.x0, 0, frameW_ - 1);
    r.y0 = std::clamp(r.y0, 0, frameH_ - 1);
    r.x1 = std::clamp(r.x0 + w - 1, 0, frameW_ - 1);
    r.y1 = std::clamp(r.y0 + h - 1, 0, frameH_ - 1);
    aoi_->bounds = r;
  }
  update();
}

void ImageView::mouseReleaseEvent(QMouseEvent* event) {
  if (dragMode_ == DragMode::Crosshair &&
      (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
    dragMode_ = DragMode::None;
    QLabel::mouseReleaseEvent(event);
    return;
  }
  if (dragMode_ != DragMode::None && mode_ == InteractionMode::DrawAoi &&
      event->button() == Qt::LeftButton) {
    dragMode_ = DragMode::None;
    if (aoi_ && !aoi_->empty()) {
      emit aoiChanged(*aoi_);
      emit roiChanged(aoi_->bounds);
    }
  }
  QLabel::mouseReleaseEvent(event);
}

void ImageView::paintAoi(QPainter& p) const {
  if (!aoi_ || aoi_->empty()) {
    return;
  }
  QPen pen(QColor(0, 220, 255), 2, Qt::DashLine);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  const QRect box = widgetRectFromRoi(aoi_->bounds);
  switch (aoi_->shape) {
    case beam::core::AoiShape::Rect:
      p.drawRect(box);
      break;
    case beam::core::AoiShape::Circle: {
      const int side = std::min(box.width(), box.height());
      QRect c(0, 0, side, side);
      c.moveCenter(box.center());
      p.drawEllipse(c);
      break;
    }
    case beam::core::AoiShape::Ellipse:
      p.drawEllipse(box);
      break;
  }
}

void ImageView::paintBeamOverlay(QPainter& p) const {
  if (!showBeamOverlay_ || !beamMetrics_ || !beamMetrics_->valid || frameW_ <= 0) {
    return;
  }
  const double cxp = beamMetrics_->centroidX_um / overlayPitchX_um_;
  const double cyp = beamMetrics_->centroidY_um / overlayPitchY_um_;
  const QPointF c = widgetFromFrameF(cxp, cyp);

  QPen cross(QColor(255, 80, 220), 2, Qt::SolidLine);
  p.setPen(cross);
  p.drawLine(QPointF(c.x() - 10, c.y()), QPointF(c.x() + 10, c.y()));
  p.drawLine(QPointF(c.x(), c.y() - 10), QPointF(c.x(), c.y() + 10));

  const double ang = beamMetrics_->orientation_rad * 180.0 / 3.14159265358979323846;
  const double major = 0.5 * beamMetrics_->diameterMajor_um / overlayPitchX_um_;
  const double minor = 0.5 * beamMetrics_->diameterMinor_um / overlayPitchY_um_;
  QPen ep(QColor(255, 220, 60), 1.5, Qt::SolidLine);
  p.setPen(ep);
  p.setBrush(Qt::NoBrush);
  p.save();
  p.translate(c);
  p.rotate(-ang);
  const QPixmap pm = pixmap(Qt::ReturnByValue);
  const double sx = pm.isNull() ? 1.0 : static_cast<double>(pm.width()) / frameW_;
  const double sy = pm.isNull() ? 1.0 : static_cast<double>(pm.height()) / frameH_;
  p.drawEllipse(QPointF(0, 0), major * sx, minor * sy);
  p.restore();
}

void ImageView::paintOverlayLegend(QPainter& p) const {
  if (frameW_ <= 0) {
    return;
  }
  QFont f = font();
  f.setPointSizeF(std::max(8.0, f.pointSizeF() - 0.5));
  p.setFont(f);
  const int pad = 8;
  int y = pad + 12;
  const int x = pad;
  auto row = [&](const QColor& color, Qt::PenStyle style, const QString& text) {
    p.setPen(QPen(color, 2, style));
    p.drawLine(x, y - 4, x + 16, y - 4);
    p.setPen(QColor(220, 230, 240));
    p.drawText(x + 22, y, text);
    y += 16;
  };
  if (showBeamOverlay_) {
    row(QColor(255, 220, 60), Qt::SolidLine, QStringLiteral("ISO 4σ 光束椭圆"));
    row(QColor(255, 80, 220), Qt::SolidLine, QStringLiteral("质心"));
  }
  if (aoi_ && !aoi_->empty()) {
    row(QColor(0, 220, 255), Qt::DashLine, QStringLiteral("AOI"));
  }
  if (showCrosshair_ && hasCrosshair_) {
    row(QColor(255, 240, 0), Qt::SolidLine, QStringLiteral("剖面十字线"));
  }
}

void ImageView::paintEvent(QPaintEvent* event) {
  QLabel::paintEvent(event);
  if (frameW_ <= 0) {
    return;
  }
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  paintAoi(p);
  paintBeamOverlay(p);

  if (showCrosshair_ && hasCrosshair_) {
    const QPoint c = widgetFromFrame(crosshair_.x(), crosshair_.y());
    QPen pen(QColor(255, 240, 0), 1, Qt::SolidLine);
    p.setPen(pen);
    p.drawLine(0, c.y(), width(), c.y());
    p.drawLine(c.x(), 0, c.x(), height());
  }

  if (showProfileLine_) {
    const QPoint a = widgetFromFrame(lineX0_, lineY0_);
    const QPoint b = widgetFromFrame(lineX1_, lineY1_);
    QPen pen(QColor(255, 80, 80), 2, Qt::SolidLine);
    p.setPen(pen);
    p.drawLine(a, b);
  }

  paintOverlayLegend(p);
}

}  // namespace beam::ui
