#include<QCoreApplication>
#include"chatserver.h"
#include"dbhelper.h"
#include<QDebug>

#include "dbconfig.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    //初始化连接数据库
    if (!Dbhelper::getInstance().connectMySql()) {
        qDebug() << "服务启动失败：数据库未连接";
        return 1;
    }
    ChatServer ser;
    if (!ser.startServer(DbConfig::kChatPort)) {
        qDebug() << "服务启动失败：端口监听失败";
        return 1;
    }

    // Set up code that uses the Qt event loop here.
    // Call QCoreApplication::quit() or QCoreApplication::exit() to quit the application.
    // A not very useful example would be including
    // #include <QTimer>
    // near the top of the file and calling
    // QTimer::singleShot(5000, &a, &QCoreApplication::quit);
    // which quits the application after 5 seconds.

    // If you do not need a running Qt event loop, remove the call
    // to QCoreApplication::exec() or use the Non-Qt Plain C++ Application template.

    return QCoreApplication::exec();
}
