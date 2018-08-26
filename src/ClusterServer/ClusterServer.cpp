#include "ClusterServer.h"

ClusterServer::ClusterServer(const char *ip, const uint16_t port, bool is_master)
    : Server(ip, port),
      m_cluster_table_leveldb(K_MAX_CLUSTER),
      m_is_master(is_master),
      m_get_heartbeat_msg(false),
      m_new_master_ongoing(false)
{
    //启动集群服务器
    std::cout << "启动集群服务器，Ip地址是： " << ip << " 端口是 port: " << port << std::endl;
    //初始化锁变量
    pthread_mutex_init(&m_index_lock, NULL);
    //初始化集群中集群服务器相关信息,标明该服务器本身已经存在集群服务器中
    m_existing_cs_set.insert(std::pair<ip_port, bool>(ip_port(getIp(), getPort()), true));

    //将当前时间记录到m_timestamp
    time(&m_timestamp);

    //设置信号闹钟
    alarm(K_HEARTBEAT_RATE);
    //信号闹钟到时间后，触发哪一个函数
    signal(SIGALRM, heartbeat_handler);

    //如果信号发送中断了某部分阻塞型的系统调用，在这里信号到来前的系统调用。
    siginterrupt(SIGALRM, 0);
}
ClusterServer::~ClusterServer()
{
    ip_port dummy;
    Json::Value msg;
    msg["req_type"] = "cluster_server_leave";
    msg["ip"] = getIp();
    msg["port"] = getPort();
    broadcast(dummy, msg);
}

//请求处理，启动主线程
void ClusterServer::requestHandler(int clfd)
{
    pthread_t main_thread_obj;
    //这里new 出来的信息内容，均由主线程进行处理
    int *clfd_ptr = new int;
    *clfd_ptr = clfd;
    std::vector<void *> *argv = new std::vector<void *>;
    argv->push_back((void *)clfd_ptr);
    argv->push_back((void *)this);

    //可能会存在问题，打个标记在这里
    int thread_create_result = pthread_create(&main_thread_obj, 0, &main_thread, (void *)argv);
    if (thread_create_result != 0)
    {
        throw K_THREAD_ERROR;
    }
}

pthread_t *ClusterServer::getThreadObj()
{
    return &m_thread_obj;
}

std::vector<ip_port> *ClusterServer::getServerList(const size_t cluster_id)
{
    return &m_cluster_table_leveldb[cluster_id];
}

//这个函数是干啥用的呢，不是很明白
Json::Value ClusterServer::getSerializedState()
{
    if (pthread_mutex_lock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
    Json::Value result;
    result["ctbl"] = Json::Value(Json::arrayValue);
    //集群服务器数目
    int ctbl_len = m_cluster_table_leveldb.size();
    for(int i =0; i < ctbl_len; i++) {
        result["ctbl"][i] = Json::Value(Json::arrayValue);
        int cluster_leveldb_num = m_cluster_table_leveldb[i].size();
        for(int j = 0; j < cluster_leveldb_num; j++) {
            Json::Value val;
            val["ip"] = m_cluster_table_leveldb[i][j].first;
            val["port"] = m_cluster_table_leveldb[i][j].second;
            result["ctbl"][i].append(val);
        }
    }
    //堆状态
    result["cmh"]["heap"] = Json::Value(Json::arrayValue);
    result["cmh"]["cluster_heap_idx"] = Json::Value(Json::arrayValue);
    //一共有几个集群
    int size = m_cluster_min_heap.cluster_heap_idx.size();
    for(int i =0; i < size; i++) {
        Json::Value val;
        val["cluster_id"] = m_cluster_min_heap.heap[i+1].first;
        val["cluster_size"] = m_cluster_min_heap.heap[i+1].second;
        result["cmh"]["heap"].append(val);
        val.clear();
        val = m_cluster_min_heap.cluster_heap_idx[i];
        result["cmh"]["cluster_heap_idx"].append(val);
    } 

    //获取leveldb_2_cluster 的信息
    result["ldbsvr_cluster_map"] = Json::Value(Json::arrayValue);
    std::map<ip_port, uint16_t>::iterator itr = m_leveldb_2_cluster_map.begin();
    while(itr != m_leveldb_2_cluster_map.end()) {
        Json::Value val;
        val["ip"] = itr->first.first;
        val["port"] = itr->first.second;
        val["cluster_id"] = itr->second;
        result["ldbsvr_cluster_map"].append(val);
        itr++;
    }
    //获取存在的cluster server 信息
    result["existing_cs_set"] = Json::Value(Json::arrayValue);
}