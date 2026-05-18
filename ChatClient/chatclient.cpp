#include "chatclient.h"
#include "./ui_chatclient.h"
#include<QMessageBox>
#include<QDebug>

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
    , m_socket(nullptr)
{
    ui->setupUi(this);
    //初始化界面：设置聊天框为只读
    ui->mes_textEdit->setReadOnly(true);
}

ChatClient::~ChatClient()
{
    delete ui;
}

void ChatClient::setUserInfo(const QString&username,const QString&nickname){
    m_username=username;
    m_nickname=nickname;
    setWindowTitle(tr("聊天室-%1").arg(m_nickname));
}

void ChatClient::setTcpSocket(QTcpSocket*socket){
    if(m_socket){
        disconnect(m_socket,nullptr);
    }
    m_socket=socket;
    if(m_socket){
        connect(m_socket,&QTcpSocket::readyRead,this,&ChatClient::readServerData);
        connect(m_socket,&QTcpSocket::disconnected,this,&ChatClient::on_disconnect);
        ui->mes_textEdit->append(tr("===== 欢迎%1，已连接服务器 =====").arg(m_nickname));
    }
}

//发送信息
void ChatClient::on_send_button_clicked(){
    //QMessageBox::information(this,"提示","正在发送信息");
    if(!m_socket||m_socket->state()!=QAbstractSocket::ConnectedState){
        QMessageBox::warning(this,tr("提示"),tr("未连接到服务器，请重新连接"));
        return;
    }
    else{
        QString text=ui->send_lineEdit->text();
        if(text.isEmpty()){
            return;
        }
        //显示自己发送信息
        ui->mes_textEdit->append("<font color='black'>我：</font><font color='green'>"+text+"</font>");
        ui->send_lineEdit->clear();
        //加换行符作为发送符号
        m_socket->write((text+"\n").toUtf8());
        m_socket->flush();
    }
}

//接收服务器端信息
void ChatClient::readServerData(){
    if(m_socket){
        return;
    }
    QByteArray data=m_socket->readAll();
    QString msg=QString::fromUtf8(data);
    QStringList lines=msg.split('\n',Qt::SkipEmptyParts);
    for(const QString &line:lines){
        ui->mes_textEdit->append("对方："+line);
    }
}

//断开连接
void ChatClient::on_disconnect(){
    ui->mes_textEdit->append(tr("===== 已断开连接 ====="));
}