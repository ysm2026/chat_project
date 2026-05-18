#ifndef LOGICAWINDOW_H
#define LOGICAWINDOW_H

#include <QWidget>
#include <QTcpSocket>

namespace Ui {
class LogicaWindow;
}

class LogicaWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LogicaWindow(QWidget *parent = nullptr);
    ~LogicaWindow();
    //用户名获取接口，提供登录后的用户名和昵称
    QString LoggedInUsername()const{return m_username;}
    QString LoggedInNickname()const{return m_nickname;}
    QTcpSocket*getSocket()const{return socket;}

private:
    void sendLoginRequest();
    void setLoginEnable(bool enabled);

    Ui::LogicaWindow *ui;
    QTcpSocket *socket;
    bool m_pendingLogin;
    QString m_username;
    QString m_password;
    QString m_nickname;

signals:
    //切换到注册窗口
    void switchRegister();
    //登录成功信号
    void loginSuccess();

//私有槽函数
private slots:
    // 注册、登录按钮槽函数
    void on_logica_pushButton_clicked();
    void on_regist_pushButton_clicked();
    void on_SocketConnected();
    // 接收服务器返回数据
    void readLoginData();
    // 网络错误
    void socketError();
};

#endif // LOGICAWINDOW_H
