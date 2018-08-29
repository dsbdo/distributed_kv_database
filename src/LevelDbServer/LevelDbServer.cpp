#include "LevelDbServer.h"
#include "Syncobj/Syncobj.h"
#include "Communicate/Communicate.h"
#include <algorithm>
#include <jsoncpp/json/json.h>
LevelDbServer::LevelDbServer(const uint16_t cluster_svr_port,
                    const uint16_t port,
                    std::string cluster_svr_ip,
                    const char ip[],
                    std::string database_dir)
    :Server(ip,port),m_cluster_svr_ip(cluster_svr_ip),m_cluster_svr_port(cluster_svr_port),m_chordso(1,2,1)//感觉这个(1,2,1)参数有问题，怎么用的
{
    m_options.create_if_missing=true;
    m_status=leveldb::DB::Open(m_options,database_dir,&m_database);
    std::cout<<m_status.ok()<<std::endl<<std::endl;
    if(!m_status.ok())
    {
        std::cerr<<"数据库打开错误"<<std::endl;
        throw K_DB_FAIL;
    }
    //加入一个已存在的cluster
    m_cluster_svr_ip=(cluster_svr_ip=="")?getIp():cluster_svr_ip;
    std::cout<<"cluster ip:"<<m_cluster_svr_ip<<std::endl;
    std::cout<<"database dir:"<<database_dir<<std::endl;
    joinCluster();
}

LevelDbServer::~LevelDbServer()
{
    leaveCluster();
    delete m_database;
}

void LevelDbServer::joinCluster()
{
    std::cout<<"集群ip："<<m_cluster_svr_ip<<" 端口："<<m_cluster_svr_port<<std::endl;
    //建立跟集群的连接，还未与本服务器连接

    Communicate comm(m_cluster_svr_ip.c_str(),m_cluster_svr_port);

    Json::Value root;
    Json::StyledWriter writer;
    Json::Reader reader;
    root["req_type"]="leveldbserver_join";
    root["req_args"]["ip"]=getIp();
    root["req_args"]["port"]=getPort();
    std::string request = writer.write(root);
    //本服务器请求加入集群并且取得返回信息
    std::string response =comm.sendString(request.c_str());
    std::cout<<"请求加入集群的回应: "<<response<<std::endl;
    root.clear();
    //把response解析到root
    if(!reader.parse(response,root))
    {
        std::cerr<<"解析错误"<<std::endl;
    }
    if(root["result"]!="ok")
    {
        std::cerr<<"加入集群失败"<<std::endl;
    }

    ip_port join_address(root["chordJoinIP"].asString(),root["chordJoinPort"].asUInt());
    ip_port self_address(getIp(),getPort());
    m_chordhd1=new chordModuleNS::chordModule(self_address,join_address);
    //创建一个线程在等待，不过这个有什么用还要看看
    if(pthread_create(&m_chordso._thread_obj_arr[0],//指向线程标识符的指针
                0,//默认线程属性
                &chordInit,//线程运行函数的起始地
                (void*)this)//运行函数的参数
                !=0)
        throw K_THREAD_ERROR;
}

void* LevelDbServer::chordInit(void* arg)
{
    while(true)
    {

    }
    return 0;
}


void LevelDbServer::leaveCluster()
{
    Communicate comm(m_cluster_svr_ip.c_str(),m_cluster_svr_port);
    Json::Value root;
    Json::StyledWriter writer;
    Json::Reader reader;
    root["req_type"]="leveldbserver_leave";
    root["req_args"]["ip"]=getIp();
    root["req_args"]["port"]=getPort();
    std::string request=writer.write(root);
    std::string response=comm.sendString(request.c_str());
    root.clear();
    if(!reader.parse(response,root))
    {
        std::cerr<<"解析错误"<<std::endl;
    }
    if(root["result"]!="ok")
    {
        std::cerr<<"离开集群失败"<<std::endl;
    }
    delete m_chordhd1;
}

void LevelDbServer::requestHandle(int clfd)
{
    //声明线程id，这里需要回应心跳包
    pthread_t main_thread_obj;
    int* clfdptr=new int;//在主线程中删除
    *clfdptr=clfd;
    std::vector<void*>* argv=new std::vector<void*>;//在主线程中删除
    argv->push_back((void*)clfdptr);
    argv->push_back((void*)this);
    if(pthread_create(&main_thread_obj,
                0,
                &mainThread,
                (void*)argv)
                !=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
}

//main线程在requesthandle中调用，然后main调用recvthread和sendthread
void* LevelDbServer::mainThread(void* arg)
{
    std::vector<void*>* argv=(std::vector<void*>*)arg;
    int* clfdptr=(int*)((*argv)[0]);//client的文件描述符
    int clfd=*clfdptr;
    LevelDbServer* ldbsvr=(LevelDbServer*)((*argv)[1]);
    
    Syncobj* so=new Syncobj(2L,2L,1L);//L是long类型 （线程数目，互斥量，条件变量）
    std::string* ackmsg=new std::string;

    std::vector<void*> thread_arg;
    thread_arg.push_back((void*)&clfd);
    thread_arg.push_back((void*)so);
    thread_arg.push_back((void*)ackmsg);
    thread_arg.push_back((void*)ldbsvr);

    std::cout<<"0000000000000000000000000"<<std::endl;
    //第一个线程是接收，第二个线程是发送
    if(pthread_create(&so->_thread_obj_arr[0],
            0,
            &recvThread,
            (void*)&thread_arg)
            ==0)
    {  
        std::cout<<"1111111111111111111111"<<std::endl;
        //throw K_THREAD_ERROR;
    }

    //等待线程结束，属于线程间同步的操作
    if(pthread_join(so->_thread_obj_arr[0],0)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }

    if(pthread_create(&so->_thread_obj_arr[1],
            0,
            &sendThread,
            (void*)&thread_arg)
            ==0)
    {   
       
        //throw K_THREAD_ERROR;
    }
    if(pthread_join(so->_thread_obj_arr[1],0)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    delete so;
    delete ackmsg;
    delete clfdptr;
    delete (std::vector<void*>*)arg;

     if(close(clfd)<0)
         throw K_SOCKET_CLOSE_ERROR;
    return 0;
}

void* LevelDbServer::recvThread(void* arg)
{
    std::vector<void*>& argv=*(std::vector<void*>*)arg;
    int clfd=*(int*)argv[0];
    //静态互斥锁,套接字互斥量
    pthread_mutex_t& socket_mutex=((Syncobj*)argv[1])->_mutex_arr[0];
    int byte_received=-1;
    std::string request;
    char buf[K_BUF_SIZE];
    bool done=false;
    //条件变量
    pthread_cond_t& cv=((Syncobj*)argv[1])->_cv_arr[0];
    //第二个互斥锁
    pthread_mutex_t& cv_mutex=((Syncobj*)argv[1])->_mutex_arr[1];
    std::string* ackmsg=(std::string*)argv[2];
    LevelDbServer* ldbsvr=(LevelDbServer*)argv[3];

    while(!done)
    {
        memset(buf,0,K_BUF_SIZE);//用0填充buffer
        //加锁
        if(pthread_mutex_lock(&socket_mutex)!=0)
        {
            //::exit(1);
            throw K_THREAD_ERROR;
        }
        //把东西读进buffer里面
        NO_EINTR(byte_received=read(clfd,buf,K_BUF_SIZE));
        //释放锁
        if(pthread_mutex_unlock(&socket_mutex)!=0)
        {
            //::exit(1);
            throw K_THREAD_ERROR;
        }
        if(buf[byte_received-1]=='\0')
            done=true;
        request=request+std::string(buf);
    }

    std::string response;
    std::cout << "levelDB get Info is: " << request  <<std::endl;
    //处理请求
    processLeveldbRequest(request,response,ldbsvr);
    std::cout << "levelDB response Info is" << response << std::endl;
    //使用互斥量把资源锁住，完成任务后再释放
    if(pthread_mutex_lock(&cv_mutex)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    *ackmsg=response;
    if(pthread_mutex_unlock(&cv_mutex)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    //发送信号给另一个正在处于阻塞状态的线程，使它脱离阻塞状态继续执行
    if(pthread_cond_signal(&cv)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    return 0;
}

void* LevelDbServer::sendThread(void* arg)
{
    std::cout << "Debug::come in send thread" << std::endl;
    std::vector<void*>& argv=*(std::vector<void*>*)arg;
    int clfd=*(int*)argv[0];
    pthread_mutex_t& socket_mutex=((Syncobj*)argv[1])->_mutex_arr[0];
    pthread_mutex_t& cv_mutex=((Syncobj*)argv[1])->_mutex_arr[1];
    pthread_cond_t& cv=((Syncobj*)argv[1])->_cv_arr[0];
    std::string response_str=(*(std::string*)argv[2]);
    //在recvThread完成之前此线程一直等待
    //同时从leveldb server中得到回应
    if (pthread_mutex_lock(&cv_mutex)!=0)
    {
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    //如果响应内容为空，则睡
    while(response_str.empty())
        pthread_cond_wait(&cv,&cv_mutex);
    if (pthread_mutex_unlock(&cv_mutex)!=0){
        //::exit(1);
        throw K_THREAD_ERROR;
    }
    std::cout << "LevelDB::send_thread is: " << response_str << std::endl;
    const char* response=response_str.c_str();
    size_t response_len=strlen(response)+1;
    size_t response_msg_size=response_len;
    size_t byte_sent=-1;
    
    while(response_msg_size>0)
    {
        if (pthread_mutex_lock(&socket_mutex)!=0)
        {
            //::exit(1);
            throw K_THREAD_ERROR;
        }
        //反止出现系统中断
        NO_EINTR(byte_sent = write(clfd,response,response_msg_size));
        if (pthread_mutex_unlock(&socket_mutex)!=0) {
            throw K_THREAD_ERROR;
        }
        if(byte_sent<0)
            throw K_FILE_IO_ERROR;
        response_msg_size-=byte_sent;
        response+=byte_sent;
    }
    std::cout << "LevelDB::send_thread over" << std::endl;
    return 0;
}

void LevelDbServer::processLeveldbRequest(std::string& request,
                            std::string& response,
                            LevelDbServer* ldbsvr)
{
    Json::Value root;
    Json::Reader reader;
    Json::StyledWriter writer;
    //处理不成功就退出
    if(!reader.parse(request,root))
    {
        ldbsvr->m_status=leveldb::Status::InvalidArgument("Json解析错误");
        root.clear();
        root["result"]="";
        root["status"]=ldbsvr->m_status.ToString();
        response=writer.write(root);
        return;
    }
    std::string req_type=root["req_type"].asString();
    //把所有字母转变为小写字母
    std::transform(req_type.begin(),
                req_type.end(),
                req_type.begin(),
                ::tolower);
    if(req_type=="put")
    {
        std::string key=root["req_args"]["key"].asString();
        std::string value=root["req_args"]["value"].asString();
        //将Key-Value插入到leveldb中
        ldbsvr->m_status=ldbsvr->m_database->Put(leveldb::WriteOptions(),key,value);
        root.clear();
        root["result"]="";
    }
    else if(req_type=="get")
    {
        std::string key=root["req_args"]["key"].asString();
        std::string value;
        //根据Key值在leveldb中查找其对应的Value，返回值存放在res中
        ldbsvr->m_status=ldbsvr->m_database->Get(leveldb::ReadOptions(),key,&value);
        root.clear();
        root["result"]=value;
    }
    else if(req_type=="delete")
    {
        std::string key=root["req_args"]["key"].asString();
        //根据Key值在leveldb中删除其对应的Value
        ldbsvr->m_status=ldbsvr->m_database->Delete(leveldb::WriteOptions(),key);
        root.clear();
        root["result"]="";
    }
    else if (req_type=="exit")
    {
        ldbsvr->m_status = leveldb::Status::OK();
        root.clear();
        root["result"] = "";
    }
    else if(req_type == "heartbeat") {
        //这里是心跳包，直接响应即可
        std::cout << "recv heart beat prepare to response it" << std::endl;
        root["result"] = "OK";
    }
    else
    {
        ldbsvr->m_status = leveldb::Status::InvalidArgument(request);
        root.clear();
        root["result"] = "";
    }
    root["status"]=ldbsvr->m_status.ToString();
    response=writer.write(root);
}
