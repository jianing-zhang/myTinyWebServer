#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
#include <string.h>

using namespace std;

Log::Log(){
    m_count = 0;
    m_is_async = false;
}

Log::~Log(){
    if(m_fp!=NULL){
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步需要设置为max_queue_size=0
//只能初始化一次，因为，pthread_create的存在，导致出现多次初始化会出现线程错乱的问题
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    
    if(max_queue_size >= 1){
        m_is_async = true;
        //创建并设置阻塞队列长度，block_queue_size
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    // 是否关闭日志标志
    m_close_log = close_log;
    //输出内容的长度
    m_log_buf_size = log_buf_size; 
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    //日志的最大行数
    m_split_lines = split_lines;

    //下面的操作都是为日志文件根据当前时间取名字

    //获取当前时间
    time_t t = time(NULL);

    //这里localtime返回了一个tm指针，空间是由localtime自己控制的，所以如果 连续调用这个函数会有问题。
    //每次都是在原来的地址上改变，所以每次得到后应该立马取出
    //解析时间到结构体tm去
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    //从后往前找到第一个/的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if(p == NULL){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
    }
    else{
        strcpy(log_name, p+1);
        strncpy(dir_name,file_name,p-file_name+1);
        snprintf(log_full_name,255,"%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1 , my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if(m_fp == NULL)
        return false;
    
    return true;
}


// 日志写入前会判断当前day是否为创建日志的时间，行数是否超过最大行限制
// 若为创建日志时间，写入日志，否则按当前时间创建新log，更新创建时间和行数
// 若行数超过最大行限制，在当前日志的末尾加count/max_lines为后缀创建新log
void Log::write_log(int level, const char *format, ...){
    
    //获取当前时间
    struct timeval now = {0,0};
    gettimeofday(&now,NULL);
    time_t t = now.tv_sec;      //作用和time_t t = time(NULL);差不多
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    
    //日志分级

    switch (level){
        case 0:
            strcpy(s,"[debug]:");
            break;
        case 1:
            strcpy(s,"[info]:");
            break;
        case 2:
            strcpy(s,"[warn]:");
            break;
        case 3:
            strcpy(s,"[erro]:");
            break;
        default:
            strcpy(s,"[info]");
            break;
    }

    m_mutex.lock();
    
    //更新现有的行数
    m_count++;
    
    // 判断是否需要创建新的日志文件
    //日志不是今天或者写入的日志行数超过单个日志文件的最大行
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        //新文件的名字
        char new_log[256]={0};
        //把缓存区的文件写入，关闭当前打开的文件
        fflush(m_fp);
        fclose(m_fp);

        // 新文件的后缀，即年月日
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);

        if(m_today != my_tm.tm_mday){
            snprintf(new_log,255,"%s%s%s",dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }        
        else{
            snprintf(new_log,255,"%s%s%s,%lld",dir_name, tail, log_name, m_count/m_split_lines);
        }
        m_fp = fopen(new_log,"a");
    }

    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    // 用于转化m_buf为string类型放入block_queue中
    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    

    int m = vsnprintf(m_buf+n, m_log_buf_size - n -1, format, valst);
    m_buf[n+m] = '\n';
    m_buf[n+m+1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    //若m_is_async为true表示异步，默认为同步
    //若异步,则将日志信息加入阻塞队列,同步则加锁向文件中写
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }
    else{
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}



