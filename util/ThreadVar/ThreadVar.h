#ifndef _THREAD_VAR_H_
#define _THREAD_VAR_H_
#include <vector>
#include<pthread.h>
class ThreadVar {
public:
    ThreadVar(size_t thread_num, size_t mutex_num, size_t cv_num);
    ~ThreadVar();
    std::vector<pthread_t> m_thread_obj_arr;
    std::vector<pthread_mutex_t> m_mutex_arr;
    //条件变量，感情就是一个共享锁吧
    std::vector<pthread_cond_t> m_cv_arr;
private:
    const ThreadVar& operator=(const ThreadVar&);
};
#endif