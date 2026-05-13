#include "chatclient.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ChatClient w;
    w.show();
    return QCoreApplication::exec();
}
