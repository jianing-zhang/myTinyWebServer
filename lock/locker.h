#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 封装信号量
class sem{
public:
    sem(){
        if(sem_init(&m_sem,0,0)!=0)         //第一个参数是需要绑定的信号量，第二个是进程共享，为0表示在当前进程共享，第三个参数是指定初始化第一个参数的值
            throw std::exception();
    }
    
    // 指定信号量初始值
    sem(int num){
        if(sem_init(&m_sem,0,num)!=0)
            throw std::exception();
    }
    
    ~sem(){
        sem_destroy(&m_sem);
    }

    void wait(){                        //信号量-1，信号量不能小于0
        sem_wait(&m_sem);
    }

    void post(){                    //信号量+1 
        sem_post(&m_sem);
    }

private:
    sem_t m_sem;
};

// 封装互斥锁
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL)!=0)       //第一个参数是绑定的锁描述符，第二个参数是对m_mutex的配置，可以配置是进程内线程可用还是进程之间，默认是进程内；
            throw std::exception();
    }

    ~locker(){
        pthread_mutex_destroy(&m_mutex);            
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex)==0;         //上锁
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex)==0;       //解锁
    }
    
    // 获取锁描述符
    pthread_mutex_t *get(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

// 封装条件变量
class cond
{
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0)       //第二个是第一个参数的配置，配置条件变量属性
            throw std::exception();
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }

    // 条件等待，第一个参数为要解除的互斥锁，工作流程：
    // 首先挂起当前线程，将其加入等待满足条件队列中，等待被唤醒
    // 释放mutex，此时会在等待被别的线程唤醒，当被使用pthread_cond_signal/pthread_cond_boradcast函数唤醒后该线程后
    // 将再次争夺mutex，只要争夺到了mutex锁，就继续pthread_cond_wait后续的代码
    bool wait(pthread_mutex_t *m_mutex){
        auto ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret==0;
    }
    // 设置超时事件，线程等待一定的时间，如果超时或有信号触发，线程唤醒。
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        auto ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret==0;
    }
    // 唤醒单个变量
    bool singal(){
        return pthread_cond_signal(&m_cond)==0;
    }
    // 广播唤醒所有线程
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond)==0;
    }
private:
    pthread_cond_t m_cond;
};

#endif