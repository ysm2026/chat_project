#ifndef LOGICAWINDOW_H
#define LOGICAWINDOW_H

#include <QWidget>
#include <QTcpSocket>
#include <QByteArray>
#include <QStringList>
#include <QTimer>

namespace Ui {
class LogicaWindow;
}

class LogicaWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LogicaWindow(QWidget *parent = nullptr);
    ~LogicaWindow();
    QString LoggedInUsername()const{return m_username;}
    QString LoggedInNickname()const{return m_nickname;}
    QTcpSocket*getSocket()const{return socket;}
    QStringList takePendingServerLines();
    void pauseLoginSocketHandlers();
    void resumeLoginSocketHandlers();

private:
    void sendLoginRequest();
    void sendPacket(const QString &line);
    void setLoginEnable(bool enabled);
    void finishLoginRequest(bool success);

    Ui::LogicaWindow *ui;
    QTcpSocket *socket;
    QByteArray m_recvBuffer;
    QTimer m_loginTimeout;
    bool m_pendingLogin;
    bool m_waitingLoginResponse;
    QString m_username;
    QString m_password;
    QString m_nickname;
    QStringList m_pendingServerLines;

signals:
    void switchRegister();
    void loginSuccess();

private slots:
    void on_logica_pushButton_clicked();
    void on_regist_pushButton_clicked();
    void on_SocketConnected();
    void readLoginData();
    void socketError();
    void onLoginTimeout();
};

#endif // LOGICAWINDOW_H
