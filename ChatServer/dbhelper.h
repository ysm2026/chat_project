#ifndef DBHELPER_H
#define DBHELPER_H

#include<QObject>       //Qt基类
#include<QSqlDatabase>  //数据库连接类,处理打开/关闭、配置连接参数
#include<QSqlQuery>     //执行Sql语句
#include<QSqlError>    //获取错误信息，用于调试和错误处理
#include<QDebug>       //控制台打印错误信息

class Dbhelper:public QObject{
    Q_OBJECT    //Qt信号槽必备宏
public:
    static Dbhelper&getInstance(); //全局只有一个数据库连接对象，这个项目共用
    bool connectMySql();            //连接数据库，成功返回true，失败返回false
    bool execSql(const QString&sql);//执行增删改操，无结果返回
    QSqlQuery querySql(const QString&sql);  //执行查找操作，没有结果返回
private:
    explicit Dbhelper(QObject*parent=nullptr);//私有构造函数，防止外部创建对象，保证单例
    QSqlDatabase db;        //存储数据库连接实例
};

#endif // DBHELPER_H
