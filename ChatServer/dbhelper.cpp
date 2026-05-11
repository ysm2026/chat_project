#include"dbhelper.h"
//Md5加密
QString Dbhelper::md5Password(const QString &password_hash){
    QByteArray bt=password_hash.toUtf8();
    QByteArray res=QCryptographicHash::hash(bt,QCryptographicHash::Md5);
    return res.toHex();
}

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

/*=====================1、用户注册===================*/
bool Dbhelper::registerUser(const QString &username,
                            const QString &password_hash,
                            const QString &nickname,
                            const QString &email,
                            const QString &phone){
    QString pwd=md5Password(password_hash);
    QString sql=QString("insert int user(username,password_hash,nickname,email,phone)valuse"
                          "(:username, :password_hash, :nickname, :email, :phone)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":username",username);
    query.bindValue(":passwor_hash",pwd);
    query.bindValue(":nickname",nickname);
    query.bindValue(":email",email);
    query.bindValue(":phone",phone);
    if(!query.exec()){
        qDebug()<<"注册失败:"<<query.lastError().text();
        return false;
    }
    return true;
}
/*=====================2、用户登录===================*/
bool Dbhelper::loging(const QString &username,const QString &password_hash,QString &nickname){
    QString pwd=md5Password(password_hash);
    QString sql=QString("select id,nickname from user"
                          "where username=:username and password_hash=:password_hash and status=1");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":username",username);
    query.bindValue(":password_hash",pwd);
    if(!query.exec()){
        qDebug()<<"登录失败查询："<<query.lastError().text();
        nickname.clear();
        return false;
    }
    if(query.next()){
        nickname=query.value("nickname").toString();
        return true;
    }
    nickname.clear();
    return false;
}
/*=====================3、创建聊天室===================*/
bool Dbhelper::create_room(const QString &room_name,int room_type,int create_id){
    QString sql("insert into chat_room(room_name,room_type,create_id)"
                "values(:room_name, :room_type, :create_id)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":room_name",room_name);
    query.bindValue(":room_type",room_type);
    query.bindValue(":create_id",create_id);
    if(!query.exec()){
        qDebug()<<"房间创建失败"<<query.lastError().text();
        return false;
    }
    QSqlQuery q=querySql("select last_insert_id()");
    if(q.next()){
        return q.value(0).toInt();
    }
    return false;
}