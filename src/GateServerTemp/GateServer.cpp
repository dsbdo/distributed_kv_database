#include "GateServer.h"

void *GateServer::cluster_server_init(void *arg)
{
    GateServer *gs = (GateServer *)arg;
    //接收连接请求
    while (true)
    {
        int clfd = gs->cs->acceptConnect();
        if (clfd < 0)
        {
            throw K_SOCKET_ACCEPT_ERROR;
        }
        gs->cs->requestHandle(clfd);
    }
}

GateServer::GateServer(const uint16_t gsport,
                       const uint16_t csport,
                       const char *ip,
                       bool master):Server(ip, gsport), sync_client(false){
    //启动集群管理器监听levelDB的加入和离开
    cs = new ClusterServer(getIp().c_str(), csport, master);
    std::cout << "\033[32mDEBUG::GateServer Init: ip is" << getIp() << " port is: " << getPort() << "\033[0m"<< std::endl;  

    //开启集群服务器的线程，等待外界连接
    if(pthread_create(cs->getThreadObj(), 0, &cluster_server_init, (void*)this)!= 0) {
        throw K_THREAD_ERROR;
    }
}
GateServer::~GateServer() {
    delete cs;
}
void* GateServer::main_thread(void* arg) {
     /*
   * 1) 运行一个接收进程，接受来自客户端的请求
   * 2) 接着识别一个levelDB 服务器
   * 3) 等待来自 数据库服务器的响应，并回复客户端
   *    then pass that response to client
   */
  std::vector<void*>* argv = (std::vector<void*>*)arg;
  int* clfd_ptr = (int*)((*argv)[0]);
  int clfd = *clfd_ptr;

  GateServer* gate_server = (GateServer*)((*argv)[1]);
  volatile bool* client_exit = new bool;
   *client_exit = false;
   while (!(*client_exit))
  {
    std::string* ackmsg = new std::string;
    ThreadVar* thread_var = new ThreadVar(2L, 2L, 1L);
    std::vector<void*> thread_arg;
    thread_arg.push_back((void*)&clfd);
    thread_arg.push_back((void*)thread_var);
    thread_arg.push_back((void*)ackmsg);
    thread_arg.push_back((void*)client_exit);
    thread_arg.push_back((void*)gate_server);
    //接受进程
        //for recv thread:
    if (pthread_create(&thread_var->m_thread_obj_arr[0], 0, &recv_thread, (void*)&thread_arg)!=0) {
        throw K_THREAD_ERROR;
    }
      
    //for send thread:
    if (pthread_create(&thread_var->m_thread_obj_arr[1], 
		       0, 
		       &send_thread, 
		       (void*)&thread_arg)
	!=0)
      throw K_THREAD_ERROR;

    if (pthread_join(thread_var->m_thread_obj_arr[0], 0)!=0) //recv thread
      throw K_THREAD_ERROR;
    if (pthread_join(thread_var->m_thread_obj_arr[1], 0)!=0) //send thread
      throw K_THREAD_ERROR;
    delete ackmsg;
    delete thread_var;
  }
  delete clfd_ptr;
  delete (std::vector<void*>*)arg;
  delete client_exit;

  if (close(clfd)<0)
    throw K_SOCKET_CLOSE_ERROR;
  return 0;
}

void* GateServer::recv_thread(void* arg) {
    std::vector<void *> &argv = *(std::vector<void *> *)arg;
  int clfd = *(int *)argv[0];
  pthread_mutex_t &socket_mutex = ((ThreadVar*)argv[1])->m_mutex_arr[0];
  int byte_received = -1;
  std::string request;
  char buf[K_BUF_SIZE];
  bool done = false;
  pthread_cond_t &cv = ((ThreadVar *)argv[1])->m_cv_arr[0];
  pthread_mutex_t &cv_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[1];
  std::string *ackmsg = (std::string *)argv[2];
  bool *client_exit = (bool *)argv[3];
  GateServer *gatesvr = (GateServer *)argv[4];


  while (!done)
  {
    memset(buf, 0, K_BUF_SIZE);
    if (pthread_mutex_lock(&socket_mutex) != 0)
      throw K_THREAD_ERROR;
      //这里需要注意一下
    byte_received = read(clfd, buf, K_BUF_SIZE);
    if (pthread_mutex_unlock(&socket_mutex) != 0)
      throw K_THREAD_ERROR;
    if (buf[byte_received - 1] == '\0')
      done = true;
    request = request + std::string(buf);
  }
  std::cout << "request=" << request << std::endl;
  // 如果客户端存在
  // 设置client_exit = false;
  //main thread can close socket accordingly
  Json::Value root;
  Json::Reader reader;
  //Json 解析生成失败
  if (!reader.parse(request, root))
  {
    if (pthread_mutex_lock(&cv_mutex) != 0)
      throw K_THREAD_ERROR;
    *ackmsg = "error: invalid json requst format encounted in recv thread";
    if (pthread_mutex_unlock(&cv_mutex) != 0)
      throw K_THREAD_ERROR;
    if (pthread_cond_signal(&cv) != 0)
      throw K_THREAD_ERROR;
    return 0;
  }
  //处理退出请求
  std::string req_type = root["req_type"].asString();
  if (req_type == "exit")
  { //levelDB 服务器请求退出
    //pthread_mutex_lock(&socket_mutex);
    //client_exit doesn't seem to need sync protection, since
    //only recv thread is accessing it...
    *client_exit = true;
    //pthread_mutex_unlock(&socket_mutex);
    Json::StyledWriter writer;
    root.clear();
    root["status"] = "OK";
    root["result"] = "";
    if (pthread_mutex_lock(&cv_mutex) != 0)
      throw K_THREAD_ERROR;
    *ackmsg = writer.write(root);
    if (pthread_mutex_unlock(&cv_mutex) != 0)
      throw K_THREAD_ERROR;
    if (pthread_cond_signal(&cv) != 0)
      throw K_THREAD_ERROR;
    return 0;
  }
  //请求同步
  std::string sync = root["sync"].asString();
  //是否为同步更新
  (sync == "true") ? gatesvr->setsync() : gatesvr->setasync();
  // 挑选一个levelDB 去转发请求, 现在gateServer作为一个客户端去跟levelDB服务器交互
  std::string key = root["req_args"]["key"].asString();
  bool skip = false;
  std::string ldback;
  //为什么会没有回复，因为他根本就没有找到levelDB的地址去发
  //size_t cluster_id = hash(key);//为何要哈希取值呢
  //通过增加字段来通过调试
  size_t cluster_id = root["req_args"]["cluster_id"].asInt();

  std::cout << "\033[32mDEBUG::GateServer::target cluster id is: " << cluster_id << "\033[0m" << std::endl;
  std::vector<ip_port>* svrlst =
      gatesvr->cs->getServerList(cluster_id);
  std::vector<ip_port>::iterator itr = svrlst->begin();
  if (gatesvr->sync_client)
  {
    while (itr != svrlst->end())
    {
      std::string ldbsvrip = itr->first;
      const uint16_t ldbsvrport = itr->second;
      //std::cout<<"ldbsvrip,port="<<ldbsvrip<<","<<ldbsvrport<<std::endl;
      itr++;
      Communicate comm(ldbsvrip.c_str(), ldbsvrport);
      //向一个levelDB 发送信息
      ldback = comm.sendString(request.c_str());
      root.clear();
      if (!reader.parse(ldback, root))
        continue;
      if (root["status"].asString() == "OK" && !skip)
      {
        skip = true;
        if (pthread_mutex_lock(&cv_mutex) != 0)
          continue;
            *ackmsg = ldback;
        if (pthread_mutex_unlock(&cv_mutex) != 0)
          continue;
      }
    }
    if (!skip) //no leveldb response is positive
    {
      if (pthread_mutex_lock(&cv_mutex) != 0)
        throw K_THREAD_ERROR;
         *ackmsg = ldback;
      if (pthread_mutex_unlock(&cv_mutex) != 0)
        throw K_THREAD_ERROR;
        //触发因条件变量而睡的线程
      if (pthread_cond_signal(&cv) != 0)
        throw K_THREAD_ERROR;
      return 0;
    }
    if (pthread_cond_signal(&cv) != 0)
      throw K_THREAD_ERROR;
  }
  else //async client, i.e. call client::sendstring_noblock 异步更新
  {
    int *numdone = new int;
    *numdone = 0;
    int *numtotal = new int;
    *numtotal = svrlst->size();
    ThreadVar *cso = new ThreadVar(0, 1, 1);
    char *reqstr = new char[request.size() + 1];
    memcpy(reqstr, request.c_str(), request.size() + 1);
    std::vector<Communicate *> *client_vec = new std::vector<Communicate *>();
    std::vector<std::string *> *ldback_vec = new std::vector<std::string *>();
    while (itr != svrlst->end())
    {
      std::string ldbsvrip = itr->first;
      const uint16_t ldbsvrport = itr->second;
      itr++;
      std::string *ldback = new std::string;
      *ldback = "";
      ldback_vec->push_back(ldback);
      Communicate *clt = new Communicate(ldbsvrip.c_str(), ldbsvrport);
      clt->sendStringNoBlock(reqstr, cso, numdone, numtotal, ldback);
      client_vec->push_back(clt);
    }
    //for eventual consistency, we wait for at least one ack.
    if (pthread_mutex_lock(&cso->m_mutex_arr[0]))
      throw K_THREAD_ERROR;
    while (!(*numdone))
      pthread_cond_wait(&cso->m_cv_arr[0], &cso->m_mutex_arr[0]);
    if (pthread_mutex_unlock(&cso->m_mutex_arr[0]))
      throw K_THREAD_ERROR;
    //delete cso;
    std::vector<void *> *cleanarg = new std::vector<void *>;
    cleanarg->push_back((void *)cso);
    cleanarg->push_back((void *)numdone);
    cleanarg->push_back((void *)numtotal);
    cleanarg->push_back((void *)reqstr);
    cleanarg->push_back((void *)client_vec);
    cleanarg->push_back((void *)ldback_vec);
    pthread_t cleanup_thread; //clean up numdone in background
    if (pthread_create(&cleanup_thread,
                       0, &cleanup_thread_handler, (void *)cleanarg))
      throw K_THREAD_ERROR;

    int ldbacksz = ldback_vec->size();
    for (int i = 0; i < ldbacksz; i++)
    {
      std::string *ldback = (*ldback_vec)[i];
      if (*ldback != "")
      {
        if (pthread_mutex_lock(&cv_mutex) != 0)
          throw K_THREAD_ERROR;
        *ackmsg = *ldback;
        if (pthread_mutex_unlock(&cv_mutex) != 0)
          throw K_THREAD_ERROR;
        break;
      }
    }

    if (pthread_cond_signal(&cv) != 0)
      throw K_THREAD_ERROR;
  }
  return 0;


}

void* GateServer::send_thread(void* arg)  {
  std::vector<void *> &argv = *(std::vector<void *> *)arg;
  int clfd = *(int *)argv[0];
  pthread_mutex_t &socket_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[0];
  pthread_mutex_t &cv_mutex = ((ThreadVar*)argv[1])->m_mutex_arr[1];
  pthread_cond_t &cv = ((ThreadVar *)argv[1])->m_cv_arr[0];
  std::string &resp_str = (*(std::string *)argv[2]);

  //wait until recv thread finishes reception
  //and get a response from leveldb server
  if (pthread_mutex_lock(&cv_mutex) != 0)
    throw K_THREAD_ERROR;
  while (resp_str.empty())
    pthread_cond_wait(&cv, &cv_mutex);
  if (pthread_mutex_unlock(&cv_mutex) != 0)
    throw K_THREAD_ERROR;

  const char *resp = resp_str.c_str();
  size_t resp_len = strlen(resp) + 1;
  size_t rmsz = resp_len;
  size_t byte_sent = -1;
  while (rmsz > 0)
  {
    if (pthread_mutex_lock(&socket_mutex) != 0)
      throw K_THREAD_ERROR;
    NO_EINTR(byte_sent = write(clfd, resp, rmsz));
    if (pthread_mutex_unlock(&socket_mutex) != 0)
      throw K_THREAD_ERROR;
    if (byte_sent < 0)
      throw K_FILE_IO_ERROR;
    rmsz -= byte_sent;
    resp += byte_sent;
  }
  return 0;
}

void *GateServer::cleanup_thread_handler(void *arg)
{
  std::vector<void *> *cleanarg = (std::vector<void *> *)arg;
  ThreadVar *cso = (ThreadVar *)((*cleanarg)[0]);
  int *numdone = (int *)((*cleanarg)[1]);
  int *numtotal = (int *)((*cleanarg)[2]);
  char *reqstr = (char *)((*cleanarg)[3]);
  std::vector<Communicate *> *client_vec =
      (std::vector<Communicate *> *)((*cleanarg)[4]);
  std::vector<std::string *> *ldback_vec =
      (std::vector<std::string *> *)((*cleanarg)[5]);
  if (pthread_mutex_lock(&cso->m_mutex_arr[0]))
    throw K_THREAD_ERROR;
  while (*numdone < *numtotal)
    pthread_cond_wait(&cso->m_cv_arr[0], &cso->m_mutex_arr[0]);
  if (pthread_mutex_unlock(&cso->m_mutex_arr[0]))
    throw K_THREAD_ERROR;
  delete cso;
  delete numdone;
  delete numtotal;
  delete cleanarg;
  delete[] reqstr;
  int numclient = client_vec->size();
  for (int i = 0; i < numclient; i++)
  {
    delete (*client_vec)[i];
    delete (*ldback_vec)[i];
  }
  delete client_vec;
  delete ldback_vec;
}

void GateServer::join_cluster(std::string &joinip, uint16_t joinport)
{
  cs->joinCluster(joinip, joinport);
}

void GateServer::requestHandle(int clfd)
{
  pthread_t main_thread_obj;
  int *clfdptr = new int; //will be deleted by main thread
  *clfdptr = clfd;
  std::vector<void *> *argv = new std::vector<void *>;
  //argv will be deleted by main thread
  argv->push_back((void *)clfdptr);
  argv->push_back((void *)this);
  if (pthread_create(&main_thread_obj,
                     0,
                     &main_thread,
                     (void *)argv) != 0)
    throw K_THREAD_ERROR;
}
