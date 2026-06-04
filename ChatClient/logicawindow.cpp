#include "logicawindow.h"
#include "ui_logicawindow.h"
#include <QMessageBox>
#include <QTcpSocket>
#include <QDebug>
#include <QCryptographicHash>
#include <QPixmap>
#include <utility>

#include "../common/netpacket.h"
#include "../ChatServer/dbconfig.h"

namespace {
QString md5ForTransport(const QString &rawPassword)
{
    return QCryptographicHash::hash(rawPassword.toUtf8(), QCryptographicHash::Md5).toHex();
}

constexpr int kRequestTimeoutMs = 15000;
}

LogicaWindow::LogicaWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogicaWindow)
    ,socket(new QTcpSocket(this))
    ,m_pendingLogin(false)
    ,m_waitingLoginResponse(false)
{
    ui->setupUi(this);

    const QPixmap pix(":/login_bg.png");
    if (!pix.isNull()) {
        ui->label->setPixmap(pix);
    }

    setWindowTitle(tr("登录"));
    ui->pwd_lineEdit->setEchoMode(QLineEdit::Password);

    connect(socket,&QTcpSocket::connected,this,&LogicaWindow::on_SocketConnected);
    connect(socket,&QTcpSocket::readyRead,this,&LogicaWindow::readLoginData);
    connect(socket,&QTcpSocket::errorOccurred,this,&LogicaWindow::socketError);

    m_loginTimeout.setSingleShot(true);
    connect(&m_loginTimeout, &QTimer::timeout, this, &LogicaWindow::onLoginTimeout);
}

LogicaWindow::~LogicaWindow()
{
    delete ui;
}

void LogicaWindow::on_logica_pushButton_clicked(){
    m_username=ui->user_lineEdit->text().trimmed();
    m_password=ui->pwd_lineEdit->text();

    if(m_username.isEmpty()){
        QMessageBox::warning(this,tr("提示"),tr("用户名不能为空"));
        return;
    }
    if(m_password.isEmpty()){
        QMessageBox::warning(this,tr("提示"),tr("密码不能为空"));
        return;
    }

    setLoginEnable(false);

    if(socket->state()==QAbstractSocket::ConnectedState){
        sendLoginRequest();
    }
    else{
        m_pendingLogin=true;
        if(socket->state()!=QAbstractSocket::UnconnectedState){
            socket->abort();
        }
        socket->connectToHost(DbConfig::kChatHost, DbConfig::kChatPort);
    }
}

void LogicaWindow::on_regist_pushButton_clicked(){
    emit switchRegister();
}

QStringList LogicaWindow::takePendingServerLines()
{
    return std::exchange(m_pendingServerLines, QStringList());
}

void LogicaWindow::pauseLoginSocketHandlers()
{
    m_loginTimeout.stop();
    m_recvBuffer.clear();
    m_waitingLoginResponse = false;
    disconnect(socket, &QTcpSocket::readyRead, this, &LogicaWindow::readLoginData);
    disconnect(socket, &QTcpSocket::errorOccurred, this, &LogicaWindow::socketError);
    disconnect(socket, &QTcpSocket::connected, this, &LogicaWindow::on_SocketConnected);
}

void LogicaWindow::resumeLoginSocketHandlers()
{
    disconnect(socket, &QTcpSocket::readyRead, this, &LogicaWindow::readLoginData);
    disconnect(socket, &QTcpSocket::errorOccurred, this, &LogicaWindow::socketError);
    disconnect(socket, &QTcpSocket::connected, this, &LogicaWindow::on_SocketConnected);

    connect(socket, &QTcpSocket::readyRead, this, &LogicaWindow::readLoginData);
    connect(socket, &QTcpSocket::errorOccurred, this, &LogicaWindow::socketError);
    connect(socket, &QTcpSocket::connected, this, &LogicaWindow::on_SocketConnected);
}

void LogicaWindow::on_SocketConnected(){
    if(m_pendingLogin){
        m_pendingLogin = false;
        sendLoginRequest();
    }
}

void LogicaWindow::sendPacket(const QString &line)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    socket->write(NetPacket::encode(line));
}

void LogicaWindow::sendLoginRequest(){
    m_waitingLoginResponse = true;
    m_recvBuffer.clear();
    m_loginTimeout.start(kRequestTimeoutMs);

    const QString transportPassword = md5ForTransport(m_password);
    const QString payload = QString("LOGIN|%1|%2").arg(m_username, transportPassword);
    sendPacket(payload);
    qDebug() << "发送登录请求: LOGIN|" << m_username << "|***";
}

void LogicaWindow::finishLoginRequest(bool success)
{
    Q_UNUSED(success);
    m_loginTimeout.stop();
    m_waitingLoginResponse = false;
    m_pendingLogin = false;
    setLoginEnable(true);
}

void LogicaWindow::readLoginData(){
    if (!socket) {
        return;
    }
    QStringList lines;
    NetPacket::feed(m_recvBuffer, socket->readAll(), lines);

    bool loginOk = false;
    for (const QString &rawResponse : lines) {
        const QString response = rawResponse.trimmed();
        if (response.isEmpty()) {
            continue;
        }
        qDebug() << "接收服务器响应:" << response;

        if(response.startsWith("LOGIN_OK|")){
            m_nickname=response.section('|',1);
            if(m_nickname.isEmpty()){
                m_nickname=m_username;
            }
            loginOk = true;
            continue;
        }
        if(response.startsWith("LOGIN_FAIL|")){
            QString reason=response.section('|',1);
            if(reason.isEmpty()){
                reason=tr("用户名或密码错误");
            }
            finishLoginRequest(false);
            QMessageBox::warning(this,tr("登录失败"),reason);
            return;
        }

        if (m_waitingLoginResponse) {
            // 与 LOGIN_OK 同包到达的 ONLINE_LIST / USER_ONLINE 等，交给聊天窗处理
            m_pendingServerLines.append(response);
        }
    }

    if (loginOk) {
        m_recvBuffer.clear();
        finishLoginRequest(true);
        emit loginSuccess();
    }
}

void LogicaWindow::socketError(){
    m_loginTimeout.stop();
    m_pendingLogin=false;
    m_waitingLoginResponse=false;
    setLoginEnable(true);
    QMessageBox::critical(this,tr("网络错误"),socket->errorString());
}

void LogicaWindow::onLoginTimeout()
{
    if (!m_waitingLoginResponse) {
        return;
    }
    finishLoginRequest(false);
    QMessageBox::warning(this, tr("登录超时"), tr("服务器长时间无响应，请稍后重试"));
}

void LogicaWindow::setLoginEnable(bool enabled){
    ui->user_lineEdit->setEnabled(enabled);
    ui->pwd_lineEdit->setEnabled(enabled);
    ui->logica_pushButton->setEnabled(enabled);
    ui->regist_pushButton->setEnabled(enabled);
}
