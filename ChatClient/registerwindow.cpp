#include "registerwindow.h"
#include "ui_registerwindow.h"
#include <QDebug>

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
    if(!m_socket){
        return;
    }
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

    // 未连接：先连服务器，连上后在 onSocketConnected 里发 REGISTER
    m_pendingRegister = true;
    if(m_socket->state() != QAbstractSocket::UnconnectedState){
        m_socket->abort();
    }
    m_socket->connectToHost("127.0.0.1", 8888);
}

void registerwindow::on_casual_pushButton_clicked()
{
    m_pendingRegister = false;
    m_waitingRegisterResponse = false;
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
    const QString payload = QString("REGISTER|%1|%2|%3\n")
                                .arg(m_username, m_password, m_nickname);
    m_socket->write(payload.toUtf8());
    m_socket->flush();
    qDebug() << "发送注册请求:" << payload;
}

void registerwindow::readRegisterData()
{
    if(!m_socket){
        return;
    }

    const QByteArray data = m_socket->readAll();
    const QString response = QString::fromUtf8(data).trimmed();
    qDebug() << "注册收到服务器响应:" << response;

    if(!m_waitingRegisterResponse){
        return;
    }

    if(response.startsWith("REGISTER_OK")){
        m_waitingRegisterResponse = false;
        setRegisterEnable(true);
        emit registerSuccess();
        return;
    }

    if(response.startsWith("REGISTER_FAIL|")){
        m_waitingRegisterResponse = false;
        setRegisterEnable(true);
        QString reason = response.section('|', 1);
        if(reason.isEmpty()){
            reason = tr("注册失败");
        }
        showError(reason);
        return;
    }

    // 已发注册请求但收到非注册协议（例如旧版服务器只广播、未实现 REGISTER）
    m_waitingRegisterResponse = false;
    setRegisterEnable(true);
    showError(tr("服务器响应异常，请确认 ChatServer 已重新编译并正在运行"));
}

void registerwindow::onSocketError()
{
    // 仅处理「本次点击注册」触发的连接错误，忽略历史错误或无关信号
    if(!m_pendingRegister){
        return;
    }

    m_pendingRegister = false;
    m_waitingRegisterResponse = false;
    setRegisterEnable(true);

    if(!m_socket){
        return;
    }

    const QString err = m_socket->errorString();
    if(m_socket->error() == QAbstractSocket::ConnectionRefusedError){
        showError(tr("无法连接服务器(127.0.0.1:8888)，请先启动 ChatServer"));
    }
    else{
        showError(tr("网络错误：%1").arg(err));
    }
    qDebug() << "注册连接失败:" << err;
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
