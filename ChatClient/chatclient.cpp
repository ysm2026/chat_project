#include "chatclient.h"
#include "./ui_chatclient.h"
#include<QMessageBox>
#include<QDebug>

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
{
    ui->setupUi(this);
    //初始化客户端套接字
    socket=new QTcpSocket(this);
    isconnect=false;

    //绑定信号槽：将网络事件与对应函数关联起来
    connect(socket,&QTcpSocket::readyRead,this,&ChatClient::readServerData);
    connect(socket,&QTcpSocket::connected,this,&ChatClient::on_connect);
    connect(socket,&QTcpSocket::disconnected,this,&ChatClient::on_disconnect);
    connect(socket,&QTcpSocket::errorOccurred,this,&ChatClient::on_socket_error);
    //初始化界面：设置聊天框为只读，默认ip和端口，可修改
    ui->mes_textEdit->setReadOnly(true);
    ui->ip_lineEdit->setText("127.0.0.1");
    ui->port_lineEdit->setText("8888");

}

ChatClient::~ChatClient()
{
    delete ui;
}

//连接/断开按钮点击事件
void ChatClient::on_connect_button_clicked(){
    if(!isconnect){
        //1.获取用户输入的ip和端口
        QString ip=ui->ip_lineEdit->text();
        quint16 port=ui->port_lineEdit->text().toInt();
        //QMessageBox::information(this,"提示","正在连接服务器!");
        //2.主动连接服务器
        socket->connectToHost(ip,port);
    }
    else{
        socket->disconnectFromHost();
    }
}

//连接成功
void ChatClient::on_connect(){
    isconnect=true;
    ui->connect_button->setText("断开连接");
    ui->mes_textEdit->append("======成功连接服务器======");
}

//断开成功
void ChatClient::on_disconnect(){
    isconnect=false;
    ui->connect_button->setText("连接服务器");
    ui->mes_textEdit->append("======成功断开服务器======");
}

//接收服务器端信息
void ChatClient::readServerData(){
    QString msg=socket->readAll();
    ui->mes_textEdit->append("对方："+msg);
}

//发送信息
void ChatClient::on_send_button_clicked(){
    //QMessageBox::information(this,"提示","正在发送信息");
    if(!isconnect){
        QMessageBox::warning(this,"提示","请先连接服务器，发送失败!");
        return;
    }
    else{
        QString text=ui->send_lineEdit->text();
        if(text.isEmpty()){
            return;
        }
        ui->mes_textEdit->append("<font color='black'>我：</font><font color='green'>"+text+"</font>");
        ui->send_lineEdit->clear();
        socket->write(text.toUtf8());
        socket->waitForBytesWritten(1000);
    }
}

//网络错误提示
void ChatClient::on_socket_error(){
    QMessageBox::critical(this,"连接失败",socket->errorString());
}