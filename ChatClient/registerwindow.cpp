#include "registerwindow.h"
#include "ui_registerwindow.h"

registerwindow::registerwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::registerwindow)
{
    ui->setupUi(this);
}

registerwindow::~registerwindow()
{
    delete ui;
}
