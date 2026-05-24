#include "chatclient.h"
#include "logicawindow.h"
#include <QApplication>
#include "registerwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 登录窗口
    LogicaWindow loginWin;
    // 注册窗口（默认隐藏）
    registerwindow registWin;
    // 聊天主窗口（登录成功后再显示）
    ChatClient chatWin;

    // 登录成功：关闭登录窗，用同一 TCP 连接进入聊天界面
    QObject::connect(&loginWin, &LogicaWindow::loginSuccess, [&]() {
        loginWin.pauseLoginSocketHandlers();
        chatWin.setTcpSocket(loginWin.getSocket());
        chatWin.setUserInfo(loginWin.LoggedInUsername(), loginWin.LoggedInNickname());
        loginWin.close();
        chatWin.show();
    });

    // 点击「注册」：暂停登录读 socket，显示注册窗
    QObject::connect(&loginWin, &LogicaWindow::switchRegister, [&]() {
        loginWin.pauseLoginSocketHandlers();
        registWin.setTcpSocket(loginWin.getSocket());
        registWin.resetForm();
        loginWin.hide();
        registWin.show();
    });

    auto backToLogin = [&]() {
        registWin.detachSocketHandlers();
        registWin.hide();
        loginWin.resumeLoginSocketHandlers();
        loginWin.show();
    };

    // 注册成功：关闭注册窗，回到登录窗
    QObject::connect(&registWin, &registerwindow::registerSuccess, [&]() {
        registWin.resetForm();
        backToLogin();
    });

    // 取消注册：回到登录窗
    QObject::connect(&registWin, &registerwindow::registerCancel, backToLogin);

    loginWin.show();

    return QCoreApplication::exec();
}
