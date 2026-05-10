#include"dbhelper.h"
//私有函数的构造
Dbhelper::Dbhelper(QObject*parent):QObject(parent){

}
Dbhelper & Dbhelper::getInstance(){
    static Dbhelper ins;
    return ins;
}
bool Dbhelper::connectMySql(){
    db=QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("localhost");
    db.setPort(3306);
    db.setDatabaseName("chat_room");
    db.setUserName("root");
    db.setPassword("663399QQ");
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_SSL_MODE=SSL_MODE_DISABLED");
    if(!db.open()){
        qDebug()<<"数据库连接失败:"<<db.lastError().text();
        return false;
    }
    qDebug()<<"数据库连接成功";
    return true;
}

bool Dbhelper::execSql(const QString&sql){
    QSqlQuery query;
    if(!query.exec(sql)){
        qDebug()<<"SQL执行失败："<<query.lastError().text();
        return false;
    }
    qDebug()<<"SQL执行成功";
    return true;
}

QSqlQuery Dbhelper::querySql(const QString&sql){
    QSqlQuery query;
    query.exec(sql);
    return query;
}