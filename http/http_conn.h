#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

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
#include <error.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>


#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{
public:
    //只有常量静态数据成员可以在类内初始化
    static const int FILENAME_LEN = 200;    //设置读取文件的名称m_real_file大小
    static const int READ_BUFFER_SIZE = 2048;   //设置读缓冲区m_read_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;  //设置写缓冲区m_write_buf大小

    //报文的请求方式，本项目只使用GET和POST
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    //主状态机的状态
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,    //解析请求行
        CHECK_STATE_HEADER,         //解析请求头
        CHECK_STATE_CONTENT         //解析消息体，仅用于解析POST请求
    };

    //报文解析的结果
    enum HTTP_CODE{
        NO_REQUEST,             //请求不完整，需要继续读取请求数据
        GET_REQUEST,            //完整的GET请求
        BAD_REQUEST,            //请求语法有误
        NO_RESOURCE,            //请求的资源不存在
        FORBIDDEN_REQUEST,      //请求被禁止访问
        FILE_REQUEST,           //请求资源的文件存在
        INTERNAL_ERROR,         //服务器内部错误
        CLOSED_CONNECTION       //客户端已关闭连接
    };

    //从状态机的状态
    enum LINE_STATUS{
        LINE_OK = 0,    //读取到一个完整的行
        LINE_BAD,       //报文语法错误
        LINE_OPEN       //读取的行不完整
    };

public:
    http_conn() = default;
    ~http_conn() = default;

    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,int close_log, string user, string passwd, string sqlname);
    //关闭http连接
    void close_conn(bool real_close = true);

    void process();
    //读取浏览器发来的全部数据
    bool read_once();
    //响应报文写入函数
    bool write();
    sockaddr_in *get_address(){
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);   // 初始化MySQL查询结果，从连接池中获取连接，并从数据库中获取用户名和密码数据
    // 用于Reactor模式，因为Reactor模式下，对于网络数据的读取还有写都是用工作线程，如果工作线程读取或者写出了问题，则需要打标记，用主线程处理
    int timer_flag;         //定时器标识，用于标识是否删除定时器,因为使用线程池的线程处理出了问题时，这个标志为1，删除定时器
    int improv;             //表示放入缓冲队列中的数据是否被线程池中的线程所处理，如果被处理了置为1；


private:
    void init();
    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);
    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char* text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //生成响应报文
    HTTP_CODE do_request();


    //m_start_line是已解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line(){
        return m_read_buf + m_start_line;
    }
    //从状态机读取一行,逐行解析
    LINE_STATUS parse_line();
    void unmap();

    //根据响应报文格式，生成对于8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();                  //添加长连接
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;                //读为0，写为1

private:
    int m_sockfd;           //客户端套接字描述符
    sockaddr_in m_address;
    //储存读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];
    //缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    long m_read_idx;
    //缓冲区m_read_buf中待解析但已经处理过的终点位置
    long m_checked_idx;
    //m_read_buf中已经解析过的字符个数
    int m_start_line;

    //存储应该发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    //指示m_write_buf中的长度
    int m_write_idx;

    //主状态机的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //解析报文中对应的6个变量
    char m_real_file[FILENAME_LEN];     //储存读取网页文件的路径
    char *m_url;                        //储存请求的URL
    char *m_version;                    //储存HTTP协议的版本号
    char *m_host;                       //储存主机名
    long  m_content_length;             //储存HTTP请求消息体的长度
    bool m_linger;                      //标识是否使用长连接

    char *m_file_address;               //读取服务器上的文件地址，被映射到内存中的地址
    struct stat m_file_stat;           //储存文件状态信息
    struct iovec m_iv[2];               //io向量机制iovec,用于writev()
    int m_iv_count;                     //io向量的数量
    int cgi;                            //是否启用的post
    char *m_string;                     //储存请求体的数据
    int bytes_to_send;                  //剩余发送字节数
    int bytes_have_send;                //已发送的字节数
    char *doc_root;                     //服务器根目录 

    map<string, string>m_users;         //储存用户名和密码的映射
    int m_TRIGMode;                     //触发模式，用于标识LT或者ET模式
    int m_close_log;                    //标识是否关闭日志记录

    char sql_user[100];                 //储存MySQL用户名
    char sql_passwd[100];               //储存MySQL密码
    char sql_name[100];                 //储存MySQL数据库名
};


#endif