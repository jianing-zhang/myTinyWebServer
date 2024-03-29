#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;


// 单例模式
//连接池就是一个池子，要就拿出来用，用完记得放回去，为了防止拿出用完没有放回去，所以要使用RAII机制
class connection_pool{
public:
    MYSQL *GetConnection();                 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    //释放连接
    int GetFreeConn();                      //获取链接
    void DestroyPool();                     //销毁所有链接


    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DatabaseName, int Port, int MaxConn, int close_log);
    
private:
    connection_pool();
    ~connection_pool();
    
    int m_MaxConn; //最大连接数
    int m_CurConn; //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数
    locker lock;
    list<MYSQL *> connList; //连接池
    sem reserve;

public:
    string m_url; //主机地址
    string m_Port; //数据库端口号
    string m_User; //登陆数据库用的用户名
    string m_PassWord; //登陆数据库的密码
    string m_DatabaseName; //使用数据库名
    int m_close_log;    //日志开关

};


//使用RAII机制，获取连接池中的连接，自动管理连接的释放
//但此处的RAII机制，写得不好，具体看实现的注释
class connectionRAII{
public:
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};



#endif