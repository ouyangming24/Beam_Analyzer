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
  background-color: #e8ecf1;
  color: #1a2332;
}
QMenuBar {
  background-color: #f4f6f9;
  border-bottom: 1px solid #c5ced9;
  padding: 2px;
}
QMenuBar::item:selected { background: #d6e2f0; }
QMenu {
  background-color: #ffffff;
  border: 1px solid #c5ced9;
}
QMenu::item:selected { background: #2f6fed; color: #fff; }
QToolBar {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f7f9fc, stop:1 #e4eaf2);
  border-bottom: 1px solid #b8c4d4;
  spacing: 4px;
  padding: 4px 6px;
}
QToolButton {
  background: transparent;
  border: 1px solid transparent;
  border-radius: 3px;
  padding: 4px 8px;
  color: #1a2332;
}
QToolButton:hover { background: #d9e4f2; border-color: #a8b8cc; }
QToolButton:checked { background: #2f6fed; color: #fff; border-color: #1f56c4; }
QStatusBar {
  background: #dfe6ee;
  border-top: 1px solid #b8c4d4;
  color: #334155;
}
QDockWidget {
  titlebar-close-icon: none;
  color: #1a2332;
}
QDockWidget::title {
  background: #d0dae8;
  padding: 5px 8px;
  border: 1px solid #b0bdcf;
  font-weight: 600;
}
QGroupBox {
  border: 1px solid #b8c4d4;
  border-radius: 4px;
  margin-top: 12px;
  padding-top: 10px;
  background: #f5f7fa;
  font-weight: 600;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 10px;
  padding: 0 4px;
  color: #2a3a50;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #ffffff;
  border: 1px solid #b0bdcf;
  border-radius: 3px;
  padding: 3px 6px;
  selection-background-color: #2f6fed;
}
QComboBox::drop-down { border: none; width: 18px; }
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #f8fafc, stop:1 #dce4ef);
  border: 1px solid #9aabbf;
  border-radius: 3px;
  padding: 4px 12px;
  min-height: 22px;
}
QPushButton:hover { background: #e8f0fb; border-color: #2f6fed; }
QPushButton:pressed { background: #c9d8ec; }
QPushButton:disabled { color: #8a97a8; background: #eceff3; }
QCheckBox, QLabel { color: #1a2332; }
QScrollArea { border: none; background: #cfd8e4; }
QSplitter::handle { background: #b8c4d4; width: 3px; height: 3px; }
QSlider::groove:horizontal {
  height: 6px; background: #c5ced9; border-radius: 3px;
}
QSlider::handle:horizontal {
  width: 14px; margin: -5px 0; border-radius: 7px;
  background: #2f6fed; border: 1px solid #1f56c4;
}
QTableWidget, QTreeWidget {
  background: #ffffff; gridline-color: #d5dde8; border: 1px solid #b0bdcf;
}
QHeaderView::section {
  background: #e4eaf2; border: 1px solid #c5ced9; padding: 4px; font-weight: 600;
}
)");
  }

  // Dark — professional lab / instrument look
  return QStringLiteral(R"(
* {
  font-family: "Segoe UI", "Microsoft YaHei UI", "Noto Sans CJK SC", "Source Han Sans SC", sans-serif;
  font-size: 12px;
}
QMainWindow, QDialog {
  background-color: #1b1f27;
  color: #e6eaf0;
}
QMenuBar {
  background-color: #232833;
  border-bottom: 1px solid #2f3645;
  padding: 2px;
  color: #d7dde8;
}
QMenuBar::item:selected { background: #3a4558; }
QMenu {
  background-color: #252b36;
  border: 1px solid #3a4558;
  color: #e6eaf0;
}
QMenu::item:selected { background: #3d7eff; color: #fff; }
QToolBar {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2a303c, stop:1 #222733);
  border-bottom: 1px solid #141820;
  spacing: 4px;
  padding: 4px 6px;
}
QToolButton {
  background: transparent;
  border: 1px solid transparent;
  border-radius: 3px;
  padding: 4px 8px;
  color: #e6eaf0;
}
QToolButton:hover { background: #3a4558; border-color: #4a5670; }
QToolButton:checked { background: #3d7eff; color: #fff; border-color: #2a62d4; }
QStatusBar {
  background: #161a22;
  border-top: 1px solid #2f3645;
  color: #a8b2c4;
}
QDockWidget { color: #e6eaf0; }
QDockWidget::title {
  background: #2a3140;
  padding: 5px 8px;
  border: 1px solid #3a4558;
  font-weight: 600;
}
QGroupBox {
  border: 1px solid #3a4558;
  border-radius: 4px;
  margin-top: 12px;
  padding-top: 10px;
  background: #222833;
  font-weight: 600;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 10px;
  padding: 0 4px;
  color: #9eb0cc;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
  background: #151922;
  border: 1px solid #3a4558;
  border-radius: 3px;
  padding: 3px 6px;
  color: #e6eaf0;
  selection-background-color: #3d7eff;
}
QComboBox QAbstractItemView {
  background: #252b36; selection-background-color: #3d7eff; color: #e6eaf0;
}
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #353c4c, stop:1 #2a303c);
  border: 1px solid #4a5670;
  border-radius: 3px;
  padding: 4px 12px;
  min-height: 22px;
  color: #e6eaf0;
}
QPushButton:hover { background: #3f4860; border-color: #3d7eff; }
QPushButton:pressed { background: #252b36; }
QPushButton:disabled { color: #6a7388; background: #1e2430; }
QCheckBox, QLabel { color: #d7dde8; }
QScrollArea { border: none; background: #12151c; }
QSplitter::handle { background: #2f3645; width: 3px; height: 3px; }
QSlider::groove:horizontal {
  height: 6px; background: #2f3645; border-radius: 3px;
}
QSlider::handle:horizontal {
  width: 14px; margin: -5px 0; border-radius: 7px;
  background: #3d7eff; border: 1px solid #2a62d4;
}
QTableWidget, QTreeWidget {
  background: #151922; gridline-color: #2f3645; border: 1px solid #3a4558;
  color: #e6eaf0;
}
QHeaderView::section {
  background: #2a3140; border: 1px solid #3a4558; padding: 4px;
  font-weight: 600; color: #c5d0e2;
}
)");
}

}  // namespace beam::ui
