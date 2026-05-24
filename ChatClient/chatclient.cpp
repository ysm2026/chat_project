#include "chatclient.h"
#include "./ui_chatclient.h"

#include <QMessageBox>

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
    , m_socket(nullptr)
{
    ui->setupUi(this);
    ui->mes_textEdit->setReadOnly(true);
}

ChatClient::~ChatClient()
{
    delete ui;
}

void ChatClient::setUserInfo(const QString &username, const QString &nickname)
{
    m_username = username;
    m_nickname = nickname;
    setWindowTitle(tr("聊天室-%1").arg(m_nickname));
}

void ChatClient::setTcpSocket(QTcpSocket *socket)
{
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
    }
    m_socket = socket;
    m_recvBuffer.clear();

    if (m_socket) {
        connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::readServerData);
        connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::on_disconnect);
        ui->mes_textEdit->append(tr("===== 欢迎%1，已连接服务器 =====").arg(m_nickname));
    }
}

void ChatClient::sendLine(const QString &line)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    m_socket->write((line + "\n").toUtf8());
    m_socket->flush();
}

void ChatClient::on_send_button_clicked()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, tr("提示"), tr("未连接到服务器，请重新连接"));
        return;
    }

    QString text = ui->send_lineEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    QString sender = m_nickname.isEmpty() ? tr("我") : m_nickname;
    ui->mes_textEdit->append(sender + tr("：") + text);
    ui->send_lineEdit->clear();
    sendLine(text);
}

void ChatClient::readServerData()
{
    if (!m_socket) {
        return;
    }

    m_recvBuffer.append(m_socket->readAll());

    int lineEnd = -1;
    while ((lineEnd = m_recvBuffer.indexOf('\n')) >= 0) {
        QByteArray lineBytes = m_recvBuffer.left(lineEnd);
        m_recvBuffer.remove(0, lineEnd + 1);

        QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith("CHAT|")) {
            QStringList parts = line.split('|');
            if (parts.size() >= 3) {
                QString nickname = parts.at(1);
                QString content = parts.mid(2).join("|");
                ui->mes_textEdit->append(nickname + tr("：") + content);
                continue;
            }
        }
        ui->mes_textEdit->append(tr("对方：") + line);
    }
}

void ChatClient::on_disconnect()
{
    ui->mes_textEdit->append(tr("===== 已断开连接 ====="));
    m_recvBuffer.clear();
}
