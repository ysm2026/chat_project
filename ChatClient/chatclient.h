#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui {
class ChatClient;
}
QT_END_NAMESPACE

class ChatClient : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChatClient(QWidget *parent = nullptr);
    ~ChatClient() override;

    void setUserInfo(const QString &username, const QString &nickname);
    void setTcpSocket(QTcpSocket *socket);

private slots:
    void on_send_button_clicked();
    void readServerData();
    void on_disconnect();

private:
    void sendLine(const QString &line);

    Ui::ChatClient *ui;
    QTcpSocket *m_socket;
    QString m_username;
    QString m_nickname;
    QByteArray m_recvBuffer;
};

#endif // CHATCLIENT_H
