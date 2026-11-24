#include "ProfilePlot.hpp"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace beam::ui {

ProfilePlot::ProfilePlot(QWidget* parent) : QWidget(parent) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
}

void ProfilePlot::setProfile(const beam::core::LineProfile& profile, const QString& title) {
  x_ = profile.position_um;
  y_ = profile.intensity;
  overlayX_.clear();
  overlayY_.clear();
  title_ = title;
  xLabel_ = QStringLiteral("µm");
  yLabel_.clear();
  if (!profile.valid) {
    x_.clear();
    y_.clear();
  }
  update();
}

void ProfilePlot::setSeries(const std::vector<double>& x,
                            const std::vector<double>& y,
                            const QString& title,
                            const QString& xLabel,
                            const QString& yLabel) {
  x_ = x;
  y_ = y;
  title_ = title;
  xLabel_ = xLabel;
  yLabel_ = yLabel;
  update();
}

void ProfilePlot::setOverlaySeries(const std::vector<double>& x, const std::vector<double>& y) {
  overlayX_ = x;
  overlayY_ = y;
  update();
}

void ProfilePlot::clearProfile() {
  x_.clear();
  y_.clear();
  overlayX_.clear();
  overlayY_.clear();
  title_.clear();
  xLabel_.clear();
  yLabel_.clear();
  update();
}

void ProfilePlot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(28, 28, 30));
  p.setPen(QColor(180, 180, 180));
  p.drawText(8, 16, title_.isEmpty() ? QStringLiteral("曲线") : title_);

  if (x_.size() < 2 || y_.size() < 2 || x_.size() != y_.size()) {
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("无数据"));
    return;
  }

  const int left = 48;
  const int right = width() - 12;
  const int top = 28;
  const int bottom = height() - 24;
  if (right <= left || bottom <= top) {
    return;
  }

  double yMin = *std::min_element(y_.begin(), y_.end());
  double yMax = *std::max_element(y_.begin(), y_.end());
  for (double v : overlayY_) {
    yMin = std::min(yMin, v);
    yMax = std::max(yMax, v);
  }
  if (!(yMax > yMin)) {
    yMax = yMin + 1.0;
  }
  const double x0 = *std::min_element(x_.begin(), x_.end());
  const double x1 = *std::max_element(x_.begin(), x_.end());
  const double xSpan = (x1 > x0) ? (x1 - x0) : 1.0;

  p.setPen(QColor(70, 70, 74));
  p.drawRect(left, top, right - left, bottom - top);

  auto toPoly = [&](const std::vector<double>& xs, const std::vector<double>& ys) {
    QPolygonF poly;
    const std::size_t n = std::min(xs.size(), ys.size());
    poly.reserve(static_cast<int>(n));
    for (std::size_t i = 0; i < n; ++i) {
      const double xn = (xs[i] - x0) / xSpan;
      const double yn = (ys[i] - yMin) / (yMax - yMin);
      poly << QPointF(left + xn * (right - left), bottom - yn * (bottom - top));
    }
    return poly;
  };

  if (overlayX_.size() >= 2 && overlayX_.size() == overlayY_.size()) {
    p.setPen(QPen(QColor(255, 180, 80), 1.2, Qt::DashLine));
    p.drawPolyline(toPoly(overlayX_, overlayY_));
  }

  p.setPen(QPen(QColor(80, 200, 255), 1.5));
  p.drawPolyline(toPoly(x_, y_));

  // Measured markers
  p.setBrush(QColor(80, 200, 255));
  for (std::size_t i = 0; i < x_.size(); ++i) {
    const double xn = (x_[i] - x0) / xSpan;
    const double yn = (y_[i] - yMin) / (yMax - yMin);
    p.drawEllipse(QPointF(left + xn * (right - left), bottom - yn * (bottom - top)), 2.5, 2.5);
  }

  p.setPen(QColor(150, 150, 150));
  const QString xl = xLabel_.isEmpty() ? QStringLiteral("") : QStringLiteral(" ") + xLabel_;
  p.drawText(QRect(left, bottom + 2, right - left, 20), Qt::AlignLeft,
             QStringLiteral("%1%2").arg(x0, 0, 'g', 4).arg(xl));
  p.drawText(QRect(left, bottom + 2, right - left, 20), Qt::AlignRight,
             QStringLiteral("%1%2").arg(x1, 0, 'g', 4).arg(xl));
  p.drawText(QRect(2, top, left - 4, 16), Qt::AlignRight, QString::number(yMax, 'g', 3));
  p.drawText(QRect(2, bottom - 14, left - 4, 16), Qt::AlignRight,
             QString::number(yMin, 'g', 3));
  if (!yLabel_.isEmpty()) {
    p.drawText(QRect(2, (top + bottom) / 2 - 8, left - 4, 16), Qt::AlignRight, yLabel_);
  }
}

}  // namespace beam::ui
