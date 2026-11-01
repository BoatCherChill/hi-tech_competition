#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/_runtime");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}