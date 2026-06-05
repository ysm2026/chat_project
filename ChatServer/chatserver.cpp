#include "chatserver.h"

#include <QDebug>
#include <QDateTime>
#include <QHostAddress>

#include "../common/netpacket.h"
#include "dbhelper.h"

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &ChatServer::newClientConnect);

    // 启动心跳扫描定时器：定期调用 checkHeartbeatTimeouts 清理假在线连接
    m_heartbeatCheckTimer.setInterval(kHeartbeatCheckIntervalMs);
    connect(&m_heartbeatCheckTimer, &QTimer::timeout, this, &ChatServer::checkHeartbeatTimeouts);
    m_heartbeatCheckTimer.start();
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
    m_userIds.append(0);
    m_lastActiveMs.append(QDateTime::currentMSecsSinceEpoch());  // 新连接初始活跃时间

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
    int userId = 0;
    if (Dbhelper::getInstance().loging(username, password, nickname, &userId)) {
        if (nickname.isEmpty()) {
            nickname = username;
        }

        const int idx = indexOfClient(client);
        if (idx >= 0) {
            m_usernames[idx] = username;
            m_nicknames[idx] = nickname;
            m_userIds[idx] = userId;
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
    const QString content = line.section('|', 2, -1).trimmed();
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

    const int senderUserId = m_userIds.at(senderIdx);
    const int receiverUserId = Dbhelper::getInstance().getUserIdByUsername(targetUser);
    if (receiverUserId <= 0) {
        sendPacket(senderSocket, "SYS|对方用户名不存在");
        return;
    }

    QTcpSocket *targetSock = m_onlineByUsername.value(targetUser, nullptr);
    const bool receiverOnline = (targetSock != nullptr);
    const int messageId = Dbhelper::getInstance().savePrivate_Message(
        senderUserId, receiverUserId, content, receiverOnline ? 1 : 0);
    if (messageId <= 0) {
        sendPacket(senderSocket, "SYS|私聊消息保存失败，请稍后重试");
        return;
    }

    const QString payload = "PRIVATE|" + fromNick + "|" + fromUser + "|" + content;
    if (receiverOnline) {
        sendPacket(targetSock, payload);
        qDebug() << "私聊(在线)" << fromUser << "->" << targetUser << content;
    } else {
        sendPacket(senderSocket, "SYS|对方不在线，消息已保存，对方上线后将收到");
        qDebug() << "私聊(离线入库)" << fromUser << "->" << targetUser << content;
    }
}

void ChatServer::deliverOfflinePrivateMessages(QTcpSocket *client, int userId)
{
    if (!client || userId <= 0) {
        return;
    }

    const QList<PendingPrivateMessage> pending =
        Dbhelper::getInstance().fetchUndeliveredPrivateMessages(userId);
    if (pending.isEmpty()) {
        return;
    }

    sendPacket(client, QString("OFFLINE_BEGIN|%1").arg(pending.size()));

    QList<int> deliveredIds;
    deliveredIds.reserve(pending.size());
    for (const PendingPrivateMessage &item : pending) {
        const QString sentAt = item.createdAt.isValid()
            ? item.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString();
        const QString line = QString("OFFLINE_PRIVATE|%1|%2|%3|%4")
                                 .arg(item.fromNickname,
                                      item.fromUsername,
                                      item.content,
                                      sentAt);
        sendPacket(client, line);
        deliveredIds.append(item.id);
    }

    Dbhelper::getInstance().markPrivateMessagesDelivered(deliveredIds);
    sendPacket(client, QString("OFFLINE_DONE|%1").arg(pending.size()));
    qDebug() << "投递离线私聊" << pending.size() << "条 -> userId" << userId;
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

// 任意来自该客户端的数据（PING、CHAT、LOGIN 等）均刷新活跃时间，避免纯聊天用户被误踢
void ChatServer::touchClientActivity(QTcpSocket *client)
{
    const int idx = indexOfClient(client);
    if (idx >= 0) {
        m_lastActiveMs[idx] = QDateTime::currentMSecsSinceEpoch();
    }
}

// 收到应用层 PING：回复 PONG，不广播、不写库；touchClientActivity 已在 readData 中调用
void ChatServer::handlePing(QTcpSocket *client)
{
    sendPacket(client, "PONG");
}

// 心跳超时检测：90 秒内无任何数据 → disconnectFromHost → 触发 clientLeave 清理 Hash/广播 OFFLINE
void ChatServer::checkHeartbeatTimeouts()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = m_clients.size() - 1; i >= 0; --i) {
        QTcpSocket *sock = m_clients.at(i);
        if (!sock || sock->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        if (now - m_lastActiveMs.at(i) > kHeartbeatTimeoutMs) {
            const QString username = m_usernames.at(i);
            qDebug() << "心跳超时，断开连接:" << (username.isEmpty() ? sock->peerAddress().toString() : username);
            sock->disconnectFromHost();
        }
    }
}

void ChatServer::dispatchLine(QTcpSocket *client, const QString &line)
{
    // 应用层心跳：客户端每 60s 发 PING，服务端回 PONG
    if (line == QLatin1String("PING") || line.startsWith(QLatin1String("PING|"))) {
        handlePing(client);
    } else if (line == QLatin1String("FETCH_OFFLINE")
               || line.startsWith(QLatin1String("FETCH_OFFLINE|"))) {
        const int idx = indexOfClient(client);
        if (idx >= 0 && m_userIds.at(idx) > 0) {
            deliverOfflinePrivateMessages(client, m_userIds.at(idx));
        }
        return;
    } else if (line.startsWith("LOGIN|")) {
        handleLogin(client, line);
    } else if (line.startsWith("REGISTER|")) {
        handleRegister(client, line);
    } else if (line.startsWith("PRIVATE|")) {
        handlePrivateChat(client, line);
    } else if (line.startsWith("CHAT|")) {
        broadcastChat(client, line.section('|', 1));
    } else if (isLoggedIn(client)) {
        // 内部信令不得当作群聊内容广播
        if (line == QLatin1String("PING")
            || line.startsWith(QLatin1String("PING|"))
            || line == QLatin1String("FETCH_OFFLINE")
            || line.startsWith(QLatin1String("FETCH_OFFLINE|"))) {
            return;
        }
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
        touchClientActivity(client);  // 每条完整报文均视为该连接仍存活
        dispatchLine(client, line);
    }
}

void ChatServer::clientLeave()
{
    // 正常断开或心跳超时 disconnectFromHost 后都会进入此槽：
    // 移除 m_onlineByUsername、广播 USER_OFFLINE、清理并行数组（含 m_lastActiveMs）
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
        m_userIds.removeAt(idx);
        m_lastActiveMs.removeAt(idx);
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
    m_userIds.clear();
    m_lastActiveMs.clear();
    m_onlineByUsername.clear();
    m_heartbeatCheckTimer.stop();
    qDebug() << "服务器已关闭";
}
