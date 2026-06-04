#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QWidget>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>

namespace Ui {
class registerwindow;
}

class registerwindow : public QWidget
{
    Q_OBJECT

public:
    explicit registerwindow(QWidget *parent = nullptr);
    ~registerwindow();

    void setTcpSocket(QTcpSocket *socket);
    void resetForm();
    void detachSocketHandlers();

signals:
    void registerSuccess();
    void registerCancel();

private slots:
    void on_true_pushButton_clicked();
    void on_casual_pushButton_clicked();
    void onSocketConnected();
    void readRegisterData();
    void onSocketError();
    void onRegisterTimeout();

private:
    void sendRegisterRequest();
    void sendPacket(const QString &line);
    void setRegisterEnable(bool enabled);
    void showError(const QString &msg);
    void clearError();
    void finishRegisterRequest();

    Ui::registerwindow *ui;
    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;
    QTimer m_registerTimeout;
    bool m_pendingRegister;
    bool m_waitingRegisterResponse;
    QString m_username;
    QString m_password;
    QString m_nickname;
};

#endif // REGISTERWINDOW_H
