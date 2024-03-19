#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        head = head->next;
        delete tmp;
        tmp = head;
    }
}

// 将目标定时器添加到链表中，添加时按照升序添加，如果在头部插入，或者是链表一开始是空的，就直接处理，如果非头部插入，非空，调用重载add_timer
// 其实Leetcode中就有过，一个很优雅的写法。创建一个dummy头节点，使其指向head，这样子，就不用判断列表是否在头部插入的情况。但是是否为空还是需要判断
// 但这里应该是别处需要用到，分开来写
void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer)
        return;
    if(!head){
        head=tail=timer;

        //修改，原本没有
        timer->prev = NULL;
        timer->next = NULL;
        return;
    }
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;

        //修改，原本没有
        timer->prev = NULL;
        return;
    }
    add_timer(timer,head);
}

//重载add_timer,处理非头节点，非空列表插入的情况
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    
    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while(tmp){
        if(timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = prev;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

// 调整定时器，任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer)
        return;

    util_timer *tmp = timer->next;

    //被调整的定时器在链表尾部或者定时器超时值仍然小于下一个定时器超时值，不调整
    if(!tmp || (timer->expire < tmp->expire))
        return;
    
    //被调整定时器是链表头节点，将定时器取出，重新插入
    if(timer==head){
        //修改原本是
        /* head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head); */
        head = head->next;
        if(head) head->prev = NULL;
        add_timer(timer);        
    }

    //被调整的定时器在内部，将定时器取出，重新插入
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        //其实这里不应该是这样子添加，因为不能确保，当前expire的时间是比前面的大
        //但在这里实现没问题完全是因为，expire每次都是当前时间+T，每个定时器加的T都一样
        //所以，这里可以正常运行
        //修改，原本是add_timer(timer, timer->next);
        add_timer(timer);  
    }
}

void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer)
        return;
    if((timer==head)&&(timer==tail)){
        delete timer;
        head = tail = NULL;
        return;
    }
    if(timer==head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer==tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//定时任务处理函数
void sort_timer_lst::tick(){
    if(!head)
        return;
    //获取当前时间
    time_t cur = time(NULL);

    //循环删除超时的连接    
    while(head){
        if(cur < head->expire)
            break;
        auto tmp = head;
        head = head->next;
        tmp->cb_func(tmp->user_data);
        delete tmp;
        if(head){
            head->prev = NULL;
        }
        //
        // int m_close_log=0;
        // LOG_INFO("%s\n","tick()");
    }
}

void Utils::init(int timeslot){
    m_TIMESLOT = timeslot;
}

//对指定文件描述符设置非阻塞
int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}


//向内核事件表注册事件，可选择ET/LT，同时可开启EPOLLONESHOT
void Utils::addfd(int epollfd,int fd, bool one_shot, int TRIGMode){
    epoll_event event;

    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; //可读，边缘触发，
    else 
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig){
    //为保证函数的可重入性，保留原来的errno
    int save_error = errno;
    int msg = sig;
    send(u_pipefd[1],(char *)&msg, 1, 0);
    errno = save_error;
}

//设置信号函数
void Utils::addsig(int sig, void(*handler)(int),bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    //将所有信号都设置为阻塞状态，以确保在信号处理函数执行期间阻塞所有其他信号，不会被其他信号中断
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL)!=-1);
}


//定时处理任务，处理超时连接和重新定时以不断触发SIGALRM信号
void Utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}


int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;

//定时器回掉函数，删除定时器的注册事件和套接字连接
void cb_func(client_data *user_data){
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    //关闭文件描述符
    close(user_data->sockfd);
    //减少连接数
    http_conn::m_user_count--;
}