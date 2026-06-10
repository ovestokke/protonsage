#include <QApplication>
#include <QCoreApplication>
#include <QStyleFactory>
#include <QPalette>
#include "ui/mainwindow.h"

int main(int argc, char* argv[]) {
    // Disable GTK platform theme to avoid CSS parsing warnings
    qputenv("QT_QPA_PLATFORMTHEME", "");
    QCoreApplication::addLibraryPath("/usr/lib/qt6/plugins");
    
    // Force Fusion style to avoid broken GTK theme parsing
    if (QStyleFactory::keys().contains("Fusion"))
        QApplication::setStyle("Fusion");
    
    QApplication app(argc, argv);
    app.setApplicationName("ProtonSage");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("ProtonSage");

    ProtonSage::MainWindow window;
    window.show();

    return app.exec();
}
