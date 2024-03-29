#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"



const int MAX_FD = 65536;        //最大文件描述符
const int MAX_EVENT_NUMBER = 10000;     //最大事件数
const int TIMESLOT = 5;         //最小超时单位

class WebServer{
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string passWord, string databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    //初始化线程池
    void thread_pool();
    //初始化数据库池
    void sql_pool();
    //日志初始化
    void log_write();
    //触发模式初始化 LT/ET
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    // 删除当前文件描述符定时器和当前的文件描述符的注册事件，和关闭套接字连接
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclinetdata();

    // 统一信号源的处理，使用管道（pipe）通迅，在别的函数中，系统接收到sigaction后
    // 对应的处理函数将会通过管道传递，本质上pipe接口也是文件描述符，所以可以用epoll监听pipe文件描述符
    // 使得统一起来，使用epoll来监听，统一处理
    // 目的是为了定时处理超时连接
    bool dealwithsignal(bool& timeout, bool& stop_server);

    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;        //是否异步写日志
    int m_close_log;        //是否开启日志
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //数据库相关
    connection_pool *m_connPool;
    string m_user;      //登陆数据库用户名
    string m_passWord;  //登陆数据库密码
    string m_databaseName;  //使用数据库名
    int m_sql_num;
    
    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;                 //触发模式
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    client_data *users_timer;
    Utils utils;
};

#endif