#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QWidget>
#include <QTcpSocket>

namespace Ui {
class registerwindow;
}

class registerwindow : public QWidget
{
    Q_OBJECT

public:
    explicit registerwindow(QWidget *parent = nullptr);
    ~registerwindow();

    // 使用与登录窗口相同的 TCP 连接（由主窗口传入）
    void setTcpSocket(QTcpSocket *socket);
    // 打开注册界面前清空输入与提示
    void resetForm();
    // 离开注册页时断开本窗口对 socket 的监听
    void detachSocketHandlers();

signals:
    // 注册成功，关闭注册窗并回到登录窗
    void registerSuccess();
    // 用户点击取消，回到登录窗
    void registerCancel();

private slots:
    // 点击「确认」：校验密码后发送注册请求
    void on_true_pushButton_clicked();
    // 点击「取消」
    void on_casual_pushButton_clicked();
    // TCP 连接成功后再发送注册包
    void onSocketConnected();
    // 读取服务器注册结果
    void readRegisterData();
    // 网络错误
    void onSocketError();

private:
    void sendRegisterRequest();
    void setRegisterEnable(bool enabled);
    void showError(const QString &msg);
    void clearError();

    Ui::registerwindow *ui;
    QTcpSocket *m_socket;       // 与服务器通信的套接字（可与登录窗共用）
    bool m_pendingRegister;     // 是否正在等待 TCP 连接成功后再发注册请求
    bool m_waitingRegisterResponse; // 是否已发出注册请求、等待服务器 REGISTER_* 响应
    QString m_username;
    QString m_password;
    QString m_nickname;
};

#endif // REGISTERWINDOW_H
