#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BeamAnalyzer"));
  QApplication::setOrganizationName(QStringLiteral("BeamAnalyzer"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

  beam::ui::MainWindow window;
  window.show();
  return app.exec();
}
