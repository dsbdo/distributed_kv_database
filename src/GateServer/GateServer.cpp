#include"GateServer.h"


//这部分是为了初始化一个gateserver同时也是作为一个cluster中一分子的clusterserver的功能。开启监听
void* GateServer::cluster_server_Init(void* arg)   //传入的参数是一个GateServer指针
{
    GateServer *gs = (GateServer *)arg ;
    
    //一直维持监听状态
    while(true)
    {
        int fd_client = gs->cs->acceptConnect(); //返回值是另外开通的fd用来通讯的为listen的fd中的队列中的client
        if(fd_client < 0)
            throw K_SOCKET_ACCEPT_ERROR ; //开通通讯socket失败
        gs->cs->requestHandle(fd_client) ; //交付cs进行监听，接受，发送处理
    }
}


GateServer::GateServer(const uint16_t port_gs,  //作为gateserver的端口号
                  const uint16_t port_cs, //作为clusterserver的端口号
                  const char* ip, //ip地址，默认为null，可以通过检索网卡自动获取
                  bool master)
            :Server(port_gs,ip),sysc_client(false)
{   
    //初始化一个gateserver作为一个clusterserver部分的功能模块
    cs = new ClusterServer(ip,port_cs,master);
    if(pthread_create(cs->getThreadObj,0,&cluster_server_Init,(void*)this)!= 0)
        throw K_THREAD_ERROR;

}

GateServer::~GateServer()
{
    delete cs;//析构函数要处理一些成员变量释放空间
}

//从handler把工作转交给main之后，主要就有以下三部分的内容
//1.创建一个接收线程来负责处理连接进来的client
//2/创建一个发送线程去负责挑选适合的leveldb服务器去通讯
//3.等待ldb服务器的反馈，并把他返回给client
void *GateServer::main_thread(void* arg)  //此部分的arg主要分为两部分，fd_client，this指针
{
    std::vector<void*> *argv = (std::vector<void*>*)arg;
    int *fd_client_ptr = (int*)((*argv)[0]);

    GateServer* gateserver_ptr = (GateServer*)((*argv)[1]) ;

    volatile bool* client_exit = new bool;
    *client_exit = false;

    while(!(*client_exit))
    {
        std::string* ackmsg = new std::string ;

        //初始化线程，互斥量
        ThreadVar* thread_arr = new ThreadVar(2L,2L,1L);

        std::vector<void*> *pass_arg;
        (*pass_arg).push_back((void*)fd_client_ptr);//此处直接传指针，没必要定义int来记录，因为参数存在的堆栈和定义一个新的int是一样的
        (*pass_arg).push_back((void*)pass_arg);
        (*pass_arg).push_back((void*)ackmsg);
        (*pass_arg).push_back((void*)client_exit);
        (*pass_arg).push_back((void*)gateserver_ptr);

          //for recv thread:
        if (pthread_create(&thread_arr->m_thread_obj_arr[0], 
                0, 
                &rec_thread, 
                (void*)&pass_arg)
        !=0)
        throw K_THREAD_ERROR;
        //for send thread:
        if (pthread_create(&thread_arr->m_thread_obj_arr[1], 
                0, 
                &send_thread, 
                (void*)&pass_arg)
        !=0)
        throw K_THREAD_ERROR;

        if (pthread_join(thread_arr->m_thread_obj_arr[0], 0)!=0) //recv thread
        throw K_THREAD_ERROR;
        if (pthread_join(thread_arr->m_thread_obj_arr[1], 0)!=0) //send thread
        throw K_THREAD_ERROR;
        delete ackmsg;
        delete so;
    }
    delete fd_client_ptr;
    delete gateserver_ptr;
    delete client_exit;
    delete arg;

    //关闭socket
     if (close(*fd_client_ptr)<0)
        throw K_SOCKET_CLOSE_ERROR;
     return 0;

}

//主要负责把工作分发到main手上，其实可以省略
void GateServer::requestHandler(int fd_client)
{
    pthread_t main_thread_obj;
    
    int * fd_client_ptr = new int ;
    *fd_client_ptr  =fd_client;

    std::vector<void*> *pass_arg = new std::vector<void*>;
    (*pass_arg).push_back((void*)fd_client_ptr);
    (*pass_arg).push_back((void*)this);

    //创建主线程，把工作交付给主线程处理
    if(pthread_create(&main_thread_obj,0,&main_thread,(void*)pass_arg)!=0)
        throw K_THREAD_ERROR;
    
}

void* GateServer::send_thread(void* arg)
{
    std::vector<void*> &argv = *(std::vector<void*>)arg;
    int fd_client  = *(int*)argv[0];
   
    //引用全局的互斥量
    pthread_mutex_t& socket_mutex
    = ((ThreadVar*)argv[1])->m_mutex_arr[0];
    pthread_mutex_t& cv_mutex
    = ((ThreadVar*)argv[1])->m_mutex_arr[1];
    pthread_cond_t& cv 
    = ((ThreadVar*)argv[1])->m_cv_arr[0];

     std::string &ackmsg = *(std::string*)argv[2] ;

    //线程等待直到rec_thread完成并且收到ldbserver的回应
     if (pthread_mutex_lock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        while(ackmsg.empty())
            pthread_cond_wait(&cv, &cv_mutex);
     if (pthread_mutex_unlock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;


    const char* response  = ackmsg.c_str();
    //+1为了留个位置存'\0'
    size_t response_len = strlen(response)+1;
    size_t remain_size = response_len;
    size_t byte_sent = -1;
    while(remain_size>0)
    {
        if (pthread_mutex_lock(&socket_mutex)!=0) 
            throw K_THREAD_ERROR;
            //向socket中写入数据，进行通讯
        NO_EINTR(byte_sent = write(fd_client,response,remain_size));
        if (pthread_mutex_unlock(&socket_mutex)!=0) 
            throw K_THREAD_ERROR;
        if (byte_sent<0) 
            throw K_FILE_IO_ERROR;
        remain_size -= byte_sent;
        response += byte_sent;//向前跳过已经发送的部分内容
    }

    return 0;
}

void* GateServer::rec_thread(void* arg)
{
    std::vector<void*> &argv = *(std::vector<void*>*)arg;
    int fd_client =*(int *)argv[0];
    pthread_mutex_t& socket_mutex 
         = ((ThreadVar*)argv[1])->m_mutex_arr[0];
    pthread_cond_t& cv 
         = ((ThreadVar*)argv[1])->m_cv_arr[0];
    pthread_mutex_t& cv_mutex 
         = ((ThreadVar*)argv[1])->m_mutex_arr[1];
    std::string* ackmsg = (std::string*)argv[2];
    bool* client_exit = (bool*)argv[3];
    GateServer* gateserver = (GateServer*)argv[4];

    int byte_received = -1;
    std::string request;
    char buf[BUF_SIZE];//用于储存接收到的内容
    bool done = false;

    //把socket中的内容全部读到request中，如果socket中没有内容，则会自动睡眠read线程
    while (!done)
    {
        memset(buf, 0, BUF_SIZE);
        if (pthread_mutex_lock(&socket_mutex)!=0) 
            throw K_THREAD_ERROR;
        NO_EINTR(byte_received=read(clfd, buf, BUF_SIZE));
        if (pthread_mutex_unlock(&socket_mutex)!=0) 
            throw K_THREAD_ERROR;
        if(buf[byte_received-1]=='\0')
        done = true;
        request = request + std::string(buf);
    }
    std::cout<<"request="<<request<<std::endl;

    //if client requests exit, 
    //set client_exit flag so that 
    //main thread can close socket accordingly
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(request,root))
    {
        //错误的请求信息
        if (pthread_mutex_lock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        *ackmsg = 
        "错误：该请求的内容格式并不符合json格式";
        if (pthread_mutex_unlock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        if (pthread_cond_signal(&cv)!=0) 
            throw K_THREAD_ERROR;
        return 0;
    }

    std::string req_type = root["req_type"].asString();
    if (req_type=="exit")
    { //leveldb server requests to exit cluster
        //pthread_mutex_lock(&socket_mutex);
        //client_exit doesn't seem to need sync protection, since 
        //only recv thread is accessing it...
        *client_exit = true; //main线程会退出
    
        //pthread_mutex_unlock(&socket_mutex);
        Json::StyledWriter writer;
        root.clear();
        root["status"] = "OK";
        root["result"] = "";
        if (pthread_mutex_lock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        *ackmsg = writer.write(root);
        if (pthread_mutex_unlock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        if (pthread_cond_signal(&cv)!=0) 
            throw K_THREAD_ERROR;
        return 0;
    }

    std::string sync = root["sync"].asString();
    (sync=="true")?GateServer->setsync():GateServer->setasync();
    // 根据通讯报文中的req_type key字段中的内容去hash出对应的cluster id然后再广播给这个集群中的server
    // 
    //char ldbsvrip[INET_ADDRSTRLEN] = "192.168.75.164";
    //const uint16_t ldbsvrport = 8888;
    std::string key = root["req_args"]["cluster_id"].asString();
    bool skip = false;
    std::string ldback;
   // size_t cluster_id = hash(key);
    //std::cout<<"clusterid="<<cluster_id<<std::endl;
    std::vector<ip_port >& svrlst = 
        GateServer->cs->getServerList(cluster_id);
    std::vector<ip_port >::iterator itr 
        = svrlst.begin();
    //std::cout<<"svrlst.size()="<<svrlst.size()<<std::endl;
    if (GateServer->sync_client)
    {
    while(itr!=svrlst.end())
    {
        std::string ldbsvrip = itr->first;
        const uint16_t ldbsvrport = itr->second;
    //std::cout<<"ldbsvrip,port="<<ldbsvrip<<","<<ldbsvrport<<std::endl;
        itr++;
        Communicate clt(ldbsvrip.c_str(), ldbsvrport);
        ldback = clt.sendString(request.c_str()); //来自另一方的回文
        root.clear();
        //如果回文正常，则进行一下个服务器的发送
        if (!reader.parse(ldback,root))
        continue;
        if (root["status"].asString()=="OK"&&!skip)
        {
            skip = true;
            if (pthread_mutex_lock(&cv_mutex)!=0) 
                continue;
            *ackmsg = ldback;
            if (pthread_mutex_unlock(&cv_mutex)!=0) 
                continue;
        }
    }
    if (!skip) //no leveldb response is positive
    {
        if (pthread_mutex_lock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        *ackmsg = ldback;
        if (pthread_mutex_unlock(&cv_mutex)!=0) 
            throw K_THREAD_ERROR;
        if (pthread_cond_signal(&cv)!=0) 
            throw K_THREAD_ERROR;
            return 0;
    }
    if (pthread_cond_signal(&cv)!=0) 
        throw K_THREAD_ERROR;
    }
    else //async client, i.e. call client::sendstring_noblock
    {
        int* numdone = new int;
        *numdone = 0;
        int* numtotal = new int;
        *numtotal = svrlst.size();
        ThreadVar* cso = new ThreadVar(0, 1, 1);
        char* reqstr = new char[request.size()+1];
        memcpy(reqstr, request.c_str(), request.size()+1);
        std::vector<Communicate*>* client_vec = new std::vector<Communicate*>();
        std::vector<std::string*>* ldback_vec = new std::vector<std::string*>();
        while(itr!=svrlst.end())
        {
            std::string ldbsvrip = itr->first;
            const uint16_t ldbsvrport = itr->second;
            itr++;
            std::string* ldback = new std::string;
            *ldback = "";
            ldback_vec->push_back(ldback);
            Communicate* clt = new Communicate(ldbsvrip.c_str(), ldbsvrport);
            clt->sendStringNoBlock(reqstr, cso, numdone, numtotal, ldback);
            client_vec->push_back(clt);
        }
        //for eventual consistency, we wait for at least one ack.
        if (pthread_mutex_lock(&cso->_mutex_arr[0])) 
            throw K_THREAD_ERROR;
        while(!(*numdone))
            pthread_cond_wait(&cso->_cv_arr[0], &cso->_mutex_arr[0]);
        if (pthread_mutex_unlock(&cso->_mutex_arr[0])) throw K_THREAD_ERROR;
        //delete cso;
        std::vector<void*>* cleanarg = new std::vector<void*>;
        cleanarg->push_back((void*)cso);
        cleanarg->push_back((void*)numdone);
        cleanarg->push_back((void*)numtotal);
        cleanarg->push_back((void*)reqstr);
        cleanarg->push_back((void*)client_vec);
        cleanarg->push_back((void*)ldback_vec);
        pthread_t cleanup_thread; //clean up numdone in background
        if (pthread_create(&cleanup_thread, 
            0, &cleanup_thread_handler, (void*)cleanarg))
            throw K_THREAD_ERROR;

        int ldbacksz = ldback_vec->size();
        for (int i=0; i<ldbacksz; i++)
        {
            std::string* ldback = (*ldback_vec)[i];
            if (*ldback!="")
            {
            if (pthread_mutex_lock(&cv_mutex)!=0) 
                throw K_THREAD_ERROR;
            *ackmsg = *ldback;
            if (pthread_mutex_unlock(&cv_mutex)!=0)
                throw K_THREAD_ERROR;
            break;
            }
        } 
            
        if (pthread_cond_signal(&cv)!=0) 
            throw K_THREAD_ERROR;
    }
    return 0;
}

void* GateServer::cleanup_thread(void* arg)
{
  std::vector<void*>* cleanarg = (std::vector<void*>*)arg;
  ThreadVar* cso = (ThreadVar*)((*cleanarg)[0]);
  int* numdone = (int*)((*cleanarg)[1]);
  int* numtotal = (int*)((*cleanarg)[2]);
  char* reqstr = (char*)((*cleanarg)[3]);
  std::vector<Communicate*>* client_vec =
    (std::vector<Communicate*>*)((*cleanarg)[4]);
  std::vector<std::string*>* ldback_vec =
    (std::vector<std::string*>*)((*cleanarg)[5]);
  if (pthread_mutex_lock(&cso->m_mutex_arr[0])) throw K_THREAD_ERROR;
  while (*numdone < *numtotal) 
    pthread_cond_wait(&cso->m_cv_arr[0], &cso->m_mutex_arr[0]);
  if (pthread_mutex_unlock(&cso->m_mutex_arr[0])) 
        throw K_THREAD_ERROR;
  delete cso;
  delete numdone;
  delete numtotal;
  delete cleanarg;
  delete [] reqstr;
  int numclient = client_vec->size();
  for (int i=0; i<numclient; i++)
  {
    delete (*client_vec)[i];
    delete (*ldback_vec)[i];
  }
  delete client_vec;
  delete ldback_vec;

}

void GateServer::join_cluster(std::string& joinip, uint16_t joinport)
{
  cs->join_cluster(joinip, joinport);
}


