#include"dbhelper.h"
#include "dbconfig.h"
#include <QRegularExpression>

namespace {

bool isMd5Hex(const QString &input)
{
    static const QRegularExpression kMd5Pattern("^[A-Fa-f0-9]{32}$");
    return kMd5Pattern.match(input).hasMatch();
}

QString normalizeIncomingPassword(const QString &input)
{
    if (isMd5Hex(input)) {
        return input.toLower();
    }
    return Dbhelper::md5Password(input);
}
}

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
    db.setHostName(DbConfig::kHost);
    db.setPort(DbConfig::kPort);
    db.setDatabaseName(DbConfig::kDatabaseName);
    db.setUserName(DbConfig::kUser);
    db.setPassword(DbConfig::kPassword);

    const QString sslMode = QString::fromUtf8(DbConfig::kSslMode).trimmed();
    if (!sslMode.isEmpty()) {
        db.setConnectOptions("MYSQL_OPT_SSL_MODE=" + sslMode);
    }

    if (db.password().isEmpty() || QString(DbConfig::kPassword).isEmpty()) {
        qDebug() << "数据库连接失败: 请在 ChatServer/dbconfig.h 中设置 kPassword";
        return false;
    }

    if(!db.open()){
        qDebug()<<"数据库连接失败:"<<db.lastError().text();
        return false;
    }
    qDebug()<<"数据库连接成功";
    return true;
}

bool Dbhelper::execSql(const QString&sql){
    QSqlQuery query(db);
    if(!query.exec(sql)){
        qDebug()<<"SQL执行失败："<<query.lastError().text();
        return false;
    }
    qDebug()<<"SQL执行成功";
    return true;
}

QSqlQuery Dbhelper::querySql(const QString&sql){
    QSqlQuery query(db);
    query.exec(sql);
    return query;
}

/*=====================0、检查用户名是否存在===================*/
bool Dbhelper::userExists(const QString &username){
    QSqlQuery query(db);
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
    const QString pwd = normalizeIncomingPassword(password_hash);
    QString sql=QString("insert into user(username,password_hash,nickname,email,phone,status)values"
                          "(:username, :password_hash, :nickname, :email, :phone, 1)");
    QSqlQuery query(db);
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
bool Dbhelper::loging(const QString &username,const QString &password_hash,QString &nickname,int *userId){
    const QString pwd = normalizeIncomingPassword(password_hash);
    QString sql=QString("select id,nickname from user where username=:username and password_hash=:password_hash and status=1");
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":username",username);
    query.bindValue(":password_hash",pwd);
    if(!query.exec()){
        qDebug()<<"登陆失败,登录失败查询："<<query.lastError().text();
        nickname.clear();
        if (userId) {
            *userId = 0;
        }
        return false;
    }
    if(query.next()){
        if (userId) {
            *userId = query.value("id").toInt();
        }
        nickname=query.value("nickname").toString();
        return true;
    }
    nickname.clear();
    if (userId) {
        *userId = 0;
    }
    return false;
}

int Dbhelper::getUserIdByUsername(const QString &username)
{
    QSqlQuery query(db);
    query.prepare("select id from user where username=:username and status=1");
    query.bindValue(":username", username);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}
/*=====================3、创建聊天室===================*/
int Dbhelper::create_room(const QString &room_name,int room_type,int create_id){
    QString sql("insert into chat_room(room_name,room_type,create_id)"
                "values(:room_name, :room_type, :create_id)");
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":room_name",room_name);
    query.bindValue(":room_type",room_type);
    query.bindValue(":create_id",create_id);
    if(!query.exec()){
        qDebug()<<"房间创建失败"<<query.lastError().text();
        return 0;
    }
    QSqlQuery q(db);
    if(q.exec("select last_insert_id()") && q.next()){
        return q.value(0).toInt();
    }
    return 0;
}
/*=====================4、用户加入聊天室===================*/
bool Dbhelper::join_room(int room_id,int user_id){
    QString sql("insert into room_number(room_id,user_id)values(:room_id, :user_id)");
    QSqlQuery query(db);
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
    QSqlQuery query(db);
    query.prepare("insert into message(room_id,send_id,receiver_id,content,status,message_type,delivery_status)"
                  " values(:room_id, :send_id, NULL, :content, 1, 1, 1)");
    query.bindValue(":room_id",room_id);
    query.bindValue(":send_id",send_id);
    query.bindValue(":content",content);
    if(!query.exec()){
        qDebug()<<"群聊消息入库失败:"<<query.lastError().text();
        return false;
    }
    return true;
}
/*=====================6、保留私聊信息==================*/
int Dbhelper::savePrivate_Message(int send_id,int receiver_id,const QString &content,int delivery_status){
    QSqlQuery query(db);
    query.prepare("insert into message(receiver_id,send_id,room_id,content,status,message_type,delivery_status)"
                  " values(:receiver_id, :send_id, 0, :content, 1, 1, :delivery_status)");
    query.bindValue(":receiver_id",receiver_id);
    query.bindValue(":send_id",send_id);
    query.bindValue(":content",content);
    query.bindValue(":delivery_status",delivery_status);
    if(!query.exec()){
        qDebug() << "私聊消息入库失败:" << query.lastError().text()
                 << "（若提示 delivery_status 不存在，请执行 ChatServer/migrate_offline.sql）";
        return 0;
    }
    return query.lastInsertId().toInt();
}

QList<PendingPrivateMessage> Dbhelper::fetchUndeliveredPrivateMessages(int receiverId)
{
    QList<PendingPrivateMessage> result;
    if (receiverId <= 0) {
        return result;
    }

    QSqlQuery query(db);
    query.prepare(
        "select m.id, m.content, m.send_time, u.username, u.nickname "
        "from message m "
        "inner join user u on u.id = m.send_id "
        "where m.receiver_id = :receiver_id and m.room_id = 0 "
        "and m.message_type = 1 and m.status = 1 and m.delivery_status = 0 "
        "order by m.id asc");
    query.bindValue(":receiver_id", receiverId);
    if (!query.exec()) {
        qDebug() << "查询离线私聊失败:" << query.lastError().text()
                 << "（请确认已执行 migrate_offline.sql 且存在 delivery_status 字段）";
        return result;
    }

    while (query.next()) {
        PendingPrivateMessage item;
        item.id = query.value("id").toInt();
        item.fromUsername = query.value("username").toString();
        item.fromNickname = query.value("nickname").toString();
        item.content = query.value("content").toString();
        item.createdAt = query.value("send_time").toDateTime();
        if (item.fromNickname.isEmpty()) {
            item.fromNickname = item.fromUsername;
        }
        result.append(item);
    }
    return result;
}

bool Dbhelper::markPrivateMessagesDelivered(const QList<int> &messageIds)
{
    if (messageIds.isEmpty()) {
        return true;
    }

    QStringList placeholders;
    for (int i = 0; i < messageIds.size(); ++i) {
        placeholders.append(QString(":id%1").arg(i));
    }

    QSqlQuery query(db);
    query.prepare(QString(
        "update message set delivery_status = 1 "
        "where id in (%1) and room_id = 0 and message_type = 1")
                      .arg(placeholders.join(',')));
    for (int i = 0; i < messageIds.size(); ++i) {
        query.bindValue(QString(":id%1").arg(i), messageIds.at(i));
    }
    if (!query.exec()) {
        qDebug() << "标记私聊已投递失败:" << query.lastError().text();
        return false;
    }
    return true;
}