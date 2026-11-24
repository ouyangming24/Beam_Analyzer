#include "ProfilePlot.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

namespace beam::ui {

ProfilePlot::ProfilePlot(QWidget* parent) : QWidget(parent) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setObjectName(QStringLiteral("ProfilePlot"));
}

void ProfilePlot::setProfile(const beam::core::LineProfile& profile, const QString& title) {
  series_.clear();
  title_ = title;
  xLabel_ = QStringLiteral("µm");
  yLabel_.clear();
  if (profile.valid && profile.position_um.size() == profile.intensity.size() &&
      profile.position_um.size() >= 2) {
    PlotSeries s;
    s.x = profile.position_um;
    s.y = profile.intensity;
    s.color = QColor(64, 196, 255);
    s.penWidth = 1.6;
    s.markers = false;
    s.legend = QStringLiteral("I");
    series_.push_back(std::move(s));
  }
  update();
}

void ProfilePlot::setSeries(const std::vector<double>& x,
                            const std::vector<double>& y,
                            const QString& title,
                            const QString& xLabel,
                            const QString& yLabel) {
  series_.clear();
  title_ = title;
  xLabel_ = xLabel;
  yLabel_ = yLabel;
  if (x.size() >= 2 && x.size() == y.size()) {
    PlotSeries s;
    s.x = x;
    s.y = y;
    s.color = QColor(64, 196, 255);
    s.penWidth = 1.5;
    s.markers = true;
    s.legend = QStringLiteral("data");
    series_.push_back(std::move(s));
  }
  update();
}

void ProfilePlot::setOverlaySeries(const std::vector<double>& x, const std::vector<double>& y) {
  // Keep first measured series; replace/add dashed overlay as series[1].
  if (series_.size() > 1) {
    series_.resize(1);
  }
  if (x.size() >= 2 && x.size() == y.size()) {
    PlotSeries s;
    s.x = x;
    s.y = y;
    s.color = QColor(255, 176, 64);
    s.penStyle = Qt::DashLine;
    s.penWidth = 1.3;
    s.markers = false;
    s.legend = QStringLiteral("fit");
    series_.push_back(std::move(s));
  }
  update();
}

void ProfilePlot::setMultiSeries(const std::vector<PlotSeries>& series,
                                 const QString& title,
                                 const QString& xLabel,
                                 const QString& yLabel) {
  series_ = series;
  title_ = title;
  xLabel_ = xLabel;
  yLabel_ = yLabel;
  update();
}

void ProfilePlot::clearProfile() {
  series_.clear();
  title_.clear();
  xLabel_.clear();
  yLabel_.clear();
  update();
}

void ProfilePlot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QColor bg = palette().color(QPalette::Base);
  const QColor fg = palette().color(QPalette::WindowText);
  const QColor grid = QColor(fg.red(), fg.green(), fg.blue(), 40);
  const QColor muted(fg.red(), fg.green(), fg.blue(), 160);

  p.fillRect(rect(), bg);
  p.setPen(muted);
  QFont titleFont = font();
  titleFont.setBold(true);
  titleFont.setPointSizeF(std::max(8.0, titleFont.pointSizeF()));
  p.setFont(titleFont);
  p.drawText(8, 15, title_.isEmpty() ? QStringLiteral("曲线") : title_);

  bool any = false;
  for (const auto& s : series_) {
    if (s.x.size() >= 2 && s.x.size() == s.y.size()) {
      any = true;
      break;
    }
  }
  if (!any) {
    p.setFont(font());
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("无数据"));
    return;
  }

  const int left = 52;
  const int right = width() - 10;
  const int top = 26;
  const int bottom = height() - 28;
  if (right <= left || bottom <= top) {
    return;
  }

  double yMin = 0;
  double yMax = 0;
  double x0 = 0;
  double x1 = 0;
  bool init = false;
  for (const auto& s : series_) {
    if (s.x.size() < 2 || s.x.size() != s.y.size()) {
      continue;
    }
    for (std::size_t i = 0; i < s.y.size(); ++i) {
      if (!init) {
        yMin = yMax = s.y[i];
        x0 = x1 = s.x[i];
        init = true;
      } else {
        yMin = std::min(yMin, s.y[i]);
        yMax = std::max(yMax, s.y[i]);
        x0 = std::min(x0, s.x[i]);
        x1 = std::max(x1, s.x[i]);
      }
    }
  }
  if (!init) {
    return;
  }
  if (!(yMax > yMin)) {
    yMax = yMin + 1.0;
  }
  const double xSpan = (x1 > x0) ? (x1 - x0) : 1.0;
  const double yPad = 0.04 * (yMax - yMin);
  yMin -= yPad;
  yMax += yPad;

  p.setPen(QPen(grid, 1));
  p.setBrush(Qt::NoBrush);
  p.drawRect(left, top, right - left, bottom - top);
  // Light horizontal guides
  for (int g = 1; g < 4; ++g) {
    const int yy = top + (bottom - top) * g / 4;
    p.drawLine(left, yy, right, yy);
  }

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

  for (const auto& s : series_) {
    if (s.x.size() < 2 || s.x.size() != s.y.size()) {
      continue;
    }
    p.setPen(QPen(s.color, s.penWidth, s.penStyle));
    p.drawPolyline(toPoly(s.x, s.y));
    if (s.markers) {
      p.setBrush(s.color);
      p.setPen(Qt::NoPen);
      for (std::size_t i = 0; i < s.x.size(); ++i) {
        const double xn = (s.x[i] - x0) / xSpan;
        const double yn = (s.y[i] - yMin) / (yMax - yMin);
        p.drawEllipse(QPointF(left + xn * (right - left), bottom - yn * (bottom - top)), 2.4,
                      2.4);
      }
    }
  }

  // Legend (top-right inside plot)
  {
    QFont lf = font();
    lf.setPointSizeF(std::max(7.5, lf.pointSizeF() - 1));
    p.setFont(lf);
    int ly = top + 4;
    for (const auto& s : series_) {
      if (s.legend.isEmpty()) {
        continue;
      }
      const int lx = right - 92;
      p.setPen(QPen(s.color, 2, s.penStyle));
      p.drawLine(lx, ly + 6, lx + 16, ly + 6);
      p.setPen(muted);
      p.drawText(lx + 20, ly + 10, s.legend);
      ly += 14;
      if (ly > top + 56) {
        break;
      }
    }
  }

  p.setFont(font());
  p.setPen(muted);
  const QString xl = xLabel_.isEmpty() ? QString() : QStringLiteral(" ") + xLabel_;
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
