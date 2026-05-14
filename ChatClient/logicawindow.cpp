#include "logicawindow.h"
#include "ui_logicawindow.h"

LogicaWindow::LogicaWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogicaWindow)
{
    ui->setupUi(this);
    QPixmap pix("D:/Qt/project/chat/ChatClient/微信图片_20260312091244_6_115.png");
    ui->label->setPixmap(pix);
    connect(ui->logica_pushButton,&QPushButton::clicked,this,&LogicaWindow::switchRegister);
}

LogicaWindow::~LogicaWindow()
{
    delete ui;
}
