#include "chatclient.h"
#include "./ui_chatclient.h"

#include <QMessageBox>
#include <QListWidgetItem>

#include "../common/netpacket.h"

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
    , m_socket(nullptr)
{
    ui->setupUi(this);
    ui->mes_textEdit->setReadOnly(true);
    ui->user_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    updateChatModeLabel();
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

void ChatClient::setTcpSocket(QTcpSocket *socket, const QStringList &pendingServerLines)
{
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
    }
    m_socket = socket;
    m_recvBuffer.clear();
    m_privateTargetUser.clear();
    ui->user_listWidget->clear();
    updateChatModeLabel();

    if (m_socket) {
        connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::readServerData);
        connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::on_disconnect);
        ui->mes_textEdit->append(tr("===== 欢迎 %1，已连接服务器 =====").arg(m_nickname));

        ensureSelfContactItem();

        for (const QString &line : pendingServerLines) {
            appendChatLine(line);
        }
        if (ui->user_listWidget->count() == 0 && m_socket->bytesAvailable() > 0) {
            readServerData();
        }
    }
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
        ui->chatMode_label->setText(tr("当前：私聊 → %1").arg(m_privateTargetUser));
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

    const QString sender = m_nickname.isEmpty() ? tr("我") : m_nickname;
    ui->send_lineEdit->clear();

    if (!m_privateTargetUser.isEmpty()) {
        if (m_privateTargetUser == m_username) {
            ui->mes_textEdit->append(tr("[备忘] %1").arg(text));
        }
        sendPacket(QString("PRIVATE|%1|%2").arg(m_privateTargetUser, text));
    } else {
        ui->mes_textEdit->append(sender + tr("：") + text);
        sendPacket(QString("CHAT|%1").arg(text));
    }
}

void ChatClient::appendChatLine(const QString &line)
{
    if (line.startsWith("CHAT|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString content = line.section('|', 2);
        if (!nickname.isEmpty()) {
            ui->mes_textEdit->append(nickname + tr("：") + content);
            return;
        }
    }

    if (line.startsWith("PRIVATE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString fromUser = line.section('|', 2, 2);
        const QString content = line.section('|', 3);
        if (fromUser == m_username) {
            return;
        }
        ui->mes_textEdit->append(tr("[私聊]%1(%2)：%3").arg(nickname, fromUser, content));
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
            for (int i = 0; i < ui->user_listWidget->count(); ++i) {
                if (ui->user_listWidget->item(i)->data(Qt::UserRole).toString() == username) {
                    return;
                }
            }
            auto *item = new QListWidgetItem(QString("%1 (%2)").arg(nickname, username));
            item->setData(Qt::UserRole, username);
            ui->user_listWidget->addItem(item);
            dedupeSelfContactEntries();
            ui->mes_textEdit->append(tr("[上线] %1").arg(nickname));
        }
        return;
    }

    if (line.startsWith("USER_OFFLINE|")) {
        const QString nickname = line.section('|', 1, 1);
        const QString username = line.section('|', 2, 2);
        ui->mes_textEdit->append(tr("[离线] %1").arg(nickname));
        for (int i = ui->user_listWidget->count() - 1; i >= 0; --i) {
            QListWidgetItem *item = ui->user_listWidget->item(i);
            if (!username.isEmpty() && item->data(Qt::UserRole).toString() == username) {
                delete ui->user_listWidget->takeItem(i);
            }
        }
        if (!m_privateTargetUser.isEmpty() && m_privateTargetUser == username) {
            m_privateTargetUser.clear();
            ui->mes_textEdit->append(tr("----- 对方已离线，已切回群聊 -----"));
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
            const QString display = nickname.isEmpty()
                ? username
                : QString("%1 (%2)").arg(nickname, username);
            auto *item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, username);
            ui->user_listWidget->addItem(item);
        }
        ensureSelfContactItem();
        return;
    }

    ui->mes_textEdit->append(line);
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
    ui->mes_textEdit->append(tr("===== 已与服务器断开，请关闭后重新登录 ====="));
    m_recvBuffer.clear();
    QMessageBox::warning(this, tr("连接断开"), tr("与服务器连接已断开"));
}
