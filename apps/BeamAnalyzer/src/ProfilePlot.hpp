#pragma once

#include "beam/core/Profile.hpp"

#include <QWidget>

namespace beam::ui {

/// Lightweight 1D profile plot (no external chart dependency).
class ProfilePlot final : public QWidget {
  Q_OBJECT
 public:
  explicit ProfilePlot(QWidget* parent = nullptr);

  void setProfile(const beam::core::LineProfile& profile, const QString& title);
  void clearProfile();

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override { return {320, 140}; }
  QSize minimumSizeHint() const override { return {200, 100}; }

 private:
  beam::core::LineProfile profile_;
  QString title_;
};

}  // namespace beam::ui
