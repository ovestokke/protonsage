#include <QApplication>
#include <QCoreApplication>
#include "ui/mainwindow.h"

int main(int argc, char* argv[]) {
    QCoreApplication::addLibraryPath("/usr/lib/qt6/plugins");
    QApplication app(argc, argv);
    app.setApplicationName("ProtonSage");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("ProtonSage");

    ProtonSage::MainWindow window;
    window.show();

    return app.exec();
}
