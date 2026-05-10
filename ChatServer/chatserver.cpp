#include "chatserver.h"
#include<QDebug>
#include<QHostAddress>

chatserver::chatserver(QObject *parent)
    : QObject{parent}
{
    //初始化服务器
    server=new QTcpServer(this);
    //监听新客户端连接
    connect(server,&QTcpServer::newConnection,this,&chatserver::newClientConnect);
}
    //启动服务器
void chatserver::startServer(quint16 port){
    //监听本机所有ip和端口
    bool ok=server->listen(QHostAddress::Any,port);
    if(ok){
        qDebug()<<"服务器启动成功，端口："<<port;
    }
    else{
        qDebug()<<"服务器启动失败";
    }
}
    //有新客户端连接
void chatserver::newClientConnect(){
    //获取刚连接的客户端套接字
    QTcpSocket*client=server->nextPendingConnection();
    clientArr.append(client);
    qDebug()<<"新客户端上线:"<<client->peerAddress().toString();
    //绑定读取数据信号
    connect(client,&QTcpSocket::readyRead,this,&chatserver::clientLeave);
}
//读取客户端信息
void chatserver::readData(){
    //拿到发送数据的客户端
    QTcpSocket*cli=qobject_cast<QTcpSocket*>(sender());
    QString msg=cli->readAll();
    qDebug()<<"收到消息:"<<msg;
}
//客户端离开
void chatserver::clientLeave(){
    QTcpSocket*cli=qobject_cast<QTcpSocket*>(sender());
    qDebug()<<"客户端下线";
    clientArr.removeOne(cli);
    cli->deleteLater();
}
