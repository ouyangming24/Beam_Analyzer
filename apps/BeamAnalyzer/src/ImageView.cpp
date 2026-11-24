#include "ImageView.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace beam::ui {

ImageView::ImageView(QWidget* parent) : QLabel(parent) {
  setAlignment(Qt::AlignCenter);
  setMinimumSize(480, 360);
  setStyleSheet(QStringLiteral("QLabel { background: #1a1a1a; color: #ccc; }"));
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
  setText(QStringLiteral("请打开 CSV 或加载模拟帧"));
}

void ImageView::setInteractionMode(InteractionMode mode) {
  mode_ = mode;
  dragging_ = false;
}

void ImageView::setRoi(const std::optional<beam::core::RectRoi>& roi) {
  roi_ = roi;
  update();
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
  if (frameW_ <= 0 || frameH_ <= 0 || pixmap(Qt::ReturnByValue).isNull()) {
    return {};
  }
  const QPixmap pm = pixmap(Qt::ReturnByValue);
  const int pmW = pm.width();
  const int pmH = pm.height();
  const int ox = (width() - pmW) / 2;
  const int oy = (height() - pmH) / 2;
  const int lx = static_cast<int>((fx + 0.5) * pmW / frameW_);
  const int ly = static_cast<int>((fy + 0.5) * pmH / frameH_);
  return {ox + lx, oy + ly};
}

QRect ImageView::widgetRectFromRoi(const beam::core::RectRoi& roi) const {
  const QPoint tl = widgetFromFrame(roi.x0, roi.y0);
  const QPoint br = widgetFromFrame(roi.x1, roi.y1);
  return QRect(tl, br).normalized();
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

  if (mode_ == InteractionMode::DrawRoi && event->button() == Qt::LeftButton) {
    dragging_ = true;
    dragStartFrame_ = fp;
    beam::core::RectRoi r{fp.x(), fp.y(), fp.x(), fp.y()};
    roi_ = r;
    update();
    return;
  }

  if (mode_ == InteractionMode::PickCrosshair && event->button() == Qt::LeftButton) {
    setCrosshair(fp.x(), fp.y());
    emit crosshairChanged(fp.x(), fp.y());
    return;
  }

  // Default: click sets crosshair for profiles
  if (event->button() == Qt::LeftButton && mode_ == InteractionMode::None) {
    setCrosshair(fp.x(), fp.y());
    emit crosshairChanged(fp.x(), fp.y());
  }
}

void ImageView::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_ || mode_ != InteractionMode::DrawRoi) {
    QLabel::mouseMoveEvent(event);
    return;
  }
  const QPoint fp = frameFromWidget(event->pos());
  if (fp.x() < 0) {
    return;
  }
  beam::core::RectRoi r;
  r.x0 = std::min(dragStartFrame_.x(), fp.x());
  r.y0 = std::min(dragStartFrame_.y(), fp.y());
  r.x1 = std::max(dragStartFrame_.x(), fp.x());
  r.y1 = std::max(dragStartFrame_.y(), fp.y());
  roi_ = r;
  update();
}

void ImageView::mouseReleaseEvent(QMouseEvent* event) {
  if (dragging_ && mode_ == InteractionMode::DrawRoi && event->button() == Qt::LeftButton) {
    dragging_ = false;
    if (roi_ && !roi_->empty()) {
      emit roiChanged(*roi_);
    }
  }
  QLabel::mouseReleaseEvent(event);
}

void ImageView::paintEvent(QPaintEvent* event) {
  QLabel::paintEvent(event);
  if (frameW_ <= 0) {
    return;
  }
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);

  if (roi_ && !roi_->empty()) {
    QPen pen(QColor(0, 220, 255), 2, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(widgetRectFromRoi(*roi_));
  }

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
}

}  // namespace beam::ui
