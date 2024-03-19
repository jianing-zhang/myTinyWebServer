#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}


//单例模式中的饿汉模式，参考log的解释
connection_pool *connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string PassWord, string DatabaseName, int Port, int MaxConn, int close_log){
    m_url = url;
    m_Port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DatabaseName;
    m_close_log = close_log;

    for(int i=0; i<MaxConn; i++){
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con==NULL){
            LOG_DEBUG("MySQL Error");
            exit(1);
        }
        con = mysql_real_connect(con,url.c_str(),User.c_str(),PassWord.c_str(),DatabaseName.c_str(),Port,NULL,0);


        if(con==NULL){
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }
    reserve = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection(){
    MYSQL *con = NULL;

    if(connList.size()==0)
        return NULL;

    reserve.wait();
    lock.lock();
    //获取连接池中的其中一个连接，然后拿出来
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

//释放当前连接
bool connection_pool::ReleaseConnection(MYSQL *con){

    if(con==NULL)
        return false;
    lock.lock();
    
    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    reserve.post();
    return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool(){
    lock.lock();

    if (connList.size() > 0){
        for(auto it = connList.begin(); it!=connList.end();++it){
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

//获取当前空闲的连接数
int connection_pool::GetFreeConn(){
    return m_FreeConn;
}

connection_pool::~connection_pool(){
    DestroyPool();
}

//使用RAII机制获取连接，绑定到第一个参数，第二个参数是连接池
//应该是要注意一下，第一个参数的生命周期应该要跟这个类实例一致，因为第一个参数保存着连接的地址
//ReleaseConnection函数只是把这连接的地址放回连接池中，并不是销毁，如果第一个参数生命周期跟类的实例不一致
//那这个连接一直存在，会导致下一次调用GetConnection的时候获取的连接地址，跟第一个参数是一样的，这样子就会很麻烦
//所以我觉得这个写法不太好，因为私有成员conRAII是这个连接，所以只需要，写一个函数接口，使用类的实例获取conRAII即可
//这样子就可以避免第一个参数的生命周期和类的实例生命周期不一致会导致的情况
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
    *SQL = connPool->GetConnection();
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}
