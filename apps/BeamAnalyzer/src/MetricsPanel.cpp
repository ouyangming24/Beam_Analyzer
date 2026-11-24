#include "MetricsPanel.hpp"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

namespace beam::ui {
namespace {

QString num(double v, char fmt = 'f', int prec = 3) {
  if (!std::isfinite(v)) {
    return QStringLiteral("—");
  }
  return QString::number(v, fmt, prec);
}

QLabel* sectionHeader(const QString& text) {
  auto* t = new QLabel(text);
  t->setObjectName(QStringLiteral("SectionHeader"));
  return t;
}

}  // namespace

MetricsPanel::MetricsPanel(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  setObjectName(QStringLiteral("MetricsPanel"));
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(4);

  auto* form = new QFormLayout;
  form->setLabelAlignment(Qt::AlignRight);
  form->setFormAlignment(Qt::AlignTop);
  form->setHorizontalSpacing(8);
  form->setVerticalSpacing(2);
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  sourceLabel_ = addRow(form, QStringLiteral("来源"));
  liveLabel_ = addRow(form, QStringLiteral("状态"));

  auto* sep1 = new QFrame;
  sep1->setFrameShape(QFrame::HLine);
  root->addLayout(form);
  root->addWidget(sep1);

  root->addWidget(sectionHeader(QStringLiteral("ISO 11146")));
  auto* iso = new QFormLayout;
  iso->setLabelAlignment(Qt::AlignRight);
  iso->setHorizontalSpacing(8);
  iso->setVerticalSpacing(2);

  totalPower_ = addRow(iso, QStringLiteral("总功率"));
  centroidX_ = addRow(iso, QStringLiteral("质心 X"));
  centroidY_ = addRow(iso, QStringLiteral("质心 Y"));
  dx_ = addRow(iso, QStringLiteral("Dx (4σ)"));
  dy_ = addRow(iso, QStringLiteral("Dy (4σ)"));
  dMajor_ = addRow(iso, QStringLiteral("D major"));
  dMinor_ = addRow(iso, QStringLiteral("D minor"));
  orientation_ = addRow(iso, QStringLiteral("方位角"));
  ellipticity_ = addRow(iso, QStringLiteral("椭圆度 ε"));
  root->addLayout(iso);

  auto* sep2 = new QFrame;
  sep2->setFrameShape(QFrame::HLine);
  root->addWidget(sep2);

  root->addWidget(sectionHeader(QStringLiteral("功率 / 能量")));
  auto* pow = new QFormLayout;
  pow->setLabelAlignment(Qt::AlignRight);
  pow->setHorizontalSpacing(8);
  pow->setVerticalSpacing(2);
  sumCounts_ = addRow(pow, QStringLiteral("Σ counts"));
  peakCounts_ = addRow(pow, QStringLiteral("峰值"));
  meanCounts_ = addRow(pow, QStringLiteral("均值"));
  powerW_ = addRow(pow, QStringLiteral("功率"));
  energyJ_ = addRow(pow, QStringLiteral("能量"));
  root->addLayout(pow);

  auto* sep3 = new QFrame;
  sep3->setFrameShape(QFrame::HLine);
  root->addWidget(sep3);

  root->addWidget(sectionHeader(QStringLiteral("测量上下文")));
  auto* ctx = new QFormLayout;
  ctx->setLabelAlignment(Qt::AlignRight);
  ctx->setHorizontalSpacing(8);
  ctx->setVerticalSpacing(2);
  roiLabel_ = addRow(ctx, QStringLiteral("ROI"));
  crossLabel_ = addRow(ctx, QStringLiteral("十字线"));
  pixelSize_ = addRow(ctx, QStringLiteral("像素尺寸"));
  camLabel_ = addRow(ctx, QStringLiteral("相机"));
  root->addLayout(ctx);
  root->addStretch(1);

  clear();
}

QLabel* MetricsPanel::addRow(QFormLayout* form, const QString& name) {
  auto* value = new QLabel(QStringLiteral("—"));
  value->setObjectName(QStringLiteral("MetricValue"));
  value->setTextInteractionFlags(Qt::TextSelectableByMouse);
  value->setWordWrap(true);
  value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  form->addRow(name, value);
  return value;
}

void MetricsPanel::setValue(QLabel* label, const QString& text, const QString& unit) {
  if (!label) {
    return;
  }
  if (unit.isEmpty()) {
    label->setText(text);
  } else {
    label->setText(text + QChar(' ') + unit);
  }
}

void MetricsPanel::clear() {
  setSource(QStringLiteral("尚无数据"));
  setLive(false);
  for (QLabel* l : {totalPower_, centroidX_, centroidY_, dx_, dy_, dMajor_, dMinor_,
                    orientation_, ellipticity_, sumCounts_, peakCounts_, meanCounts_,
                    powerW_, energyJ_, roiLabel_, crossLabel_, pixelSize_, camLabel_}) {
    if (l) {
      l->setText(QStringLiteral("—"));
    }
  }
}

void MetricsPanel::setSource(const QString& source) {
  sourceLabel_->setText(source);
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

void MetricsPanel::updateMetrics(const beam::core::Iso11146Metrics& metrics,
                                 const std::optional<beam::core::PowerMetrics>& power,
                                 const beam::core::BeamFrame* frame,
                                 const std::optional<beam::core::RectRoi>& roi,
                                 bool roiEnabled,
                                 int crossX,
                                 int crossY,
                                 const beam::camera::CameraControls* camControls) {
  if (!metrics.valid) {
    setValue(totalPower_, QStringLiteral("无效"));
    return;
  }

  setValue(totalPower_, num(metrics.totalPower, 'g', 6), QStringLiteral("a.u."));
  setValue(centroidX_, num(metrics.centroidX_um), QStringLiteral("µm"));
  setValue(centroidY_, num(metrics.centroidY_um), QStringLiteral("µm"));
  setValue(dx_, num(metrics.diameterX_um), QStringLiteral("µm"));
  setValue(dy_, num(metrics.diameterY_um), QStringLiteral("µm"));
  setValue(dMajor_, num(metrics.diameterMajor_um), QStringLiteral("µm"));
  setValue(dMinor_, num(metrics.diameterMinor_um), QStringLiteral("µm"));
  const double oriDeg = metrics.orientation_rad * 180.0 / 3.14159265358979323846;
  setValue(orientation_, num(oriDeg), QStringLiteral("°"));
  setValue(ellipticity_, num(metrics.ellipticity, 'f', 4));

  if (power && power->valid) {
    setValue(sumCounts_, num(power->sumCounts, 'g', 6));
    setValue(peakCounts_, num(power->peakCounts, 'g', 6));
    setValue(meanCounts_, num(power->meanCounts, 'g', 6));
    if (power->calibrated) {
      setValue(powerW_, num(power->power_W, 'g', 6), QStringLiteral("W"));
      setValue(energyJ_, num(power->energy_J, 'g', 6), QStringLiteral("J"));
    } else {
      setValue(powerW_, QStringLiteral("未标定"));
      setValue(energyJ_, QStringLiteral("未标定"));
    }
  }

  if (roi && roiEnabled) {
    setValue(roiLabel_,
             QStringLiteral("[%1,%2]–[%3,%4]")
                 .arg(roi->x0)
                 .arg(roi->y0)
                 .arg(roi->x1)
                 .arg(roi->y1));
  } else {
    setValue(roiLabel_, QStringLiteral("关闭"));
  }

  if (crossX >= 0) {
    setValue(crossLabel_, QStringLiteral("(%1, %2) px").arg(crossX).arg(crossY));
  }

  if (frame) {
    setValue(pixelSize_,
             QStringLiteral("%1 × %2 µm")
                 .arg(num(frame->pixelSizeX_um))
                 .arg(num(frame->pixelSizeY_um)));
  }

  if (camControls) {
    setValue(camLabel_,
             QStringLiteral("%1 | %2 µs | %3 dB")
                 .arg(QString::fromUtf8(beam::camera::toString(camControls->trigger)))
                 .arg(num(camControls->exposure_us, 'f', 1))
                 .arg(num(camControls->gain_db, 'f', 2)));
  } else {
    setValue(camLabel_, QStringLiteral("—"));
  }
}

}  // namespace beam::ui
