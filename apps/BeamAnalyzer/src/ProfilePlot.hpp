#pragma once

#include "beam/core/Profile.hpp"

#include <QColor>
#include <QWidget>
#include <vector>

namespace beam::ui {

struct PlotSeries {
  std::vector<double> x;
  std::vector<double> y;
  QColor color = QColor(80, 200, 255);
  Qt::PenStyle penStyle = Qt::SolidLine;
  qreal penWidth = 1.5;
  bool markers = false;
  QString legend;
};

/// Lightweight multi-series 1D plot (profiles / dual-axis caustic; no chart lib).
class ProfilePlot final : public QWidget {
  Q_OBJECT
 public:
  explicit ProfilePlot(QWidget* parent = nullptr);

  void setProfile(const beam::core::LineProfile& profile, const QString& title);
  /// Generic XY series (e.g. z_mm vs diameter_um). Optional second series overlay.
  void setSeries(const std::vector<double>& x,
                 const std::vector<double>& y,
                 const QString& title,
                 const QString& xLabel = QString(),
                 const QString& yLabel = QString());
  void setOverlaySeries(const std::vector<double>& x, const std::vector<double>& y);
  /// Replace all series (measured + fits) for dual-axis caustic views.
  void setMultiSeries(const std::vector<PlotSeries>& series,
                      const QString& title,
                      const QString& xLabel = QString(),
                      const QString& yLabel = QString());
  /// Draw a horizontal reference at y=0 (residual charts).
  void setZeroLineEnabled(bool enabled);
  /// Optional vertical band (e.g. AOI extent along the profile axis, µm).
  void setHighlightRange(double x0, double x1, bool enabled);
  void clearHighlightRange();
  void clearProfile();

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override { return {320, 140}; }
  QSize minimumSizeHint() const override { return {200, 100}; }

 private:
  std::vector<PlotSeries> series_;
  QString title_;
  QString xLabel_;
  QString yLabel_;
  bool zeroLine_ = false;
  bool highlightRange_ = false;
  double highlightX0_ = 0.0;
  double highlightX1_ = 0.0;
};

}  // namespace beam::ui
