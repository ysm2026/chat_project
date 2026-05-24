#include "chatserver.h"

#include <QDebug>
#include <QHostAddress>

#include "dbhelper.h"

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &ChatServer::newClientConnect);
}

void ChatServer::startServer(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "服务器启动成功，端口：" << port;
    } else {
        qDebug() << "服务器启动失败：" << m_server->errorString();
    }
}

void ChatServer::newClientConnect()
{
    QTcpSocket *client = m_server->nextPendingConnection();
    m_clients.append(client);
    m_recvBuffers.append(QByteArray());
    m_nicknames.append(QString());

    qDebug() << "新客户端上线:" << client->peerAddress().toString();

    connect(client, &QTcpSocket::readyRead, this, &ChatServer::readData);
    connect(client, &QTcpSocket::disconnected, this, &ChatServer::clientLeave);
}

int ChatServer::indexOfClient(QTcpSocket *client) const
{
    return m_clients.indexOf(client);
}

void ChatServer::sendLine(QTcpSocket *client, const QString &line)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    client->write((line + "\n").toUtf8());
    client->flush();
}

void ChatServer::handleLogin(QTcpSocket *client, const QString &line)
{
    QStringList parts = line.split('|');
    if (parts.size() < 3) {
        sendLine(client, "LOGIN_FAIL|请求格式错误");
        return;
    }

    QString username = parts.at(1).trimmed();
    QString password = parts.at(2);

    if (username.isEmpty() || password.isEmpty()) {
        sendLine(client, "LOGIN_FAIL|用户名或密码不能为空");
        return;
    }

    QString nickname;
    if (Dbhelper::getInstance().loging(username, password, nickname)) {
        if (nickname.isEmpty()) {
            nickname = username;
        }
        sendLine(client, "LOGIN_OK|" + nickname);
        int idx = indexOfClient(client);
        if (idx >= 0) {
            m_nicknames[idx] = nickname;
        }
        qDebug() << "用户登录成功:" << username;
    } else {
        sendLine(client, "LOGIN_FAIL|用户名或密码错误");
        qDebug() << "用户登录失败:" << username;
    }
}

void ChatServer::handleRegister(QTcpSocket *client, const QString &line)
{
    QStringList parts = line.split('|');
    if (parts.size() < 4) {
        sendLine(client, "REGISTER_FAIL|请求格式错误");
        return;
    }

    QString username = parts.at(1).trimmed();
    QString password = parts.at(2);
    QString nickname = parts.at(3).trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        sendLine(client, "REGISTER_FAIL|用户名或密码不能为空");
        return;
    }
    if (nickname.isEmpty()) {
        sendLine(client, "REGISTER_FAIL|昵称不能为空");
        return;
    }

    if (Dbhelper::getInstance().userExists(username)) {
        sendLine(client, "REGISTER_FAIL|用户名已存在");
        return;
    }

    if (Dbhelper::getInstance().registerUser(username, password, nickname, "", "")) {
        sendLine(client, "REGISTER_OK");
        qDebug() << "用户注册成功:" << username;
    } else {
        sendLine(client, "REGISTER_FAIL|注册失败，请稍后重试");
    }
}

void ChatServer::broadcastChat(QTcpSocket *senderSocket, const QString &msg)
{
    QString text = msg.trimmed();
    if (text.isEmpty()) {
        return;
    }

    int senderIdx = indexOfClient(senderSocket);
    QString nickname = tr("游客");
    if (senderIdx >= 0 && !m_nicknames.at(senderIdx).isEmpty()) {
        nickname = m_nicknames.at(senderIdx);
    }

    QString payload = "CHAT|" + nickname + "|" + text;

    for (int i = 0; i < m_clients.size(); ++i) {
        QTcpSocket *sock = m_clients.at(i);
        if (sock && sock != senderSocket
            && sock->state() == QAbstractSocket::ConnectedState) {
            sendLine(sock, payload);
        }
    }
    qDebug() << "收到聊天消息:" << nickname << text;
}

void ChatServer::readData()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) {
        return;
    }

    int idx = indexOfClient(client);
    if (idx < 0) {
        return;
    }

    QByteArray &buffer = m_recvBuffers[idx];
    buffer.append(client->readAll());

    int lineEnd = -1;
    while ((lineEnd = buffer.indexOf('\n')) >= 0) {
        QByteArray lineBytes = buffer.left(lineEnd);
        buffer.remove(0, lineEnd + 1);

        QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (line.startsWith("LOGIN|")) {
            handleLogin(client, line);
        } else if (line.startsWith("REGISTER|")) {
            handleRegister(client, line);
        } else {
            broadcastChat(client, line);
        }
    }
}

void ChatServer::clientLeave()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) {
        return;
    }

    int idx = indexOfClient(client);
    if (idx >= 0) {
        m_clients.removeAt(idx);
        m_recvBuffers.removeAt(idx);
        m_nicknames.removeAt(idx);
    }

    qDebug() << "客户端下线" << client->peerAddress().toString();
    client->deleteLater();
}

void ChatServer::closeServer()
{
    if (m_server->isListening()) {
        m_server->close();
    }

    QList<QTcpSocket *> clients = m_clients;
    for (int i = 0; i < clients.size(); ++i) {
        clients.at(i)->disconnectFromHost();
        clients.at(i)->deleteLater();
    }
    m_clients.clear();
    m_recvBuffers.clear();
    m_nicknames.clear();
    qDebug() << "服务器已关闭";
}
