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
  profile_ = profile;
  title_ = title;
  update();
}

void ProfilePlot::clearProfile() {
  profile_ = {};
  title_.clear();
  update();
}

void ProfilePlot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(28, 28, 30));
  p.setPen(QColor(180, 180, 180));
  p.drawText(8, 16, title_.isEmpty() ? QStringLiteral("剖面") : title_);

  if (!profile_.valid || profile_.intensity.size() < 2) {
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("无剖面数据"));
    return;
  }

  const int left = 40;
  const int right = width() - 12;
  const int top = 28;
  const int bottom = height() - 24;
  if (right <= left || bottom <= top) {
    return;
  }

  double yMin = *std::min_element(profile_.intensity.begin(), profile_.intensity.end());
  double yMax = *std::max_element(profile_.intensity.begin(), profile_.intensity.end());
  if (!(yMax > yMin)) {
    yMax = yMin + 1.0;
  }
  const double x0 = profile_.position_um.front();
  const double x1 = profile_.position_um.back();
  const double xSpan = (x1 > x0) ? (x1 - x0) : 1.0;

  p.setPen(QColor(70, 70, 74));
  p.drawRect(left, top, right - left, bottom - top);

  QPolygonF poly;
  poly.reserve(static_cast<int>(profile_.intensity.size()));
  for (std::size_t i = 0; i < profile_.intensity.size(); ++i) {
    const double xn = (profile_.position_um[i] - x0) / xSpan;
    const double yn = (profile_.intensity[i] - yMin) / (yMax - yMin);
    const double px = left + xn * (right - left);
    const double py = bottom - yn * (bottom - top);
    poly << QPointF(px, py);
  }

  p.setPen(QPen(QColor(80, 200, 255), 1.5));
  p.drawPolyline(poly);

  p.setPen(QColor(150, 150, 150));
  p.drawText(QRect(left, bottom + 2, right - left, 20), Qt::AlignLeft,
             QStringLiteral("%1 µm").arg(x0, 0, 'g', 4));
  p.drawText(QRect(left, bottom + 2, right - left, 20), Qt::AlignRight,
             QStringLiteral("%1 µm").arg(x1, 0, 'g', 4));
  p.drawText(QRect(2, top, left - 4, 16), Qt::AlignRight,
             QString::number(yMax, 'g', 3));
  p.drawText(QRect(2, bottom - 14, left - 4, 16), Qt::AlignRight,
             QString::number(yMin, 'g', 3));
}

}  // namespace beam::ui
