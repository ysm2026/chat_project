#include "chatserver.h"

#include <QDebug>
#include <QHostAddress>

#include "../common/netpacket.h"
#include "dbhelper.h"

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &ChatServer::newClientConnect);
}

bool ChatServer::startServer(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "服务器启动成功，端口：" << port;
        return true;
    }
    qDebug() << "服务器启动失败：" << m_server->errorString();
    return false;
}

void ChatServer::newClientConnect()
{
    QTcpSocket *client = m_server->nextPendingConnection();
    m_clients.append(client);
    m_recvBuffers.append(QByteArray());
    m_usernames.append(QString());
    m_nicknames.append(QString());

    qDebug() << "新客户端上线:" << client->peerAddress().toString();

    connect(client, &QTcpSocket::readyRead, this, &ChatServer::readData);
    connect(client, &QTcpSocket::disconnected, this, &ChatServer::clientLeave);
}

int ChatServer::indexOfClient(QTcpSocket *client) const
{
    return m_clients.indexOf(client);
}

bool ChatServer::isLoggedIn(QTcpSocket *client) const
{
    const int idx = indexOfClient(client);
    return idx >= 0 && !m_usernames.at(idx).isEmpty();
}

void ChatServer::sendPacket(QTcpSocket *client, const QString &line)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    client->write(NetPacket::encode(line));
}

void ChatServer::broadcastLine(const QString &line, QTcpSocket *except)
{
    for (QTcpSocket *sock : m_clients) {
        if (!sock || sock == except
            || sock->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        sendPacket(sock, line);
    }
}

void ChatServer::handleLogin(QTcpSocket *client, const QString &line)
{
    const QString username = line.section('|', 1, 1).trimmed();
    const QString password = line.section('|', 2, 2);

    if (username.isEmpty() || password.isEmpty()) {
        sendPacket(client, "LOGIN_FAIL|用户名或密码不能为空");
        return;
    }

    if (m_onlineByUsername.contains(username)) {
        sendPacket(client, "LOGIN_FAIL|该账号已在别处登录，请先退出");
        qDebug() << "重复登录被拒绝:" << username;
        return;
    }

    QString nickname;
    if (Dbhelper::getInstance().loging(username, password, nickname)) {
        if (nickname.isEmpty()) {
            nickname = username;
        }

        const int idx = indexOfClient(client);
        if (idx >= 0) {
            m_usernames[idx] = username;
            m_nicknames[idx] = nickname;
        }
        m_onlineByUsername.insert(username, client);

        sendPacket(client, "LOGIN_OK|" + nickname);
        notifyUserOnline(client);
        qDebug() << "用户登录成功:" << username;
    } else {
        sendPacket(client, "LOGIN_FAIL|用户名或密码错误");
        qDebug() << "用户登录失败:" << username;
    }
}

void ChatServer::handleRegister(QTcpSocket *client, const QString &line)
{
    const QString username = line.section('|', 1, 1).trimmed();
    const QString password = line.section('|', 2, 2);
    const QString nickname = line.section('|', 3).trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        sendPacket(client, "REGISTER_FAIL|用户名或密码不能为空");
        return;
    }
    if (nickname.isEmpty()) {
        sendPacket(client, "REGISTER_FAIL|昵称不能为空");
        return;
    }

    if (Dbhelper::getInstance().userExists(username)) {
        sendPacket(client, "REGISTER_FAIL|用户名已存在");
        return;
    }

    if (Dbhelper::getInstance().registerUser(username, password, nickname, "", "")) {
        sendPacket(client, "REGISTER_OK");
        qDebug() << "用户注册成功:" << username;
    } else {
        sendPacket(client, "REGISTER_FAIL|注册失败，请稍后重试");
    }
}

void ChatServer::broadcastChat(QTcpSocket *senderSocket, const QString &msg)
{
    const QString text = msg.trimmed();
    if (text.isEmpty()) {
        return;
    }

    const int senderIdx = indexOfClient(senderSocket);
    if (senderIdx < 0 || m_usernames.at(senderIdx).isEmpty()) {
        sendPacket(senderSocket, "SYS|请先登录后再发言");
        return;
    }

    const QString nickname = m_nicknames.at(senderIdx);
    const QString payload = "CHAT|" + nickname + "|" + text;

    for (QTcpSocket *sock : m_clients) {
        if (!sock || sock == senderSocket
            || sock->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        sendPacket(sock, payload);
    }
    qDebug() << "群聊:" << nickname << text;
}

void ChatServer::handlePrivateChat(QTcpSocket *senderSocket, const QString &line)
{
    if (!isLoggedIn(senderSocket)) {
        sendPacket(senderSocket, "SYS|请先登录后再发私聊");
        return;
    }

    const QString targetUser = line.section('|', 1, 1).trimmed();
    const QString content = line.section('|', 2).trimmed();
    if (targetUser.isEmpty() || content.isEmpty()) {
        return;
    }

    const int senderIdx = indexOfClient(senderSocket);
    const QString fromUser = m_usernames.at(senderIdx);
    const QString fromNick = m_nicknames.at(senderIdx);

    // 发给自己：仅作备忘，不向客户端回显私聊包（避免重复显示）
    if (targetUser == fromUser) {
        qDebug() << "备忘" << fromUser << content;
        return;
    }

    QTcpSocket *targetSock = m_onlineByUsername.value(targetUser, nullptr);
    if (!targetSock) {
        sendPacket(senderSocket, "SYS|对方不在线或用户名不存在");
        return;
    }

    const QString payload = "PRIVATE|" + fromNick + "|" + fromUser + "|" + content;
    sendPacket(targetSock, payload);
    sendPacket(senderSocket, payload);
    qDebug() << "私聊" << fromUser << "->" << targetUser << content;
}

QString ChatServer::buildOnlineList() const
{
    QStringList names;
    for (int i = 0; i < m_usernames.size(); ++i) {
        const QString &u = m_usernames.at(i);
        if (!u.isEmpty()) {
            const QString &nick = m_nicknames.at(i);
            names.append(u + "|" + (nick.isEmpty() ? u : nick));
        }
    }
    return names.join(';');
}

void ChatServer::notifyUserOnline(QTcpSocket *client)
{
    const int idx = indexOfClient(client);
    if (idx < 0) {
        return;
    }

    const QString username = m_usernames.at(idx);
    const QString nickname = m_nicknames.at(idx);
    if (username.isEmpty()) {
        return;
    }

    sendPacket(client, "ONLINE_LIST|" + buildOnlineList());
    broadcastLine("USER_ONLINE|" + nickname + "|" + username, client);
}

void ChatServer::notifyUserOffline(const QString &nickname, const QString &username)
{
    if (nickname.isEmpty() && username.isEmpty()) {
        return;
    }
    broadcastLine("USER_OFFLINE|" + nickname + "|" + username);
}

void ChatServer::dispatchLine(QTcpSocket *client, const QString &line)
{
    if (line.startsWith("LOGIN|")) {
        handleLogin(client, line);
    } else if (line.startsWith("REGISTER|")) {
        handleRegister(client, line);
    } else if (line.startsWith("PRIVATE|")) {
        handlePrivateChat(client, line);
    } else if (line.startsWith("CHAT|")) {
        broadcastChat(client, line.section('|', 1));
    } else if (isLoggedIn(client)) {
        broadcastChat(client, line);
    } else {
        sendPacket(client, "SYS|请先登录后再发言");
    }
}

void ChatServer::readData()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) {
        return;
    }

    const int idx = indexOfClient(client);
    if (idx < 0) {
        return;
    }

    QByteArray &buffer = m_recvBuffers[idx];
    QStringList lines;
    NetPacket::feed(buffer, client->readAll(), lines);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        dispatchLine(client, line);
    }
}

void ChatServer::clientLeave()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client) {
        return;
    }

    const int idx = indexOfClient(client);
    if (idx >= 0) {
        const QString username = m_usernames.at(idx);
        const QString nickname = m_nicknames.at(idx);

        if (!username.isEmpty()) {
            m_onlineByUsername.remove(username);
        }
        notifyUserOffline(nickname, username);

        m_clients.removeAt(idx);
        m_recvBuffers.removeAt(idx);
        m_usernames.removeAt(idx);
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

    const QList<QTcpSocket *> clients = m_clients;
    for (QTcpSocket *sock : clients) {
        if (sock) {
            sock->disconnectFromHost();
            sock->deleteLater();
        }
    }
    m_clients.clear();
    m_recvBuffers.clear();
    m_usernames.clear();
    m_nicknames.clear();
    m_onlineByUsername.clear();
    qDebug() << "服务器已关闭";
}
