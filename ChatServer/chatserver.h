#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>
#include <QHash>
#include <QString>

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

    QTcpServer *m_server;
    QList<QTcpSocket *> m_clients;
    QList<QByteArray> m_recvBuffers;
    QList<QString> m_usernames;
    QList<QString> m_nicknames;
    QHash<QString, QTcpSocket *> m_onlineByUsername;
};

#endif // CHATSERVER_H
