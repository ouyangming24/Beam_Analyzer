#include "IntensitySurface3D.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

namespace beam::ui {
namespace {

double clamp01(double t) { return std::clamp(t, 0.0, 1.0); }

}  // namespace

IntensitySurface3D::IntensitySurface3D(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("IntensitySurface3D"));
  setMinimumSize(200, 160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setToolTip(QStringLiteral("拖拽旋转 · 滚轮缩放 · 大图自动降采样显示"));
}

void IntensitySurface3D::setFrame(const beam::core::BeamFrame& frame) {
  rebuildGrid(frame);
  update();
}

void IntensitySurface3D::clearFrame() {
  grid_ = {};
  update();
}

void IntensitySurface3D::setAoi(const std::optional<beam::core::AoiRegion>& aoi) {
  aoi_ = aoi;
  update();
}

void IntensitySurface3D::setBeamOverlay(
    const std::optional<beam::core::Iso11146Metrics>& metrics) {
  metrics_ = metrics;
  update();
}

void IntensitySurface3D::rebuildGrid(const beam::core::BeamFrame& frame) {
  grid_ = {};
  if (frame.empty()) {
    return;
  }
  const int srcW = static_cast<int>(frame.width);
  const int srcH = static_cast<int>(frame.height);
  const int stepX = std::max(1, (srcW + kMaxDim - 1) / kMaxDim);
  const int stepY = std::max(1, (srcH + kMaxDim - 1) / kMaxDim);
  grid_.w = (srcW + stepX - 1) / stepX;
  grid_.h = (srcH + stepY - 1) / stepY;
  grid_.z.assign(static_cast<std::size_t>(grid_.w * grid_.h), 0.0);

  double zMin = 0.0;
  double zMax = 0.0;
  bool first = true;
  for (int j = 0; j < grid_.h; ++j) {
    for (int i = 0; i < grid_.w; ++i) {
      const int x0 = i * stepX;
      const int y0 = j * stepY;
      const int x1 = std::min(srcW, x0 + stepX);
      const int y1 = std::min(srcH, y0 + stepY);
      double sum = 0.0;
      int n = 0;
      for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
          sum += frame.intensity[static_cast<std::size_t>(y) * frame.width +
                                 static_cast<std::size_t>(x)];
          ++n;
        }
      }
      const double v = n > 0 ? sum / static_cast<double>(n) : 0.0;
      grid_.z[static_cast<std::size_t>(j * grid_.w + i)] = v;
      if (first) {
        zMin = zMax = v;
        first = false;
      } else {
        zMin = std::min(zMin, v);
        zMax = std::max(zMax, v);
      }
    }
  }
  grid_.zMin = zMin;
  grid_.zMax = zMax > zMin ? zMax : zMin + 1.0;
}

QColor IntensitySurface3D::colorForZ(double t) const {
  t = clamp01(t);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t < 0.25) {
    const double u = t / 0.25;
    g = static_cast<int>(255 * u);
    b = 255;
  } else if (t < 0.5) {
    const double u = (t - 0.25) / 0.25;
    g = 255;
    b = static_cast<int>(255 * (1.0 - u));
  } else if (t < 0.75) {
    const double u = (t - 0.5) / 0.25;
    r = static_cast<int>(255 * u);
    g = 255;
  } else {
    const double u = (t - 0.75) / 0.25;
    r = 255;
    g = static_cast<int>(255 * (1.0 - u));
  }
  return QColor(r, g, b);
}

QPointF IntensitySurface3D::project(double x, double y, double z,
                                    const QRect& viewport) const {
  constexpr double kPi = 3.14159265358979323846;
  const double yaw = yaw_deg_ * kPi / 180.0;
  const double pitch = pitch_deg_ * kPi / 180.0;
  const double cy = std::cos(yaw);
  const double sy = std::sin(yaw);
  const double cp = std::cos(pitch);
  const double sp = std::sin(pitch);

  // Center grid in [-0.5,0.5]², height in [0,1]
  const double X = x * cy - y * sy;
  const double Z = x * sy + y * cy;
  const double Y = z;
  const double X2 = X;
  const double Y2 = Y * cp - Z * sp;
  const double Z2 = Y * sp + Z * cp;

  const double scale =
      0.42 * std::min(viewport.width(), viewport.height()) * zoom_;
  const double cx = viewport.center().x();
  const double cyv = viewport.center().y() + 0.08 * viewport.height();
  return {cx + scale * X2, cyv - scale * (Y2 + 0.35 * Z2)};
}

void IntensitySurface3D::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), palette().color(QPalette::Window));
  p.setRenderHint(QPainter::Antialiasing, true);

  if (grid_.w < 2 || grid_.h < 2) {
    p.setPen(QColor(120, 140, 160));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("3D 强度曲面 · 无帧"));
    return;
  }

  const QRect vp = rect().adjusted(8, 8, -8, -28);
  struct Quad {
    QPointF pts[4];
    double depth = 0.0;
    double t = 0.0;
  };
  std::vector<Quad> quads;
  quads.reserve(static_cast<std::size_t>((grid_.w - 1) * (grid_.h - 1)));

  auto sample = [&](int i, int j) -> double {
    return grid_.z[static_cast<std::size_t>(j * grid_.w + i)];
  };
  auto normXY = [&](int i, int j, double& x, double& y, double& zn) {
    x = (static_cast<double>(i) / (grid_.w - 1)) - 0.5;
    y = (static_cast<double>(j) / (grid_.h - 1)) - 0.5;
    zn = (sample(i, j) - grid_.zMin) / (grid_.zMax - grid_.zMin);
  };

  for (int j = 0; j < grid_.h - 1; ++j) {
    for (int i = 0; i < grid_.w - 1; ++i) {
      double x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3;
      normXY(i, j, x0, y0, z0);
      normXY(i + 1, j, x1, y1, z1);
      normXY(i + 1, j + 1, x2, y2, z2);
      normXY(i, j + 1, x3, y3, z3);
      Quad q;
      q.pts[0] = project(x0, y0, z0, vp);
      q.pts[1] = project(x1, y1, z1, vp);
      q.pts[2] = project(x2, y2, z2, vp);
      q.pts[3] = project(x3, y3, z3, vp);
      q.t = 0.25 * (z0 + z1 + z2 + z3);
      q.depth = 0.25 * (q.pts[0].y() + q.pts[1].y() + q.pts[2].y() + q.pts[3].y());
      quads.push_back(q);
    }
  }

  std::sort(quads.begin(), quads.end(),
            [](const Quad& a, const Quad& b) { return a.depth < b.depth; });

  for (const Quad& q : quads) {
    QPolygonF poly;
    poly << q.pts[0] << q.pts[1] << q.pts[2] << q.pts[3];
    QColor fill = colorForZ(q.t);
    fill.setAlpha(210);
    p.setBrush(fill);
    p.setPen(QPen(fill.darker(130), 0.5));
    p.drawPolygon(poly);
  }

  // Ground grid hint
  p.setPen(QPen(QColor(34, 211, 238, 60), 1));
  for (int i = 0; i < 5; ++i) {
    const double t = i / 4.0 - 0.5;
    p.drawLine(project(t, -0.5, 0, vp), project(t, 0.5, 0, vp));
    p.drawLine(project(-0.5, t, 0, vp), project(0.5, t, 0, vp));
  }

  p.setPen(QColor(127, 149, 173));
  p.drawText(QRect(8, height() - 22, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter,
             QStringLiteral("yaw %1°  pitch %2°  zoom %3  grid %4×%5")
                 .arg(yaw_deg_, 0, 'f', 0)
                 .arg(pitch_deg_, 0, 'f', 0)
                 .arg(zoom_, 0, 'f', 2)
                 .arg(grid_.w)
                 .arg(grid_.h));
}

void IntensitySurface3D::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragging_ = true;
    lastPos_ = event->pos();
  }
}

void IntensitySurface3D::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) {
    return;
  }
  const QPoint d = event->pos() - lastPos_;
  lastPos_ = event->pos();
  yaw_deg_ += d.x() * 0.4;
  pitch_deg_ = std::clamp(pitch_deg_ + d.y() * 0.35, -15.0, 80.0);
  update();
}

void IntensitySurface3D::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    dragging_ = false;
  }
}

void IntensitySurface3D::wheelEvent(QWheelEvent* event) {
  const double steps = event->angleDelta().y() / 120.0;
  zoom_ = std::clamp(zoom_ * std::pow(1.1, steps), 0.4, 3.5);
  update();
}

}  // namespace beam::ui
