#include "chatserver.h"
#include<QDebug>
#include<QHostAddress>
#include"dbhelper.h"

ChatServer::ChatServer(QObject *parent)
    : QObject{parent}
{
    //初始化服务器
    server=new QTcpServer(this);
    //监听新客户端连接
    connect(server,&QTcpServer::newConnection,this,&ChatServer::newClientConnect);
}
//启动服务器
void ChatServer::startServer(quint16 port){
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
void ChatServer::newClientConnect(){
    //获取刚连接的客户端套接字
    QTcpSocket*client=server->nextPendingConnection();
    clientArr.append(client);
    qDebug()<<"新客户端上线:"<<client->peerAddress().toString();
    //绑定读取数据信号
    connect(client,&QTcpSocket::readyRead,this,&ChatServer::readData);
    //绑定信号槽，客户端断开连接时，触发clientleave
    connect(client,&QTcpSocket::disconnected,this,&ChatServer::clientLeave);
}
//读取客户端信息
//new:直接发送并判断状态、无中文编码处理、广播无发送人标识、无空指针判断
void ChatServer::readData(){
    //拿到发送数据的客户端
    QTcpSocket*cli=qobject_cast<QTcpSocket*>(sender());
    //空指针判断
    if(!cli){
        return;
    }
    //统一读取UTF-8
    //QString msg=cli->readAll();
    QByteArray buf=cli->readAll();
    QString msg=QString::fromUtf8(buf);

    //获取客户端ip标识是谁发送的消息
    QString ip=cli->peerAddress().toString();
    QString sendMsg=QString("[%1]:%2").arg(ip,msg);

    //遍历广播
    for(const auto&sock:std::as_const(clientArr)){
        if(sock!=cli&&sock!=nullptr&&
            sock->state()==QAbstractSocket::ConnectedState){
            sock->write(sendMsg.toUtf8());
            sock->flush();
        }
    }

    qDebug()<<"收到消息:"<<sendMsg;
}
//客户端离开
void ChatServer::clientLeave(){
    QTcpSocket*cli=qobject_cast<QTcpSocket*>(sender());
    if(!cli){
        return;
    }
    qDebug()<<"客户端下线"<<cli->peerAddress().toString();
    clientArr.removeOne(cli);
    cli->deleteLater();
}
//new:关闭服务器
void ChatServer::closeServer(){
    if(server->isListening()){
        server->close();
    }
    QList<QTcpSocket*>clients=clientArr;
    for(const auto& sock:std::as_const(clients)){
        sock->disconnectFromHost();
        sock->deleteLater();
    }
    clientArr.clear();
    qDebug()<<"服务器已关闭";
}
