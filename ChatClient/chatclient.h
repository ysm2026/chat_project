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
    void setTcpSocket(QTcpSocket *socket, const QStringList &pendingServerLines = {});

private slots:
    void on_send_button_clicked();
    void readServerData();
    void on_disconnect();
    void on_user_listWidget_itemClicked();

private:
    void sendPacket(const QString &line);
    void appendChatLine(const QString &line);
    void updateChatModeLabel();
    void ensureSelfContactItem();
    void dedupeSelfContactEntries();
    bool isSelfContactEntry(const QString &username, const QString &nickname) const;

    Ui::ChatClient *ui;
    QTcpSocket *m_socket;
    QString m_username;
    QString m_nickname;
    QByteArray m_recvBuffer;
    QString m_privateTargetUser;
};

#endif // CHATCLIENT_H
