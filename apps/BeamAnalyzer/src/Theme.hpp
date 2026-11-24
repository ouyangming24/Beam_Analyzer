#pragma once

#include <QString>

namespace beam::ui {

enum class AppTheme {
  Dark,
  Light,
};

[[nodiscard]] QString themeStyleSheet(AppTheme theme);
[[nodiscard]] const char* themeName(AppTheme theme);

}  // namespace beam::ui
