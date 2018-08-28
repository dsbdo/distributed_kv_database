//syncobj.h

/*
 * resource management class for synchronization objects
 */

#ifndef _syncobj_h
#define _syncobj_h

#include <vector>
#include <pthread.h>

class Syncobj
{
public:
  //see example usage pattern in gateserver::main_thread
  Syncobj(size_t nthread, //num threads, used in ptread_create
          size_t nmutex,  //num mux
          size_t ncv);    //num cv
  ~Syncobj();
  std::vector<pthread_t> _thread_obj_arr;
  std::vector<pthread_mutex_t> _mutex_arr;

  //条件变量 https://www.cnblogs.com/jeanschen/p/3494578.html http://www.cs.kent.edu/~ruttan/sysprog/lectures/multi-thread/pthread_cond_init.html  
  std::vector<pthread_cond_t> _cv_arr;
private:
  Syncobj();
  Syncobj(const Syncobj&);
  const Syncobj& operator=(const Syncobj&);
  
};
#endif
