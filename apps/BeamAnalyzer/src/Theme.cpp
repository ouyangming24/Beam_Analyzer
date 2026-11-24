#include "Theme.hpp"

namespace beam::ui {

const char* themeName(AppTheme theme) {
  return theme == AppTheme::Dark ? "dark" : "light";
}

QString themeStyleSheet(AppTheme theme) {
  if (theme == AppTheme::Light) {
    return QStringLiteral(R"(
* {
  font-family: "Segoe UI", "Microsoft YaHei UI", "Noto Sans CJK SC", "Source Han Sans SC", sans-serif;
  font-size: 12px;
}
QMainWindow, QDialog {
  background-color: #dfe5ee;
  color: #152033;
}
QMenuBar {
  background-color: #eef2f7;
  border-bottom: 1px solid #b7c2d1;
  padding: 1px 2px;
  font-size: 12px;
}
QMenuBar::item { padding: 3px 8px; }
QMenuBar::item:selected { background: #cfdced; }
QMenu {
  background-color: #ffffff;
  border: 1px solid #b7c2d1;
}
QMenu::item:selected { background: #2563eb; color: #fff; }
QToolBar#MainToolBar {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f4f7fb, stop:1 #dde5f0);
  border-bottom: 1px solid #aeb9ca;
  spacing: 3px;
  padding: 3px 5px;
}
QToolButton {
  background: transparent;
  border: 1px solid transparent;
  border-radius: 2px;
  padding: 3px 7px;
  color: #152033;
  font-size: 12px;
}
QToolButton:hover { background: #d3e0f2; border-color: #9eb0c8; }
QToolButton:checked { background: #2563eb; color: #fff; border-color: #1d4ed8; }
QToolButton#PrimaryAction {
  background: #2563eb; color: #fff; border-color: #1d4ed8; font-weight: 600;
}
QStatusBar {
  background: #c9d3e0;
  border-top: 1px solid #aeb9ca;
  color: #334155;
  font-size: 11px;
}
QStatusBar QLabel#StatusModeChip {
  background: #2563eb; color: #fff; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
}
QStatusBar QLabel#StatusLiveChip {
  background: #059669; color: #fff; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
}
QDockWidget { color: #152033; }
QDockWidget::title {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #e8eef6, stop:1 #d5dee9);
  padding: 4px 8px;
  border: 1px solid #aeb9ca;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox {
  border: 1px solid #b7c2d1;
  border-radius: 2px;
  margin-top: 10px;
  padding: 8px 6px 6px 6px;
  background: #f3f6fa;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 8px;
  padding: 0 3px;
  color: #3b5b8c;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #ffffff;
  border: 1px solid #aeb9ca;
  border-radius: 2px;
  padding: 2px 5px;
  min-height: 20px;
  color: #152033;
  selection-background-color: #2563eb;
}
QComboBox QAbstractItemView {
  background: #ffffff; selection-background-color: #2563eb; color: #152033;
}
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f4f7fb, stop:1 #dde5f0);
  border: 1px solid #aeb9ca;
  border-radius: 2px;
  padding: 3px 10px;
  min-height: 20px;
  color: #152033;
}
QPushButton:hover { background: #d3e0f2; border-color: #2563eb; }
QPushButton:pressed { background: #c5d2e6; }
QPushButton:disabled { color: #94a3b8; background: #e8edf4; }
QPushButton#AccentButton {
  background: #2563eb; color: #fff; border-color: #1d4ed8; font-weight: 600;
}
QPushButton#AccentButton:hover { background: #1d4ed8; }
QCheckBox, QLabel { color: #1e293b; }
QLabel#SectionHeader {
  font-weight: 700; color: #1d4ed8; font-size: 11px;
  padding-top: 2px;
}
QLabel#MetricValue {
  font-family: "Cascadia Mono", "Consolas", "Sarasa Mono SC", monospace;
  font-size: 12px; font-weight: 600; color: #0f172a;
}
QScrollArea { border: none; background: #d5dde8; }
QWidget#ControlColumn { background: #e8eef6; }
QWidget#ImageChrome, QWidget#PlayerBar {
  background: #e8eef6; border-bottom: 1px solid #b7c2d1;
}
QWidget#StatusStrip {
  background: #e2e8f0; border-bottom: 1px solid #b7c2d1;
}
QLabel#StatusStripMode { font-weight: 700; color: #1d4ed8; font-size: 11px; }
QLabel#StatusStripHint { color: #64748b; font-size: 10px; }
QLabel#StatusValidityChip {
  border: 1px solid #aeb9ca; border-radius: 2px; padding: 1px 7px;
  font-weight: 700; font-size: 10px; letter-spacing: 0.04em; color: #475569;
  background: #f8fafc;
}
QLabel#StatusValidityChip[validity="good"] {
  color: #166534; background: #dcfce7; border-color: #86efac;
}
QLabel#StatusValidityChip[validity="bad"] {
  color: #991b1b; background: #fee2e2; border-color: #fca5a5;
}
QLabel#ColorLegend { color: #64748b; font-size: 10px; }
QSplitter::handle { background: #b7c2d1; width: 2px; height: 2px; }
QSlider::groove:horizontal {
  height: 5px; background: #c5d0de; border-radius: 2px;
}
QSlider::handle:horizontal {
  width: 12px; margin: -4px 0; border-radius: 6px;
  background: #2563eb; border: 1px solid #1d4ed8;
}
QTableWidget, QTreeWidget {
  background: #ffffff; gridline-color: #d5dee9; border: 1px solid #aeb9ca;
  color: #152033; alternate-background-color: #f3f6fa; font-size: 11px;
}
QHeaderView::section {
  background: #e2e8f0; border: 1px solid #aeb9ca; padding: 3px;
  font-weight: 700; color: #334155; font-size: 11px;
}
QTabWidget::pane {
  border: 1px solid #aeb9ca; background: #f3f6fa; top: -1px;
}
QTabBar::tab {
  background: #d7e0ec; border: 1px solid #aeb9ca; padding: 4px 12px;
  margin-right: 1px; font-size: 11px; font-weight: 600;
}
QTabBar::tab:selected { background: #ffffff; border-bottom-color: #ffffff; color: #1d4ed8; }
QWidget#ProfilePlot { background: #f8fafc; }
QWidget#IntensitySurface3D { background: #e8edf4; }
QLabel#BigMetric {
  font-family: "Cascadia Mono", "Consolas", monospace;
  font-size: 28px; font-weight: 700; color: #0f172a;
}
QLabel#BigMetricCaption { font-size: 11px; color: #64748b; font-weight: 600; }
)");
  }

  // Dark — tech / commercial instrument aesthetic (cyan accent, deep chassis)
  return QStringLiteral(R"(
* {
  font-family: "Segoe UI", "Microsoft YaHei UI", "Noto Sans CJK SC", "Source Han Sans SC", sans-serif;
  font-size: 12px;
}
QMainWindow, QDialog {
  background-color: #0a0e14;
  color: #d7e2ef;
}
QMenuBar {
  background-color: #0d1219;
  border-bottom: 1px solid #1a2433;
  padding: 1px 2px;
  color: #9fb4cc;
}
QMenuBar::item { padding: 3px 8px; }
QMenuBar::item:selected { background: #152033; }
QMenu {
  background-color: #101820;
  border: 1px solid #243044;
  color: #d7e2ef;
}
QMenu::item:selected { background: #0891b2; color: #041016; }
QToolBar#MainToolBar {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #121a24, stop:1 #0b1018);
  border-bottom: 1px solid #061018;
  spacing: 3px;
  padding: 3px 5px;
}
QToolButton {
  background: transparent;
  border: 1px solid transparent;
  border-radius: 2px;
  padding: 3px 7px;
  color: #d7e2ef;
  font-size: 12px;
}
QToolButton:hover { background: #152033; border-color: #2a3d55; }
QToolButton:checked { background: #0e7490; color: #e0f7fa; border-color: #22d3ee; }
QToolButton#PrimaryAction {
  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0e7490, stop:1 #0369a1);
  color: #e0f7fa; border-color: #22d3ee; font-weight: 600;
}
QStatusBar {
  background: #06090e;
  border-top: 1px solid #1a2433;
  color: #7f95ad;
  font-size: 11px;
}
QStatusBar QLabel#StatusModeChip {
  background: #0e7490; color: #e0f7fa; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
  border: 1px solid #22d3ee;
}
QStatusBar QLabel#StatusLiveChip {
  background: #047857; color: #d1fae5; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
  border: 1px solid #34d399;
}
QDockWidget { color: #d7e2ef; }
QDockWidget::title {
  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #13202e, stop:1 #0e1620);
  padding: 4px 8px;
  border: 1px solid #1e3048;
  border-left: 3px solid #22d3ee;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox {
  border: 1px solid #1e3048;
  border-radius: 2px;
  margin-top: 10px;
  padding: 8px 6px 6px 6px;
  background: #0e1520;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 8px;
  padding: 0 3px;
  color: #67e8f9;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #070b11;
  border: 1px solid #243044;
  border-radius: 2px;
  padding: 2px 5px;
  min-height: 20px;
  color: #e2eaf4;
  selection-background-color: #0e7490;
}
QComboBox QAbstractItemView {
  background: #101820; selection-background-color: #0e7490; color: #e2eaf4;
}
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #172433, stop:1 #101820);
  border: 1px solid #2a3d55;
  border-radius: 2px;
  padding: 3px 10px;
  min-height: 20px;
  color: #d7e2ef;
}
QPushButton:hover { background: #1a2a3d; border-color: #22d3ee; }
QPushButton:pressed { background: #0a1018; }
QPushButton:disabled { color: #4a5d73; background: #0c121a; }
QPushButton#AccentButton {
  background: #0e7490; color: #e0f7fa; border-color: #22d3ee; font-weight: 600;
}
QPushButton#AccentButton:hover { background: #0891b2; }
QCheckBox, QLabel { color: #c5d4e4; }
QLabel#SectionHeader {
  font-weight: 700; color: #67e8f9; font-size: 11px;
  padding-top: 2px;
  letter-spacing: 0.02em;
}
QLabel#MetricValue {
  font-family: "Cascadia Mono", "Consolas", "Sarasa Mono SC", monospace;
  font-size: 12px; font-weight: 600; color: #f0f9ff;
}
QScrollArea { border: none; background: #070b11; }
QWidget#ControlColumn { background: #0c121a; }
QWidget#ImageChrome, QWidget#PlayerBar {
  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0e1620, stop:1 #0a1018);
  border-bottom: 1px solid #1a2433;
}
QWidget#StatusStrip {
  background: #080c12;
  border-bottom: 1px solid #1a2433;
}
QLabel#StatusStripMode { font-weight: 700; color: #67e8f9; font-size: 11px; }
QLabel#StatusStripHint { color: #6b8299; font-size: 10px; }
QLabel#StatusValidityChip {
  border: 1px solid #243044; border-radius: 2px; padding: 1px 7px;
  font-weight: 700; font-size: 10px; letter-spacing: 0.04em; color: #7f95ad;
  background: #101820;
}
QLabel#StatusValidityChip[validity="good"] {
  color: #6ee7b7; background: #064e3b; border-color: #059669;
}
QLabel#StatusValidityChip[validity="bad"] {
  color: #fca5a5; background: #7f1d1d; border-color: #991b1b;
}
QLabel#ColorLegend { color: #6b8299; font-size: 10px; }
QSplitter::handle { background: #1a2433; width: 2px; height: 2px; }
QSlider::groove:horizontal {
  height: 5px; background: #1a2433; border-radius: 2px;
}
QSlider::handle:horizontal {
  width: 12px; margin: -4px 0; border-radius: 6px;
  background: #22d3ee; border: 1px solid #0891b2;
}
QTableWidget, QTreeWidget {
  background: #070b11; gridline-color: #1a2433; border: 1px solid #243044;
  color: #d7e2ef; alternate-background-color: #0c121a; font-size: 11px;
}
QHeaderView::section {
  background: #121a24; border: 1px solid #243044; padding: 3px;
  font-weight: 700; color: #9fb4cc; font-size: 11px;
}
QTabWidget::pane {
  border: 1px solid #1e3048; background: #0e1520; top: -1px;
}
QTabBar::tab {
  background: #101820; border: 1px solid #1e3048; padding: 4px 12px;
  margin-right: 1px; font-size: 11px; font-weight: 600; color: #7f95ad;
}
QTabBar::tab:selected {
  background: #0e1520; border-bottom-color: #0e1520; color: #67e8f9;
  border-top: 2px solid #22d3ee;
}
QWidget#ProfilePlot { background: #070b11; }
QWidget#IntensitySurface3D { background: #070b11; }
QLabel#BigMetric {
  font-family: "Cascadia Mono", "Consolas", monospace;
  font-size: 28px; font-weight: 700; color: #67e8f9;
}
QLabel#BigMetricCaption { font-size: 11px; color: #6b8299; font-weight: 600; }
)");
}

}  // namespace beam::ui
