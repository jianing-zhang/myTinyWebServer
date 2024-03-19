#ifndef LOG_H
#define LOG_H


#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

// 单例模式的log，懒汉模式
class Log{
public:
    //C++11之后，内部静态变量保证是线程安全的
    static Log* get_instance(){
        //在静态成员函数中创建对象实例，这样子可以调用非静态成员函数
        //同时，该对象实例是static，因此确保只有一个实例，且是会被保证是线程安全的
        //静态成员函数调用非静态成员/函数，可以通过形参传递对象实例，或者在静态成员函数中创建一个实例，相当于弥补了没有this指针
        static Log instance; 
        return &instance;
    }

    //调用线程去异步写数据，只要block_queue中的消费者能运行（即队列中有元素存在，若无，pop就会一直等待，那这个线程也会一直等待），就写入日志
    static void *flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }

    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char* file_name, int close_log, int log_buf_size=8192, int split_lines = 5000000, int max_queue_size = 0);

    //将输出内容按照标准格式整理写入日志
    void write_log(int level, const char *fromat, ...);

    // 强制写入缓冲区，刷新缓冲区
    void flush(void);

private:
    Log();      
    virtual ~Log();

    //异步写日志，从阻塞队列中取出一个日志string，将其转化为原生字符串，写日志文件中
    void *async_write_log(){
        string single_log;
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; //log文件路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小；
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类，记录当前写log文件的时间是哪一天
    FILE *m_fp;         //打开log的文件指针
    char *m_buf;        //日志缓冲区 
    block_queue<string> *m_log_queue;   //阻塞队列  
    bool m_is_async;    //是否异步标志
    locker m_mutex;     //写日志的时候需要加锁，因为大概率是多用户使用
    int m_close_log;    //关闭日志
};

//调用下列这些的函数或者类，必须确保有m_close_log成员，因为下面宏定义在预编译的时候会被替换成后面的代码，如果m_close_log没有定义，会导致bug
#define LOG_DEBUG(fromat, ...) if(m_close_log==0) {Log::get_instance()->write_log(0, fromat, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(m_close_log==0) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(m_close_log==0) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(m_close_log==0) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif