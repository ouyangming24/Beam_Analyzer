#include "MetricsPanel.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cmath>
#include <string>

namespace beam::ui {

void MetricStat::push(double v) {
  if (!std::isfinite(v)) {
    return;
  }
  if (n == 0) {
    minV = maxV = v;
  } else {
    minV = std::min(minV, v);
    maxV = std::max(maxV, v);
  }
  last = v;
  sum += v;
  sumSq += v * v;
  ++n;
}

double MetricStat::mean() const {
  return n > 0 ? sum / static_cast<double>(n) : 0.0;
}

double MetricStat::stddev() const {
  if (n < 2) {
    return 0.0;
  }
  const double m = mean();
  const double var = sumSq / static_cast<double>(n) - m * m;
  return var > 0.0 ? std::sqrt(var) : 0.0;
}

namespace {

QString num(double v, char fmt = 'f', int prec = 3) {
  if (!std::isfinite(v)) {
    return QStringLiteral("—");
  }
  return QString::number(v, fmt, prec);
}

}  // namespace

MetricsPanel::MetricsPanel(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("MetricsPanel"));
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(4);

  disclaimerLabel_ = new QLabel(
      QStringLiteral("概念实现 / 非正式计量证书 · 显示值供工程预览"));
  disclaimerLabel_->setObjectName(QStringLiteral("DisclaimerBanner"));
  disclaimerLabel_->setWordWrap(true);
  disclaimerLabel_->setStyleSheet(
      QStringLiteral("color:#fbbf24;font-size:11px;font-weight:600;padding:2px 0;"));
  root->addWidget(disclaimerLabel_);

  sourceLabel_ = new QLabel(QStringLiteral("来源: —"));
  sourceLabel_->setWordWrap(true);
  liveLabel_ = new QLabel(QStringLiteral("静止 / 离线"));
  root->addWidget(sourceLabel_);
  root->addWidget(liveLabel_);

  auto* ctrl = new QHBoxLayout;
  statsCheck_ = new QCheckBox(QStringLiteral("滚动统计"));
  statsCheck_->setToolTip(QStringLiteral("对已勾选数值累计 mean / σ / min / max"));
  resetStatsBtn_ = new QPushButton(QStringLiteral("重置统计"));
  connect(resetStatsBtn_, &QPushButton::clicked, this, &MetricsPanel::resetStatistics);
  ctrl->addWidget(statsCheck_);
  ctrl->addWidget(resetStatsBtn_);
  ctrl->addStretch(1);
  root->addLayout(ctrl);

  tree_ = new QTreeWidget;
  tree_->setColumnCount(3);
  tree_->setHeaderLabels({QStringLiteral("测量项"), QStringLiteral("数值"), QStringLiteral("单位")});
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  tree_->setRootIsDecorated(true);
  tree_->setUniformRowHeights(true);
  tree_->setAnimated(true);
  tree_->setSelectionMode(QAbstractItemView::NoSelection);
  connect(tree_, &QTreeWidget::itemChanged, this, &MetricsPanel::onItemChanged);
  root->addWidget(tree_, 1);

  blockChange_ = true;
  auto* gPeak = addGroup(QStringLiteral("峰值 / 功率"), QStringLiteral("grp.power"));
  addItem(gPeak, QStringLiteral("peak"), QStringLiteral("峰值"), QStringLiteral("counts"));
  addItem(gPeak, QStringLiteral("sum"), QStringLiteral("Σ counts"), QStringLiteral("a.u."));
  addItem(gPeak, QStringLiteral("mean"), QStringLiteral("均值"), QStringLiteral("counts"));
  addItem(gPeak, QStringLiteral("powerW"), QStringLiteral("功率"), QStringLiteral("W"));
  addItem(gPeak, QStringLiteral("energyJ"), QStringLiteral("能量"), QStringLiteral("J"));

  auto* gPos = addGroup(QStringLiteral("质心 / 位置"), QStringLiteral("grp.centroid"));
  addItem(gPos, QStringLiteral("cx"), QStringLiteral("质心 X"), QStringLiteral("µm"));
  addItem(gPos, QStringLiteral("cy"), QStringLiteral("质心 Y"), QStringLiteral("µm"));
  addItem(gPos, QStringLiteral("ori"), QStringLiteral("方位角"), QStringLiteral("°"));
  addItem(gPos, QStringLiteral("ellip"), QStringLiteral("椭圆度 ε"), QStringLiteral("—"));

  auto* gD4 = addGroup(QStringLiteral("宽度 · D4σ (ISO)"), QStringLiteral("grp.d4"));
  addItem(gD4, QStringLiteral("dx"), QStringLiteral("Dx (4σ)"), QStringLiteral("µm"));
  addItem(gD4, QStringLiteral("dy"), QStringLiteral("Dy (4σ)"), QStringLiteral("µm"));
  addItem(gD4, QStringLiteral("dMaj"), QStringLiteral("D major"), QStringLiteral("µm"));
  addItem(gD4, QStringLiteral("dMin"), QStringLiteral("D minor"), QStringLiteral("µm"));

  auto* gKe = addGroup(QStringLiteral("宽度 · 刀口"), QStringLiteral("grp.knife"));
  addItem(gKe, QStringLiteral("ke10x"), QStringLiteral("KE 10/90 X"), QStringLiteral("µm"));
  addItem(gKe, QStringLiteral("ke10y"), QStringLiteral("KE 10/90 Y"), QStringLiteral("µm"));
  addItem(gKe, QStringLiteral("ke16x"), QStringLiteral("KE 16/84 X"), QStringLiteral("µm"));
  addItem(gKe, QStringLiteral("ke16y"), QStringLiteral("KE 16/84 Y"), QStringLiteral("µm"));

  auto* gPct = addGroup(QStringLiteral("宽度 · %能量 / %峰值"), QStringLiteral("grp.pct"));
  addItem(gPct, QStringLiteral("pEx"), QStringLiteral("%E 86.5% X"), QStringLiteral("µm"));
  addItem(gPct, QStringLiteral("pEy"), QStringLiteral("%E 86.5% Y"), QStringLiteral("µm"));
  addItem(gPct, QStringLiteral("pEd"), QStringLiteral("%E 直径"), QStringLiteral("µm"));
  addItem(gPct, QStringLiteral("pPx"), QStringLiteral("%Peak 13.5% X"), QStringLiteral("µm"));
  addItem(gPct, QStringLiteral("pPy"), QStringLiteral("%Peak 13.5% Y"), QStringLiteral("µm"));
  addItem(gPct, QStringLiteral("epsa"), QStringLiteral("包围功率 D"), QStringLiteral("µm"));

  auto* gCtx = addGroup(QStringLiteral("上下文"), QStringLiteral("grp.ctx"));
  addItem(gCtx, QStringLiteral("roi"), QStringLiteral("AOI"), QString());
  addItem(gCtx, QStringLiteral("cross"), QStringLiteral("十字线"), QString());
  addItem(gCtx, QStringLiteral("pixel"), QStringLiteral("像素尺寸"), QStringLiteral("µm"));
  addItem(gCtx, QStringLiteral("cam"), QStringLiteral("相机"), QString());

  // Default: enable common ISO + power; leave advanced widths optional-on.
  for (const char* k : {"peak", "sum", "powerW", "cx", "cy", "dx", "dy", "dMaj", "dMin", "ellip",
                        "ori", "ke10x", "ke10y", "pEd", "epsa", "roi", "pixel"}) {
    if (auto it = items_.find(k); it != items_.end()) {
      it->second->setCheckState(0, Qt::Checked);
    }
  }
  tree_->expandAll();
  blockChange_ = false;

  clear();
}

QTreeWidgetItem* MetricsPanel::addGroup(const QString& title, const QString& key) {
  auto* g = new QTreeWidgetItem(tree_);
  g->setText(0, title);
  g->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
  g->setCheckState(0, Qt::Checked);
  g->setData(0, Qt::UserRole, key);
  g->setFirstColumnSpanned(true);
  items_[key.toStdString()] = g;
  return g;
}

QTreeWidgetItem* MetricsPanel::addItem(QTreeWidgetItem* group,
                                       const QString& key,
                                       const QString& label,
                                       const QString& unit) {
  auto* it = new QTreeWidgetItem(group);
  it->setText(0, label);
  it->setText(1, QStringLiteral("—"));
  it->setText(2, unit);
  it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
  it->setCheckState(0, Qt::Unchecked);
  it->setData(0, Qt::UserRole, key);
  it->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
  items_[key.toStdString()] = it;
  return it;
}

void MetricsPanel::setItemValue(const QString& key, double value, int prec) {
  auto it = items_.find(key.toStdString());
  if (it == items_.end()) {
    return;
  }
  if (it->second->checkState(0) != Qt::Checked) {
    it->second->setText(1, QStringLiteral("—"));
    return;
  }
  QString text = num(value, 'g', prec);
  if (statsCheck_ && statsCheck_->isChecked() && std::isfinite(value)) {
    auto& st = stats_[key.toStdString()];
    st.push(value);
    text = QStringLiteral("%1  ⟨%2⟩ σ%3")
               .arg(num(value, 'g', prec))
               .arg(num(st.mean(), 'g', prec))
               .arg(num(st.stddev(), 'g', prec));
  }
  it->second->setText(1, text);
}

void MetricsPanel::setItemText(const QString& key, const QString& text) {
  auto it = items_.find(key.toStdString());
  if (it == items_.end()) {
    return;
  }
  if (it->second->checkState(0) != Qt::Checked) {
    it->second->setText(1, QStringLiteral("—"));
    return;
  }
  it->second->setText(1, text);
}

void MetricsPanel::onItemChanged(QTreeWidgetItem* item, int column) {
  if (blockChange_ || column != 0 || !item) {
    return;
  }
  rebuildVisibility();
  emit selectionChanged();
}

void MetricsPanel::rebuildVisibility() {
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* g = tree_->topLevelItem(i);
    for (int c = 0; c < g->childCount(); ++c) {
      auto* child = g->child(c);
      child->setHidden(child->checkState(0) != Qt::Checked && g->checkState(0) == Qt::Unchecked);
      // Keep unchecked items visible but dimmed via "—" until checked — show all children.
      child->setHidden(false);
    }
  }
}

void MetricsPanel::clear() {
  setSource(QStringLiteral("尚无数据"));
  setLive(false);
  blockChange_ = true;
  for (auto& kv : items_) {
    if (kv.second && kv.second->parent()) {
      kv.second->setText(1, QStringLiteral("—"));
    }
  }
  blockChange_ = false;
}

void MetricsPanel::setSource(const QString& source) {
  sourceLabel_->setText(QStringLiteral("来源: %1").arg(source));
  sourceLabel_->setToolTip(source);
}

void MetricsPanel::setLive(bool live, double fpsHint) {
  if (live) {
    if (fpsHint > 0) {
      liveLabel_->setText(QStringLiteral("● LIVE  ~%1 fps").arg(fpsHint, 0, 'f', 1));
    } else {
      liveLabel_->setText(QStringLiteral("● LIVE"));
    }
    liveLabel_->setStyleSheet(QStringLiteral("color: #34d399; font-weight: 700;"));
  } else {
    liveLabel_->setText(QStringLiteral("静止 / 离线"));
    liveLabel_->setStyleSheet(QStringLiteral("font-weight: 600;"));
  }
}

void MetricsPanel::setDisclaimerVisible(bool on) {
  disclaimerLabel_->setVisible(on);
}

void MetricsPanel::resetStatistics() {
  stats_.clear();
}

bool MetricsPanel::statisticsEnabled() const {
  return statsCheck_ && statsCheck_->isChecked();
}

QStringList MetricsPanel::enabledKeys() const {
  QStringList keys;
  for (const auto& kv : items_) {
    if (kv.second && kv.second->parent() && kv.second->checkState(0) == Qt::Checked) {
      keys << QString::fromStdString(kv.first);
    }
  }
  keys.sort();
  return keys;
}

void MetricsPanel::setEnabledKeys(const QStringList& keys) {
  blockChange_ = true;
  const QSet<QString> set(keys.begin(), keys.end());
  for (auto& kv : items_) {
    if (kv.second && kv.second->parent()) {
      const QString k = QString::fromStdString(kv.first);
      kv.second->setCheckState(0, set.contains(k) ? Qt::Checked : Qt::Unchecked);
    }
  }
  blockChange_ = false;
}

void MetricsPanel::updateMetrics(const beam::core::Iso11146Metrics& metrics,
                                 const std::optional<beam::core::PowerMetrics>& power,
                                 const std::optional<beam::core::BeamWidthSuite>& widths,
                                 const beam::core::BeamFrame* frame,
                                 const std::optional<beam::core::RectRoi>& roi,
                                 bool roiEnabled,
                                 int crossX,
                                 int crossY,
                                 const beam::camera::CameraControls* camControls) {
  if (!metrics.valid) {
    setItemText(QStringLiteral("dx"), QStringLiteral("无效"));
    return;
  }

  setItemValue(QStringLiteral("cx"), metrics.centroidX_um);
  setItemValue(QStringLiteral("cy"), metrics.centroidY_um);
  setItemValue(QStringLiteral("dx"), metrics.diameterX_um);
  setItemValue(QStringLiteral("dy"), metrics.diameterY_um);
  setItemValue(QStringLiteral("dMaj"), metrics.diameterMajor_um);
  setItemValue(QStringLiteral("dMin"), metrics.diameterMinor_um);
  setItemValue(QStringLiteral("ori"), metrics.orientation_rad * 180.0 / 3.14159265358979323846);
  setItemValue(QStringLiteral("ellip"), metrics.ellipticity, 4);

  if (power && power->valid) {
    setItemValue(QStringLiteral("sum"), power->sumCounts, 6);
    setItemValue(QStringLiteral("peak"), power->peakCounts, 6);
    setItemValue(QStringLiteral("mean"), power->meanCounts, 6);
    if (power->calibrated) {
      setItemValue(QStringLiteral("powerW"), power->power_W, 6);
      setItemValue(QStringLiteral("energyJ"), power->energy_J, 6);
    } else {
      setItemText(QStringLiteral("powerW"), QStringLiteral("未标定"));
      setItemText(QStringLiteral("energyJ"), QStringLiteral("未标定"));
    }
  }

  if (widths && widths->valid) {
    setItemValue(QStringLiteral("ke10x"), widths->knife1090.widthX_um);
    setItemValue(QStringLiteral("ke10y"), widths->knife1090.widthY_um);
    setItemValue(QStringLiteral("ke16x"), widths->knife1684.widthX_um);
    setItemValue(QStringLiteral("ke16y"), widths->knife1684.widthY_um);
    setItemValue(QStringLiteral("pEx"), widths->percentEnergy.widthX_um);
    setItemValue(QStringLiteral("pEy"), widths->percentEnergy.widthY_um);
    setItemValue(QStringLiteral("pEd"), widths->percentEnergy.diameter_um);
    setItemValue(QStringLiteral("pPx"), widths->percentPeak.widthX_um);
    setItemValue(QStringLiteral("pPy"), widths->percentPeak.widthY_um);
    setItemValue(QStringLiteral("epsa"), widths->encircled.diameter_um);
  }

  if (roi && roiEnabled) {
    setItemText(QStringLiteral("roi"),
                QStringLiteral("[%1,%2]–[%3,%4]")
                    .arg(roi->x0)
                    .arg(roi->y0)
                    .arg(roi->x1)
                    .arg(roi->y1));
  } else {
    setItemText(QStringLiteral("roi"), QStringLiteral("关闭"));
  }

  if (crossX >= 0) {
    setItemText(QStringLiteral("cross"), QStringLiteral("(%1, %2) px").arg(crossX).arg(crossY));
  }

  if (frame) {
    setItemText(QStringLiteral("pixel"),
                QStringLiteral("%1 × %2")
                    .arg(num(frame->pixelSizeX_um))
                    .arg(num(frame->pixelSizeY_um)));
  }

  if (camControls) {
    setItemText(QStringLiteral("cam"),
                QStringLiteral("%1 | %2 µs | %3 dB")
                    .arg(QString::fromUtf8(beam::camera::toString(camControls->trigger)))
                    .arg(num(camControls->exposure_us, 'f', 1))
                    .arg(num(camControls->gain_db, 'f', 2)));
  } else {
    setItemText(QStringLiteral("cam"), QStringLiteral("—"));
  }
}

}  // namespace beam::ui
