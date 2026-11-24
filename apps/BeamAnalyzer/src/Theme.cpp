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
  background: #cfd8e6;
  border-top: 1px solid #aeb9ca;
  color: #334155;
  font-size: 11px;
}
QStatusBar QLabel#StatusModeChip {
  background: #1d4ed8; color: #fff; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
}
QStatusBar QLabel#StatusLiveChip {
  background: #059669; color: #fff; border-radius: 2px;
  padding: 1px 8px; font-weight: 700; font-size: 11px;
}
QDockWidget { color: #152033; }
QDockWidget::title {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #d7e0ec, stop:1 #c5d0e0);
  padding: 4px 8px;
  border: 1px solid #a8b6c9;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox {
  border: 1px solid #aeb9ca;
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
  color: #334155;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #ffffff;
  border: 1px solid #a8b6c9;
  border-radius: 2px;
  padding: 2px 5px;
  min-height: 20px;
  selection-background-color: #2563eb;
}
QComboBox::drop-down { border: none; width: 16px; }
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f8fafc, stop:1 #d7e0ec);
  border: 1px solid #92a3b8;
  border-radius: 2px;
  padding: 3px 10px;
  min-height: 20px;
}
QPushButton:hover { background: #e4eefb; border-color: #2563eb; }
QPushButton:pressed { background: #c5d4e8; }
QPushButton:disabled { color: #8a97a8; background: #eceff3; }
QPushButton#AccentButton {
  background: #2563eb; color: #fff; border-color: #1d4ed8; font-weight: 600;
}
QPushButton#AccentButton:hover { background: #1d4ed8; }
QCheckBox, QLabel { color: #152033; }
QLabel#SectionHeader {
  font-weight: 700; color: #1d4ed8; font-size: 11px;
  padding-top: 2px;
}
QLabel#MetricValue {
  font-family: "Cascadia Mono", "Consolas", "Sarasa Mono SC", monospace;
  font-size: 12px; font-weight: 600;
}
QScrollArea { border: none; background: #c8d2e0; }
QWidget#ControlColumn { background: #e8edf4; }
QWidget#ImageChrome, QWidget#PlayerBar {
  background: #d5dde8; border-bottom: 1px solid #aeb9ca;
}
QWidget#StatusStrip {
  background: #eef3f9; border-bottom: 1px solid #b7c2d1;
}
QLabel#StatusStripMode { font-weight: 700; color: #1d4ed8; font-size: 11px; }
QLabel#StatusStripHint { color: #64748b; font-size: 10px; }
QLabel#StatusValidityChip {
  border: 1px solid #94a3b8; border-radius: 2px; padding: 1px 7px;
  font-weight: 700; font-size: 10px; letter-spacing: 0.04em; color: #64748b;
  background: #e2e8f0;
}
QLabel#StatusValidityChip[validity="good"] {
  color: #166534; background: #dcfce7; border-color: #86efac;
}
QLabel#StatusValidityChip[validity="bad"] {
  color: #991b1b; background: #fee2e2; border-color: #fca5a5;
}
QLabel#ColorLegend { color: #475569; font-size: 10px; }
QSplitter::handle { background: #aeb9ca; width: 2px; height: 2px; }
QSlider::groove:horizontal {
  height: 5px; background: #b7c2d1; border-radius: 2px;
}
QSlider::handle:horizontal {
  width: 12px; margin: -4px 0; border-radius: 6px;
  background: #2563eb; border: 1px solid #1d4ed8;
}
QTableWidget, QTreeWidget {
  background: #ffffff; gridline-color: #d0d8e4; border: 1px solid #a8b6c9;
  alternate-background-color: #f3f6fa; font-size: 11px;
}
QHeaderView::section {
  background: #dde5f0; border: 1px solid #b7c2d1; padding: 3px; font-weight: 700;
  font-size: 11px;
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
)");
  }

  // Dark — denser commercial instrument workstation look
  return QStringLiteral(R"(
* {
  font-family: "Segoe UI", "Microsoft YaHei UI", "Noto Sans CJK SC", "Source Han Sans SC", sans-serif;
  font-size: 12px;
}
QMainWindow, QDialog {
  background-color: #171b22;
  color: #e6eaf0;
}
QMenuBar {
  background-color: #1e2430;
  border-bottom: 1px solid #2a3140;
  padding: 1px 2px;
  color: #c9d2e0;
}
QMenuBar::item { padding: 3px 8px; }
QMenuBar::item:selected { background: #323b4d; }
QMenu {
  background-color: #222833;
  border: 1px solid #3a4558;
  color: #e6eaf0;
}
QMenu::item:selected { background: #3b82f6; color: #fff; }
QToolBar#MainToolBar {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #262c38, stop:1 #1c212c);
  border-bottom: 1px solid #0f1218;
  spacing: 3px;
  padding: 3px 5px;
}
QToolButton {
  background: transparent;
  border: 1px solid transparent;
  border-radius: 2px;
  padding: 3px 7px;
  color: #e6eaf0;
  font-size: 12px;
}
QToolButton:hover { background: #323b4d; border-color: #455064; }
QToolButton:checked { background: #3b82f6; color: #fff; border-color: #2563eb; }
QToolButton#PrimaryAction {
  background: #3b82f6; color: #fff; border-color: #2563eb; font-weight: 600;
}
QStatusBar {
  background: #12161e;
  border-top: 1px solid #2a3140;
  color: #9aa6ba;
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
QDockWidget { color: #e6eaf0; }
QDockWidget::title {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2a3140, stop:1 #232936);
  padding: 4px 8px;
  border: 1px solid #3a4558;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox {
  border: 1px solid #343c4d;
  border-radius: 2px;
  margin-top: 10px;
  padding: 8px 6px 6px 6px;
  background: #1c222d;
  font-weight: 700;
  font-size: 11px;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 8px;
  padding: 0 3px;
  color: #8fa3c2;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #12161e;
  border: 1px solid #3a4558;
  border-radius: 2px;
  padding: 2px 5px;
  min-height: 20px;
  color: #e6eaf0;
  selection-background-color: #3b82f6;
}
QComboBox QAbstractItemView {
  background: #222833; selection-background-color: #3b82f6; color: #e6eaf0;
}
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #323b4d, stop:1 #262c38);
  border: 1px solid #455064;
  border-radius: 2px;
  padding: 3px 10px;
  min-height: 20px;
  color: #e6eaf0;
}
QPushButton:hover { background: #3a4558; border-color: #3b82f6; }
QPushButton:pressed { background: #1c222d; }
QPushButton:disabled { color: #5c667a; background: #1a1f28; }
QPushButton#AccentButton {
  background: #3b82f6; color: #fff; border-color: #2563eb; font-weight: 600;
}
QPushButton#AccentButton:hover { background: #2563eb; }
QCheckBox, QLabel { color: #d0d7e4; }
QLabel#SectionHeader {
  font-weight: 700; color: #7aa2ff; font-size: 11px;
  padding-top: 2px;
}
QLabel#MetricValue {
  font-family: "Cascadia Mono", "Consolas", "Sarasa Mono SC", monospace;
  font-size: 12px; font-weight: 600; color: #eef2f8;
}
QScrollArea { border: none; background: #10141b; }
QWidget#ControlColumn { background: #1a1f28; }
QWidget#ImageChrome, QWidget#PlayerBar {
  background: #1a1f28; border-bottom: 1px solid #2a3140;
}
QWidget#StatusStrip {
  background: #141922; border-bottom: 1px solid #2a3140;
}
QLabel#StatusStripMode { font-weight: 700; color: #7aa2ff; font-size: 11px; }
QLabel#StatusStripHint { color: #8b97ab; font-size: 10px; }
QLabel#StatusValidityChip {
  border: 1px solid #3a4558; border-radius: 2px; padding: 1px 7px;
  font-weight: 700; font-size: 10px; letter-spacing: 0.04em; color: #9aa6ba;
  background: #222833;
}
QLabel#StatusValidityChip[validity="good"] {
  color: #86efac; background: #14532d; border-color: #166534;
}
QLabel#StatusValidityChip[validity="bad"] {
  color: #fca5a5; background: #7f1d1d; border-color: #991b1b;
}
QLabel#ColorLegend { color: #8b97ab; font-size: 10px; }
QSplitter::handle { background: #2a3140; width: 2px; height: 2px; }
QSlider::groove:horizontal {
  height: 5px; background: #2a3140; border-radius: 2px;
}
QSlider::handle:horizontal {
  width: 12px; margin: -4px 0; border-radius: 6px;
  background: #3b82f6; border: 1px solid #2563eb;
}
QTableWidget, QTreeWidget {
  background: #12161e; gridline-color: #2a3140; border: 1px solid #3a4558;
  color: #e6eaf0; alternate-background-color: #1a1f28; font-size: 11px;
}
QHeaderView::section {
  background: #262c38; border: 1px solid #3a4558; padding: 3px;
  font-weight: 700; color: #b8c4d8; font-size: 11px;
}
QTabWidget::pane {
  border: 1px solid #343c4d; background: #1c222d; top: -1px;
}
QTabBar::tab {
  background: #222833; border: 1px solid #343c4d; padding: 4px 12px;
  margin-right: 1px; font-size: 11px; font-weight: 600; color: #9aa6ba;
}
QTabBar::tab:selected { background: #1c222d; border-bottom-color: #1c222d; color: #7aa2ff; }
QWidget#ProfilePlot { background: #12161e; }
)");
}

}  // namespace beam::ui
