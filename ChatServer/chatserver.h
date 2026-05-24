#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    void startServer(quint16 port);
    void closeServer();

private slots:
    void newClientConnect();
    void readData();
    void clientLeave();

private:
    void handleLogin(QTcpSocket *client, const QString &line);
    void handleRegister(QTcpSocket *client, const QString &line);
    void sendLine(QTcpSocket *client, const QString &line);
    void broadcastChat(QTcpSocket *senderSocket, const QString &msg);
    int indexOfClient(QTcpSocket *client) const;

    QTcpServer *m_server;
    QList<QTcpSocket *> m_clients;
    QList<QByteArray> m_recvBuffers;
    QList<QString> m_nicknames;
};

#endif // CHATSERVER_H
