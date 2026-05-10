#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QSqlDatabase>
#include"dbhelper.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    Dbhelper::getInstance().connectMySql();
    qDebug()<<"所有数据库驱动："<<QSqlDatabase::drivers();
    return QCoreApplication::exec();

}
