#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("SmartFEM");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SmartFEM");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}

