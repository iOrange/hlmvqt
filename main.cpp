#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // this is for QSettings
    a.setOrganizationName("iOrange");
    a.setApplicationName("hlmvqt");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "hlmvqt_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 0);
    format.setProfile(QSurfaceFormat::NoProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    MainWindow w;
    w.show();

    if (argc > 1) {
        fs::path mdlPath = argv[1];
        if (mdlPath.extension() == ".mdl") {
            w.OpenModel(mdlPath);
        }
    }

    return a.exec();
}
