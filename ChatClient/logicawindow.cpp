#include "logicawindow.h"
#include "ui_logicawindow.h"
#include <QMessageBox>
#include <QTcpSocket>
#include <QDebug>

LogicaWindow::LogicaWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogicaWindow)
    ,socket(new QTcpSocket(this))
    ,m_pendingLogin(false)
{
    ui->setupUi(this);
    QPixmap pix("D:/Qt/project/chat/ChatClient/微信图片_20260312091244_6_115.png");
    ui->label->setPixmap(pix);

    //tr()函数，标记可翻译字符串，支持翻译功能
    setWindowTitle(tr("登录"));

    //将密码框设为密码模式即*模式
    ui->pwd_lineEdit->setEchoMode(QLineEdit::Password);

    //连接服务器到槽函数
    connect(socket,&QTcpSocket::connected,this,&LogicaWindow::on_SocketConnected);
    connect(socket,&QTcpSocket::readyRead,this,&LogicaWindow::readLoginData);
    connect(socket,&QTcpSocket::errorOccurred,this,&LogicaWindow::socketError);
}

LogicaWindow::~LogicaWindow()
{
    delete ui;
}

//登录按钮点击
void LogicaWindow::on_logica_pushButton_clicked(){

    //获取用户输入,用trimmed去除首尾空白
    m_username=ui->user_lineEdit->text().trimmed();
    m_password=ui->pwd_lineEdit->text();

    //输入校验
    if(m_username.isEmpty()){
        QMessageBox::warning(this,tr("提示"),tr("用户名不能为空"));
        return;
    }
    if(m_password.isEmpty()){
        QMessageBox::warning(this,tr("提示"),tr("密码不能为空"));
        return;
    }

    //禁用按钮，防止重复点击
    setLoginEnable(false);

    //判断socket状态：已连接则直接登录，未连接则先连接服务器
    if(socket->state()==QAbstractSocket::ConnectedState){
        sendLoginRequest();
    }
    else{
        m_pendingLogin=true;
        // 上次连接失败后需 abort，否则再次 connectToHost 可能立刻触发 errorOccurred
        if(socket->state()!=QAbstractSocket::UnconnectedState){
            socket->abort();
        }
        socket->connectToHost("127.0.0.1",8888);
    }
}
//注册按钮点击：通知主窗口切换到注册界面
void LogicaWindow::on_regist_pushButton_clicked(){
    emit switchRegister();
}

void LogicaWindow::pauseLoginSocketHandlers()
{
    disconnect(socket, &QTcpSocket::readyRead, this, &LogicaWindow::readLoginData);
    disconnect(socket, &QTcpSocket::errorOccurred, this, &LogicaWindow::socketError);
    disconnect(socket, &QTcpSocket::connected, this, &LogicaWindow::on_SocketConnected);
}

void LogicaWindow::resumeLoginSocketHandlers()
{
    connect(socket, &QTcpSocket::readyRead, this, &LogicaWindow::readLoginData);
    connect(socket, &QTcpSocket::errorOccurred, this, &LogicaWindow::socketError);
    connect(socket, &QTcpSocket::connected, this, &LogicaWindow::on_SocketConnected);
}

//连接服务器成功
void LogicaWindow::on_SocketConnected(){
    if(m_pendingLogin){
        sendLoginRequest();
    }
}

//发送登录请求
void LogicaWindow::sendLoginRequest(){
    //协议格式：Login/用户名/密码
    QString payload=QString("LOGIN|%1|%2\n").arg(m_username,m_password);
    socket->write(payload.toUtf8());
    socket->flush();
    qDebug()<<"发送登录请求:"<<payload;
}

//接收服务器响应
void LogicaWindow::readLoginData(){
    QByteArray data=socket->readAll();
    QString response=QString::fromUtf8(data).trimmed();
    qDebug()<<"接收服务器响应:"<<response;

    //恢复登录按钮
    setLoginEnable(true);
    m_pendingLogin=false;

    //解析服务器响应
    if(response.startsWith("LOGIN_OK|")){
        //登录成功，提取昵称
        m_nickname=response.section('|',1);
        if(m_nickname.isEmpty()){
            //先保留，之后注册再修改回来
            m_nickname=m_username;
        }
        //发出登录成功信号
        emit loginSuccess();
    }
    else if(response.startsWith("LOGIN_FAIL|")){
        QString reason=response.section('|',1);
        if(reason.isEmpty()){
            reason=tr("用户名或密码错误");
        }
        QMessageBox::warning(this,tr("登录失败"),reason);
    }
    else{
        QMessageBox::warning(this,tr("登录失败"),tr("服务器响应异常"));
    }
}

//网络错误处理
void LogicaWindow::socketError(){
    setLoginEnable(true);
    m_pendingLogin=false;
    QMessageBox::critical(this,tr("网络错误"),socket->errorString());
}

//启用界面控件
void LogicaWindow::setLoginEnable(bool enabled){
    ui->user_lineEdit->setEnabled(enabled);
    ui->pwd_lineEdit->setEnabled(enabled);
    ui->logica_pushButton->setEnabled(enabled);
    ui->regist_pushButton->setEnabled(enabled);
}
