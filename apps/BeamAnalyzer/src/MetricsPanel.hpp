#pragma once

#include "beam/camera/ICamera.hpp"
#include "beam/core/BeamFrame.hpp"
#include "beam/core/BeamWidths.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Roi.hpp"

#include <QWidget>

#include <optional>
#include <unordered_map>
#include <vector>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QCheckBox;
class QPushButton;

namespace beam::ui {

struct MetricStat {
  double last = 0.0;
  double sum = 0.0;
  double sumSq = 0.0;
  double minV = 0.0;
  double maxV = 0.0;
  int n = 0;
  void reset() { *this = {}; }
  void push(double v);
  [[nodiscard]] double mean() const;
  [[nodiscard]] double stddev() const;
};

/// Grouped, checkable results table (instrument-style) with optional rolling stats.
class MetricsPanel final : public QWidget {
  Q_OBJECT
 public:
  explicit MetricsPanel(QWidget* parent = nullptr);

  void clear();
  void setSource(const QString& source);
  void setLive(bool live, double fpsHint = 0.0);
  void setDisclaimerVisible(bool on);

  void updateMetrics(const beam::core::Iso11146Metrics& metrics,
                     const std::optional<beam::core::PowerMetrics>& power,
                     const std::optional<beam::core::BeamWidthSuite>& widths,
                     const beam::core::BeamFrame* frame,
                     const std::optional<beam::core::RectRoi>& roi,
                     bool roiEnabled,
                     int crossX,
                     int crossY,
                     const beam::camera::CameraControls* camControls);

  void resetStatistics();
  [[nodiscard]] QStringList enabledKeys() const;
  void setEnabledKeys(const QStringList& keys);
  [[nodiscard]] bool statisticsEnabled() const;

 signals:
  void selectionChanged();

 private:
  QTreeWidgetItem* addGroup(const QString& title, const QString& key);
  QTreeWidgetItem* addItem(QTreeWidgetItem* group,
                           const QString& key,
                           const QString& label,
                           const QString& unit);
  void setItemValue(const QString& key, double value, int prec = 3);
  void setItemText(const QString& key, const QString& text);
  void rebuildVisibility();
  void onItemChanged(QTreeWidgetItem* item, int column);

  QLabel* sourceLabel_ = nullptr;
  QLabel* liveLabel_ = nullptr;
  QLabel* disclaimerLabel_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QCheckBox* statsCheck_ = nullptr;
  QPushButton* resetStatsBtn_ = nullptr;

  std::unordered_map<std::string, QTreeWidgetItem*> items_;
  std::unordered_map<std::string, MetricStat> stats_;
  bool blockChange_ = false;
};

}  // namespace beam::ui
