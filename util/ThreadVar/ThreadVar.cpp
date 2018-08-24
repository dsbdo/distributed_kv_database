#include "ThreadVar.h"

ThreadVar::ThreadVar(size_t thread_num, size_t mutex_num, size_t cv_num) {
    for(int i =0; i < thread_num; i++) {
        pthread_t new_thread;
        m_thread_obj_arr.push_back(new_thread);
    }
    //初始化互斥量数组
    for(int i = 0; i < mutex_num; i++) {
        pthread_mutex_t mutex;
        pthread_mutex_init(&mutex, NULL);
        m_mutex_arr.push_back(mutex);
    }
    for(int i = 0; i < cv_num; i++) {
        pthread_cond_t cv;
        pthread_cond_init(&cv, NULL);
        m_cv_arr.push_back(cv);
    }
}

ThreadVar::~ThreadVar() {
    
}
