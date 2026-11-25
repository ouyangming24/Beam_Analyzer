#include "MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("BeamAnalyzer"));
  QApplication::setOrganizationName(QStringLiteral("BeamAnalyzer"));
  QApplication::setApplicationVersion(QStringLiteral("0.8.0"));

  QString captureDir;
  for (int i = 1; i < argc; ++i) {
    const QString a = QString::fromLocal8Bit(argv[i]);
    if (a == QLatin1String("--capture-shots") && i + 1 < argc) {
      captureDir = QString::fromLocal8Bit(argv[++i]);
    }
  }

  beam::ui::MainWindow window;
  window.show();

  if (!captureDir.isEmpty()) {
    QTimer::singleShot(300, &window, [&window, captureDir]() {
      window.captureDemoShots(captureDir);
      QTimer::singleShot(200, qApp, &QCoreApplication::quit);
    });
  }

  return app.exec();
}
