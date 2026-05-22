#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("local");
    QApplication::setApplicationName("ssh-tool");
    QApplication::setWindowIcon(QIcon(":/ssh-tool.png"));
    MainWindow w;
    w.show();
    return app.exec();
}
