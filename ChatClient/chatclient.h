#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>

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
    void setTcpSocket(QTcpSocket *socket,
                      const QStringList &pendingServerLines = {},
                      const QByteArray &pendingRecvBuffer = {});

private slots:
    void on_send_button_clicked();
    void readServerData();
    void on_disconnect();
    void on_user_listWidget_itemClicked();
    void onHeartbeatTick();

private:
    void sendPacket(const QString &line);
    void touchServerActivity();
    void startHeartbeat();
    void stopHeartbeat();
    void appendChatLine(const QString &line);
    void appendPrivateChatLine(const QString &nickname,
                               const QString &fromUser,
                               const QString &content,
                               bool offline,
                               const QString &sentAt = QString());
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

    // -------------------------------------------------------------------------
    // 应用层心跳（原理说明）
    //
    // 背景：TCP 在断网、休眠、进程被强杀等场景下，disconnected 信号可能延迟或不触发，
    //       服务端会误以为客户端仍在线（假在线）。
    //
    // 机制：
    //   1. 客户端每 kHeartbeatSendIntervalMs（60s）经 NetPacket 发送应用层命令 "PING"
    //      （进入聊天界面时立即发首包，之后按间隔发送）
    //   2. 服务端收到 PING 后回复 "PONG"（见 ChatServer::handlePing）
    //   3. 服务端若 kHeartbeatTimeoutMs（90s）内未收到该连接的任何数据（含 PING、
    //      CHAT、PRIVATE 等业务包），则 disconnectFromHost → clientLeave 清理资源
    //
    // 客户端侧：收到任意服务端数据（含 PONG）更新 m_lastServerActivityMs；若长时间
    //           收不到服务端数据则主动 abort，触发断线提示。
    // -------------------------------------------------------------------------
    QTimer m_heartbeatTimer;
    qint64 m_lastServerActivityMs = 0;

    static constexpr int kHeartbeatSendIntervalMs = 60000;  // 每 60 秒发送一次 PING
    static constexpr int kHeartbeatTimeoutMs = 90000;       // 90 秒无服务端数据则判定断线
};

#endif // CHATCLIENT_H
