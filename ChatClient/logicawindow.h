#ifndef LOGICAWINDOW_H
#define LOGICAWINDOW_H

#include <QWidget>
#include <QTcpSocket>

namespace Ui {
class LogicaWindow;
}

class LogicaWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LogicaWindow(QWidget *parent = nullptr);
    ~LogicaWindow();

private:
    Ui::LogicaWindow *ui;
    QTcpSocket *socket;

signals:
    void switchRegister();
    // void loginSuccess();

// private slots:
//     // 注册、登录按钮槽函数
//     void on_btnReg_clicked();
//     void on_btnLogin_clicked();
//     // 接收服务器返回数据
//     void readLoginData();
//     // 网络错误
//     void socketError();
};

#endif // LOGICAWINDOW_H
