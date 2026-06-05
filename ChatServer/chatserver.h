#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QTimer>

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    bool startServer(quint16 port);
    void closeServer();

private slots:
    void newClientConnect();
    void readData();
    void clientLeave();
    void checkHeartbeatTimeouts();

private:
    void handleLogin(QTcpSocket *client, const QString &line);
    void handleRegister(QTcpSocket *client, const QString &line);
    void handlePrivateChat(QTcpSocket *senderSocket, const QString &line);
    void dispatchLine(QTcpSocket *client, const QString &line);
    void sendPacket(QTcpSocket *client, const QString &line);
    void broadcastLine(const QString &line, QTcpSocket *except = nullptr);
    void broadcastChat(QTcpSocket *senderSocket, const QString &msg);
    void notifyUserOnline(QTcpSocket *client);
    void notifyUserOffline(const QString &nickname, const QString &username);
    QString buildOnlineList() const;
    int indexOfClient(QTcpSocket *client) const;
    bool isLoggedIn(QTcpSocket *client) const;
    void touchClientActivity(QTcpSocket *client);
    void handlePing(QTcpSocket *client);
    void deliverOfflinePrivateMessages(QTcpSocket *client, int userId);

    // 心跳参数（与客户端 60s PING 配合）：
    //   - 每 kHeartbeatCheckIntervalMs 扫描所有连接
    //   - 某连接超过 kHeartbeatTimeoutMs 未收到该客户端任何数据 → 判定断开
    static constexpr int kHeartbeatCheckIntervalMs = 10000;  // 每 10 秒扫描一次
    static constexpr int kHeartbeatTimeoutMs = 90000;        // 90 秒无数据则踢掉连接

    QTcpServer *m_server;
    QTimer m_heartbeatCheckTimer;
    QList<QTcpSocket *> m_clients;
    QList<QByteArray> m_recvBuffers;
    QList<QString> m_usernames;
    QList<QString> m_nicknames;
    QList<int> m_userIds;
    QList<qint64> m_lastActiveMs;  // 与 m_clients 同下标，记录各连接最后收到数据的时间
    QHash<QString, QTcpSocket *> m_onlineByUsername;
};

#endif // CHATSERVER_H
