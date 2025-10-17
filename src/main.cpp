#include "mainwindow.h"
#include <QApplication>
#include <QTime>

int main(int argc, char *argv[])
{
    // 设置随机数种子
    qsrand(static_cast<uint>(QTime::currentTime().msec()));
    
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
