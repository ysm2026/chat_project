#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QMessageBox>
#include <QDialog>
QT_BEGIN_NAMESPACE
namespace Ui {
class ChatClient;
}
QT_END_NAMESPACE

class ChatClient : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChatClient(QWidget *parent = nullptr);
    ~ChatClient() override;
//槽函数：对应按钮点击、网络事件
private slots:
    //连接/断开按钮点击事件
    void on_connect_button_clicked();
    //发送信息按钮点击事件
    void on_send_button_clicked();
    //收到服务器消息时触发
    void readServerData();
    //成功连接服务器时触发
    void on_connect();
    //成功断开服务器时触发
    void on_disconnect();
    //网络错误时触发
    void on_socket_error();

private:
    Ui::ChatClient *ui;
    //客户端套接字，用来和服务器通信
    QTcpSocket *socket;
    //记录当前是否已连接服务器
    bool isconnect;


};
#endif // CHATCLIENT_H
