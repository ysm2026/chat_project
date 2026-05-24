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
    db.setConnectOptions("MYSQL_OPT_SSL_MODE=0");
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

/*=====================0、检查用户名是否存在===================*/
bool Dbhelper::userExists(const QString &username){
    QSqlQuery query;
    query.prepare("select id from user where username=:username");
    query.bindValue(":username",username);
    if(!query.exec()){
        qDebug()<<"查询用户失败:"<<query.lastError().text();
        return false;
    }
    return query.next();
}

/*=====================1、用户注册===================*/
bool Dbhelper::registerUser(const QString &username,
                            const QString &password_hash,
                            const QString &nickname,
                            const QString &email,
                            const QString &phone){
    QString pwd=md5Password(password_hash);
    QString sql=QString("insert into user(username,password_hash,nickname,email,phone)values"
                          "(:username, :password_hash, :nickname, :email, :phone)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":username",username);
    query.bindValue(":password_hash",pwd);
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
    QString sql=QString("select id,nickname from user where username=:username and password_hash=:password_hash and status=1");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":username",username);
    query.bindValue(":password_hash",pwd);
    if(!query.exec()){
        qDebug()<<"登陆失败,登录失败查询："<<query.lastError().text();
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
/*=====================4、用户加入聊天室===================*/
bool Dbhelper::join_room(int room_id,int user_id){
    QString sql("insert into room_number(room_id,user_id)values(:room_id, :user_name)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":room_id",room_id);
    query.bindValue(":user_id",user_id);
    if(!query.exec()){
        qDebug()<<"加入房间失败（room_id:"<<room_id
                 <<",user_id："<<user_id<<"）"<<"错误信息："<<query.lastError().text();
        return false;
    }
    qDebug()<<"用户"<<user_id<<"加入房间"<<room_id<<"成功";
    return true;
}
/*=====================5、保留群聊信息==================*/
bool Dbhelper::saveRoom_Message(int send_id,int room_id,const QString &content){
    QString sql("insert into message(room_id,send_id,content,status,message_type)values"
                "(:room_id, :send_id, :content,1,1)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":room_id",room_id);
    query.bindValue("send_id",send_id);
    query.bindValue(":content",content);
    return execSql(sql);
}
/*=====================5、保留私聊信息==================*/
bool Dbhelper::savePrivate_Message(int send_id,int reciver_id,const QString &content){
    QString sql("insert into message(reciver_id,send_id,room_id,content,status,message_type)values"
                "(:reciver_id, :send_id,0, :content,1,1)");
    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":reciver_id",reciver_id);
    query.bindValue("send_id",send_id);
    query.bindValue(":content",content);
    return execSql(sql);
}