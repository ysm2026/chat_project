#include "registerwindow.h"
#include "ui_registerwindow.h"
#include <QDebug>
#include <QCryptographicHash>

#include "../common/netpacket.h"
#include "../ChatServer/dbconfig.h"

namespace {
QString md5ForTransport(const QString &rawPassword)
{
    return QCryptographicHash::hash(rawPassword.toUtf8(), QCryptographicHash::Md5).toHex();
}

constexpr int kRequestTimeoutMs = 15000;
}

registerwindow::registerwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::registerwindow)
    , m_socket(nullptr)
    , m_pendingRegister(false)
    , m_waitingRegisterResponse(false)
{
    ui->setupUi(this);
    setWindowTitle(tr("用户注册"));

    ui->pwd_lineEdit->setEchoMode(QLineEdit::Password);
    ui->comfirem_lineEdit->setEchoMode(QLineEdit::Password);
    clearError();

    connect(ui->true_pushButton, &QPushButton::clicked, this, &registerwindow::on_true_pushButton_clicked);
    connect(ui->casual_pushButton, &QPushButton::clicked, this, &registerwindow::on_casual_pushButton_clicked);

    m_registerTimeout.setSingleShot(true);
    connect(&m_registerTimeout, &QTimer::timeout, this, &registerwindow::onRegisterTimeout);
}

registerwindow::~registerwindow()
{
    delete ui;
}

void registerwindow::setTcpSocket(QTcpSocket *socket)
{
    detachSocketHandlers();
    m_socket = socket;
    if(!m_socket){
        return;
    }
    connect(m_socket, &QTcpSocket::connected, this, &registerwindow::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &registerwindow::readRegisterData);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &registerwindow::onSocketError);
}

void registerwindow::detachSocketHandlers()
{
    m_registerTimeout.stop();
    if(!m_socket){
        return;
    }
    m_recvBuffer.clear();
    m_waitingRegisterResponse = false;
    disconnect(m_socket, nullptr, this, nullptr);
    m_socket = nullptr;
}

void registerwindow::resetForm()
{
    ui->user_lineEdit->clear();
    ui->nic_lineEdit->clear();
    ui->pwd_lineEdit->clear();
    ui->comfirem_lineEdit->clear();
    clearError();
    m_pendingRegister = false;
    m_waitingRegisterResponse = false;
    m_registerTimeout.stop();
    setRegisterEnable(true);
}

void registerwindow::on_true_pushButton_clicked()
{
    m_username = ui->user_lineEdit->text().trimmed();
    m_password = ui->pwd_lineEdit->text();
    const QString confirmPwd = ui->comfirem_lineEdit->text();
    m_nickname = ui->nic_lineEdit->text().trimmed();

    clearError();

    if(m_username.isEmpty()){
        showError(tr("用户名不能为空"));
        return;
    }
    if(m_nickname.isEmpty()){
        showError(tr("昵称不能为空"));
        return;
    }
    if(m_password.isEmpty()){
        showError(tr("密码不能为空"));
        return;
    }
    if(m_password != confirmPwd){
        showError(tr("两次输入的密码不一致"));
        return;
    }

    if(!m_socket){
        showError(tr("未连接服务器，请从登录页重试"));
        return;
    }

    setRegisterEnable(false);
    m_waitingRegisterResponse = false;
    m_pendingRegister = false;

    if(m_socket->state() == QAbstractSocket::ConnectedState){
        sendRegisterRequest();
        return;
    }

    m_pendingRegister = true;
    if(m_socket->state() != QAbstractSocket::UnconnectedState){
        m_socket->abort();
    }
    m_socket->connectToHost(DbConfig::kChatHost, DbConfig::kChatPort);
}

void registerwindow::on_casual_pushButton_clicked()
{
    m_pendingRegister = false;
    m_waitingRegisterResponse = false;
    m_registerTimeout.stop();
    emit registerCancel();
}

void registerwindow::onSocketConnected()
{
    if(m_pendingRegister){
        m_pendingRegister = false;
        sendRegisterRequest();
    }
}

void registerwindow::sendRegisterRequest()
{
    m_waitingRegisterResponse = true;
    m_recvBuffer.clear();
    m_registerTimeout.start(kRequestTimeoutMs);

    const QString transportPassword = md5ForTransport(m_password);
    const QString payload = QString("REGISTER|%1|%2|%3")
                                .arg(m_username, transportPassword, m_nickname);
    sendPacket(payload);
    qDebug() << "发送注册请求: REGISTER|" << m_username << "|***|" << m_nickname;
}

void registerwindow::sendPacket(const QString &line)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    m_socket->write(NetPacket::encode(line));
}

void registerwindow::finishRegisterRequest()
{
    m_registerTimeout.stop();
    m_waitingRegisterResponse = false;
    m_pendingRegister = false;
    setRegisterEnable(true);
}

void registerwindow::readRegisterData()
{
    if(!m_socket){
        return;
    }

    QStringList lines;
    NetPacket::feed(m_recvBuffer, m_socket->readAll(), lines);

    for (const QString &rawResponse : lines) {
        const QString response = rawResponse.trimmed();
        if (response.isEmpty()) {
            continue;
        }
        qDebug() << "注册收到服务器响应:" << response;

        if(!m_waitingRegisterResponse){
            continue;
        }

        if(response.startsWith("REGISTER_OK")){
            m_recvBuffer.clear();
            finishRegisterRequest();
            emit registerSuccess();
            return;
        }

        if(response.startsWith("REGISTER_FAIL|")){
            QString reason = response.section('|', 1);
            if(reason.isEmpty()){
                reason = tr("注册失败");
            }
            finishRegisterRequest();
            showError(reason);
            return;
        }

        finishRegisterRequest();
        showError(tr("服务器响应异常，请确认 ChatServer 已重新编译并正在运行"));
        return;
    }
}

void registerwindow::onSocketError()
{
    if(!m_pendingRegister && !m_waitingRegisterResponse){
        return;
    }

    finishRegisterRequest();

    if(!m_socket){
        return;
    }

    const QString err = m_socket->errorString();
    if(m_socket->error() == QAbstractSocket::ConnectionRefusedError){
        showError(tr("无法连接服务器(%1:%2)，请先启动 ChatServer")
                      .arg(DbConfig::kChatHost)
                      .arg(DbConfig::kChatPort));
    }
    else{
        showError(tr("网络错误：%1").arg(err));
    }
    qDebug() << "注册连接失败:" << err;
}

void registerwindow::onRegisterTimeout()
{
    if (!m_waitingRegisterResponse) {
        return;
    }
    finishRegisterRequest();
    showError(tr("注册请求超时，请稍后重试"));
}

void registerwindow::setRegisterEnable(bool enabled)
{
    ui->user_lineEdit->setEnabled(enabled);
    ui->nic_lineEdit->setEnabled(enabled);
    ui->pwd_lineEdit->setEnabled(enabled);
    ui->comfirem_lineEdit->setEnabled(enabled);
    ui->true_pushButton->setEnabled(enabled);
    ui->casual_pushButton->setEnabled(enabled);
}

void registerwindow::showError(const QString &msg)
{
    ui->err_tip->setStyleSheet("color: red;");
    ui->err_tip->setText(msg);
}

void registerwindow::clearError()
{
    ui->err_tip->setStyleSheet("");
    ui->err_tip->clear();
}
