#include "chatclient.h"
#include "./ui_chatclient.h"

#include <QMessageBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDateTime>
#include <QDebug>

#include "../common/netpacket.h"

namespace {

constexpr int kRoleUsername = Qt::UserRole;
constexpr int kRoleSelfContact = Qt::UserRole + 1;
constexpr int kRoleOnline = Qt::UserRole + 2;
constexpr int kRoleNickname = Qt::UserRole + 3;

QString formatContactLabel(const QString &nickname, const QString &username, bool online)
{
    const QString nick = nickname.isEmpty() ? username : nickname;
    const QString base = (nick == username)
        ? username
        : QString("%1 (%2)").arg(nick, username);
    return online ? base : base + QStringLiteral(" [离线]");
}

QListWidgetItem *findContactItem(QListWidget *list, const QString &username)
{
    if (!list || username.isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item && item->data(kRoleUsername).toString() == username) {
            return item;
        }
    }
    return nullptr;
}

void upsertContactItem(QListWidget *list,
                       const QString &username,
                       const QString &nickname,
                       bool online)
{
    if (!list || username.isEmpty()) {
        return;
    }

    QListWidgetItem *item = findContactItem(list, username);
    if (!item) {
        item = new QListWidgetItem(formatContactLabel(nickname, username, online));
        item->setData(kRoleUsername, username);
        item->setData(kRoleNickname, nickname);
        item->setData(kRoleOnline, online);
        list->addItem(item);
        return;
    }

    item->setData(kRoleNickname, nickname);
    item->setData(kRoleOnline, online);
    item->setText(formatContactLabel(nickname, username, online));
}

} // namespace

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
    , m_socket(nullptr)
{
    ui->setupUi(this);
    ui->mes_textEdit->setReadOnly(true);
    ui->user_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    updateChatModeLabel();

    // 定时器到期时调用 onHeartbeatTick，按间隔发送 PING
    m_heartbeatTimer.setInterval(kHeartbeatSendIntervalMs);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &ChatClient::onHeartbeatTick);
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

void ChatClient::setTcpSocket(QTcpSocket *socket,
                              const QStringList &pendingServerLines,
                              const QByteArray &pendingRecvBuffer)
{
    stopHeartbeat();
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
    }
    m_socket = socket;
    m_recvBuffer = pendingRecvBuffer;
    m_privateTargetUser.clear();
    ui->user_listWidget->clear();
    updateChatModeLabel();

    if (m_socket) {
        connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::readServerData);
        connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::on_disconnect);
        ensureSelfContactItem();

        // 先处理登录阶段缓存的离线消息等，再显示欢迎语
        for (const QString &line : pendingServerLines) {
            appendChatLine(line);
        }
        if (m_socket->bytesAvailable() > 0) {
            readServerData();
        }
        startHeartbeat();
        sendPacket(QStringLiteral("FETCH_OFFLINE"));  // 进入聊天后拉取离线私聊
    }
}

// 记录最近一次收到服务端数据的时间（PONG、群聊、私聊等均算活跃）
void ChatClient::touchServerActivity()
{
    m_lastServerActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void ChatClient::startHeartbeat()
{
    touchServerActivity();
    m_heartbeatTimer.start();
    sendPacket(QStringLiteral("PING"));  // 进入聊天后立即发首包，不必等 60s
}

void ChatClient::stopHeartbeat()
{
    m_heartbeatTimer.stop();
    m_lastServerActivityMs = 0;
}

// 心跳定时器：先发 PING 探测；若超时未收到服务端任何回包则主动断开
void ChatClient::onHeartbeatTick()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        stopHeartbeat();
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastServerActivityMs > 0 && now - m_lastServerActivityMs > kHeartbeatTimeoutMs) {
        qDebug() << "心跳超时，断开连接";
        m_socket->abort();
        return;
    }

    sendPacket(QStringLiteral("PING"));  // 应用层心跳包，仍走 NetPacket 帧格式
}

void ChatClient::sendPacket(const QString &line)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    m_socket->write(NetPacket::encode(line));
}

bool ChatClient::isSelfContactEntry(const QString &username, const QString &nickname) const
{
    if (username.isEmpty()) {
        return false;
    }
    if (username == m_username) {
        return true;
    }
    // 兼容旧列表格式 username:nickname 写反、但仍指向本账号的情况
    if (!m_nickname.isEmpty() && username == m_nickname && nickname == m_username) {
        return true;
    }
    return false;
}

void ChatClient::dedupeSelfContactEntries()
{
    for (int i = ui->user_listWidget->count() - 1; i >= 0; --i) {
        QListWidgetItem *item = ui->user_listWidget->item(i);
        if (!item || item->data(Qt::UserRole + 1).toBool()) {
            continue;
        }
        const QString username = item->data(Qt::UserRole).toString();
        QString nickname;
        const QString text = item->text();
        if (text.contains(QLatin1Char('('))) {
            nickname = text.section(QLatin1Char('('), 0, 0).trimmed();
        }
        if (isSelfContactEntry(username, nickname)) {
            delete ui->user_listWidget->takeItem(i);
        }
    }
}

void ChatClient::ensureSelfContactItem()
{
    if (m_username.isEmpty()) {
        return;
    }
    for (int i = 0; i < ui->user_listWidget->count(); ++i) {
        QListWidgetItem *item = ui->user_listWidget->item(i);
        if (item && item->data(Qt::UserRole + 1).toBool()) {
            return;
        }
    }
    const QString displayNick = m_nickname.isEmpty() ? m_username : m_nickname;
    auto *selfItem = new QListWidgetItem(tr("发给自己 · %1").arg(displayNick));
    selfItem->setData(Qt::UserRole, m_username);
    selfItem->setData(Qt::UserRole + 1, true);
    ui->user_listWidget->insertItem(0, selfItem);
    dedupeSelfContactEntries();
}

void ChatClient::updateChatModeLabel()
{
    if (m_privateTargetUser.isEmpty()) {
        ui->chatMode_label->setText(tr("当前：群聊（全员可见）"));
    } else if (m_privateTargetUser == m_username) {
        ui->chatMode_label->setText(tr("当前：发给自己（仅自己可见，无回复）"));
    } else {
        QListWidgetItem *item = findContactItem(ui->user_listWidget, m_privateTargetUser);
        const bool targetOnline = !item || item->data(kRoleOnline).toBool();
        const QString nick = item ? item->data(kRoleNickname).toString() : QString();
        const QString display = nick.isEmpty() ? m_privateTargetUser : nick;
        if (targetOnline) {
            ui->chatMode_label->setText(tr("当前：私聊 → %1").arg(display));
        } else {
            ui->chatMode_label->setText(tr("当前：私聊 → %1（离线）").arg(display));
        }
    }
}

void ChatClient::on_user_listWidget_itemClicked()
{
    QListWidgetItem *item = ui->user_listWidget->currentItem();
    if (!item) {
        return;
    }

    const QString target = item->data(Qt::UserRole).toString();
    if (target.isEmpty()) {
        return;
    }

    if (m_privateTargetUser == target) {
        m_privateTargetUser.clear();
        ui->mes_textEdit->append(target == m_username
            ? tr("----- 已退出「发给自己」-----")
            : tr("----- 已切换为群聊 -----"));
    } else {
        m_privateTargetUser = target;
        if (target == m_username) {
            ui->mes_textEdit->append(tr("----- 已切换为发给自己（备忘，无回复）-----"));
        } else {
            ui->mes_textEdit->append(tr("----- 已与 %1 建立私聊 -----").arg(target));
        }
    }
    updateChatModeLabel();
}

void ChatClient::on_send_button_clicked()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, tr("提示"), tr("未连接到服务器，请重新登录"));
        return;
    }

    const QString text = ui->send_lineEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    ui->send_lineEdit->clear();

    if (!m_privateTargetUser.isEmpty()) {
        if (m_privateTargetUser == m_username) {
            ui->mes_textEdit->append(tr("[备忘] %1").arg(text));
        } else {
            ui->mes_textEdit->append(tr("我：") + text);
        }
        sendPacket(QString("PRIVATE|%1|%2").arg(m_privateTargetUser, text));
    } else {
        ui->mes_textEdit->append(tr("我：") + text);
        sendPacket(QString("CHAT|%1").arg(text));
    }
}

void ChatClient::appendChatLine(const QString &line)
{
    touchServerActivity();

    // PONG / 内部信令不在聊天区展示
    if (line == QLatin1String("PONG") || line.startsWith(QLatin1String("PONG|"))
        || line == QLatin1String("PING") || line.startsWith(QLatin1String("PING|"))
        || line == QLatin1String("FETCH_OFFLINE")
        || line.startsWith(QLatin1String("FETCH_OFFLINE|"))) {
        return;
    }

    if (line.startsWith("CHAT|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString content = line.section('|', 2);
        if (content == QLatin1String("FETCH_OFFLINE")
            || content == QLatin1String("PING")) {
            return;
        }
        if (!nickname.isEmpty()) {
            const QString displayName = (!m_nickname.isEmpty() && nickname == m_nickname)
                ? tr("我")
                : nickname;
            ui->mes_textEdit->append(displayName + tr("：") + content);
            return;
        }
    }

    if (line.startsWith("PRIVATE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString fromUser = line.section('|', 2, 2);
        const QString content = line.section('|', 3, -1);
        if (fromUser == m_username) {
            return;
        }
        appendPrivateChatLine(nickname, fromUser, content, false);
        return;
    }

    if (line.startsWith("OFFLINE_BEGIN|")
        || line.startsWith("OFFLINE_DONE|")) {
        return;
    }

    if (line.startsWith("OFFLINE_PRIVATE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString fromUser = line.section('|', 2, 2);
        const QString sentAt = line.section('|', -1);
        const QString content = line.section('|', 3, -2);
        appendPrivateChatLine(nickname, fromUser, content, true, sentAt);
        return;
    }

    if (line.startsWith("SYS|")) {
        ui->mes_textEdit->append(tr("[系统] %1").arg(line.section('|', 1)));
        return;
    }

    if (line.startsWith("USER_ONLINE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString username = line.section('|', 2, 2);
        if (!username.isEmpty() && !isSelfContactEntry(username, nickname)) {
            upsertContactItem(ui->user_listWidget, username, nickname, true);
            dedupeSelfContactEntries();
            ui->mes_textEdit->append(tr("[上线] %1").arg(nickname));
        }
        return;
    }

    if (line.startsWith("USER_OFFLINE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString username = line.section('|', 2, 2);
        ui->mes_textEdit->append(tr("[离线] %1").arg(nickname));
        QListWidgetItem *item = findContactItem(ui->user_listWidget, username);
        if (item && !item->data(kRoleSelfContact).toBool()) {
            const QString nick = item->data(kRoleNickname).toString();
            item->setData(kRoleOnline, false);
            item->setText(formatContactLabel(nick.isEmpty() ? nickname : nick, username, false));
        }
        if (!m_privateTargetUser.isEmpty() && m_privateTargetUser == username) {
            ui->mes_textEdit->append(tr("----- 对方已离线，可继续发私聊，上线后送达 -----"));
            updateChatModeLabel();
        }
        return;
    }

    if (line.startsWith("ONLINE_LIST|")) {
        ui->user_listWidget->clear();
        const QStringList entries = line.section('|', 1).split(';', Qt::SkipEmptyParts);
        for (const QString &entry : entries) {
            QString username;
            QString nickname;
            if (entry.contains('|')) {
                username = entry.section('|', 0, 0);
                nickname = entry.section('|', 1);
            } else if (entry.contains(QLatin1Char(':'))) {
                username = entry.section(QLatin1Char(':'), 0, 0);
                nickname = entry.section(QLatin1Char(':'), 1);
            } else {
                username = entry;
            }
            if (username.isEmpty() || isSelfContactEntry(username, nickname)) {
                continue;
            }
            bool exists = false;
            for (int i = 0; i < ui->user_listWidget->count(); ++i) {
                if (ui->user_listWidget->item(i)->data(Qt::UserRole).toString() == username) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                continue;
            }
            upsertContactItem(ui->user_listWidget, username, nickname, true);
        }
        ensureSelfContactItem();
        return;
    }

    ui->mes_textEdit->append(line);
}

void ChatClient::appendPrivateChatLine(const QString &nickname,
                                       const QString &fromUser,
                                       const QString &content,
                                       bool offline,
                                       const QString &sentAt)
{
    if (fromUser == m_username) {
        return;
    }

    const QString displayName = nickname.isEmpty() ? fromUser : nickname;
    const QString body = displayName + tr("：") + content;

    if (!offline) {
        ui->mes_textEdit->append(body);
        return;
    }

    QString timeLabel;
    if (!sentAt.isEmpty()) {
        QDateTime dt = QDateTime::fromString(sentAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!dt.isValid()) {
            dt = QDateTime::fromString(sentAt, Qt::ISODate);
        }
        if (dt.isValid()) {
            timeLabel = dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        } else {
            timeLabel = sentAt;
        }
    }

    if (timeLabel.isEmpty()) {
        ui->mes_textEdit->append(body);
        return;
    }

    const QString html = QStringLiteral(
        "<div style=\"margin-top:4px;\">"
        "<span style=\"font-size:8pt; color:#888888;\">%1</span><br/>%2"
        "</div>")
                             .arg(QString(timeLabel).toHtmlEscaped(),
                                  QString(body).toHtmlEscaped());
    ui->mes_textEdit->append(html);
}

void ChatClient::readServerData()
{
    if (!m_socket) {
        return;
    }

    QStringList lines;
    NetPacket::feed(m_recvBuffer, m_socket->readAll(), lines);
    for (const QString &rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (!trimmed.isEmpty()) {
            appendChatLine(trimmed);
        }
    }
}

void ChatClient::on_disconnect()
{
    stopHeartbeat();  // 连接已断，停止发送 PING
    ui->mes_textEdit->append(tr("===== 已与服务器断开，请关闭后重新登录 ====="));
    m_recvBuffer.clear();
    QMessageBox::warning(this, tr("连接断开"), tr("与服务器连接已断开"));
}
