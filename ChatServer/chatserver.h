#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include<QTcpServer>
#include<QTcpSocket>
#include<QList>

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    //开启服务器，传端口号
    void startServer(quint16 port);
    //new:关闭服务器
    void closeServer();
private slots:
    //客户端连上触发
    void newClientConnect();
    //接收客户端发来的信息
    void readData();
    //客户端下线
    void clientLeave();
private:
    QTcpServer*server;  //服务器对象
    QList<QTcpSocket*> clientArr;   //存放所有客户端
};

#endif // CHATSERVER_H
