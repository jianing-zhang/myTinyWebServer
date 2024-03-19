#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "../log/log.h"
#include <sys/epoll.h>


class util_timer; 

//定时器中记录的连接资源,包含客户端地址，套接字信息
struct client_data{
    sockaddr_in address;    
    int sockfd;
    util_timer *timer;
};

// 定时器类，相当于双向链表的节点，储存着单个定时器信息，前后节点指针
class util_timer{
public:
    util_timer(): prev(NULL), next(NULL) {}


    time_t expire;  //定时器超时时间
    void(* cb_func)(client_data *);     //定时器回调函数，删除定时器的注册事件和套接字连接
    client_data *user_data;             //链接资源
    util_timer *prev;                   //前向定时器
    util_timer *next;                   //后继定时器
};


class sort_timer_lst{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    //将目标定时器添加到链表中，添加时按照升序添加，如果在头部插入，或者是链表一开始是空的，就直接处理，如果非头部插入，非空，调用重载add_timer
    void add_timer(util_timer *timer);
    //当定时任务发生变化,调整对应定时器在链表中的位置
    void adjust_timer(util_timer *timer);
    //函数将超时的定时器从链表中删除
    void del_timer(util_timer *timer);
    //定时任务处理函数
    void tick();

private:
    // 重载add_timer,在上面的add_timer调用这个函数来处理非头节点，非空列表插入的情况
    void add_timer(util_timer *timer, util_timer *lst_head);

    //双向链表头尾节点
    util_timer *head;
    util_timer *tail;
};

//工具类，用于统一事件源，捕获定时事件后通过pipe发送到epoll，然后epoll端调用sort_timer_lst::tick()函数清理过期定时器
//程序刚开始就调用alarm产生SIGALRM信号和addfd()函数去注册epoll监听事件，addsig函数调用sigaction函数去捕获这个信号
//如果捕获到了，sig_handle调用向通道写入信息,此时addfd()函数注册的epoll监听事件收到消息后（统一处理信号），调用timer_handle函数，
//timer_handle使用了sort_timer_lst.tick()和alarm()函数，sort_timer_lst.tick()去处理超时连接，alarm则继续去在指定的时间后产生SIGALRM信号
class Utils{
public:
    Utils() = default;
    ~Utils() = default;

    void init(int timeslot);

    //文件描述符设置非阻塞
    int setnonblocking(int fd);

    //向内核事件表注册事件，可选择ET/LT，同时可开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
    
    //信号处理函数,向通道写入信息
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(*handler)(int), bool restart = true);

    //定时处理任务，处理超时连接和重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char* info);


    static int* u_pipefd;   //管道描述符指针
    sort_timer_lst m_timer_lst;
    //定时器超时的epoll监听描述符，其实整个程序就一个epoll，这里使用的就是那个服务端监听的那个epoll，统一事件源的基础
    static int u_epollfd;   
    int m_TIMESLOT;     //alarm触发的时间
};


//定时器回掉函数，删除定时器的注册事件和套接字连接
void cb_func(client_data *user_data);




#endif