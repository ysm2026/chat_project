#include "chatclient.h"
#include "logicawindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //ChatClient w;
    //w.show();

    //创建登录窗口
    LogicaWindow loginWin;
    //创建聊天主窗口，设为不显示，登录成功再显示
    ChatClient chatWin;

    //绑定：登录成功，关闭登录窗，打开聊天窗
    QObject::connect(&loginWin,&LogicaWindow::loginSuccess,[&](){
        chatWin.setTcpSocket(loginWin.getSocket());
        chatWin.setUserInfo(loginWin.LoggedInUsername(),loginWin.LoggedInNickname());
        loginWin.close();
        chatWin.show();
    });
    loginWin.show();
    return QCoreApplication::exec();
}
