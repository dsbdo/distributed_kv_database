#include "ClusterServer.h"


ClusterServer *cluster_server_obj;
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
    signal(SIGALRM, heartbeatHandler);

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

//这个函数用以获取当前集群服务器的全局信息
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
    for (int i = 0; i < ctbl_len; i++)
    {
        result["ctbl"][i] = Json::Value(Json::arrayValue);
        int cluster_leveldb_num = m_cluster_table_leveldb[i].size();
        for (int j = 0; j < cluster_leveldb_num; j++)
        {
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
    for (int i = 0; i < size; i++)
    {
        Json::Value val;
        val["cluster_id"] = m_cluster_min_heap.heap[i + 1].first;
        val["cluster_size"] = m_cluster_min_heap.heap[i + 1].second;
        result["cmh"]["heap"].append(val);
        val.clear();
        val = m_cluster_min_heap.cluster_heap_idx[i];
        result["cmh"]["cluster_heap_idx"].append(val);
    }

    //获取leveldb_2_cluster 的信息
    result["ldbsvr_cluster_map"] = Json::Value(Json::arrayValue);
    std::map<ip_port, uint16_t>::iterator itr = m_leveldb_2_cluster_map.begin();
    while (itr != m_leveldb_2_cluster_map.end())
    {
        Json::Value val;
        val["ip"] = itr->first.first;
        val["port"] = itr->first.second;
        val["cluster_id"] = itr->second;
        result["ldbsvr_cluster_map"].append(val);
        itr++;
    }
    //获取存在的cluster server 信息
    result["existing_cs_set"] = Json::Value(Json::arrayValue);
    std::map<ip_port, bool>::iterator itr_second = m_existing_cs_set.begin();
    while (itr_second != m_existing_cs_set.end())
    {
        Json::Value val;
        val["ip"] = itr_second->first.first;
        val["port"] = itr_second->first.second;
        val["existing_cs_set"].append(val);
        itr_second++;
    }
    //获取timestamp
    result["timestamp"] = static_cast<double>(m_timestamp);
    if (pthread_mutex_unlock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
    return result;
}

//向一个集群服务器发起加入申请
void ClusterServer::joinCluster(const std::string join_ip, const uint16_t join_port)
{
    //创建一个通信类
    Communicate comm_client(join_ip.c_str(), join_port);
    Json::Value root;
    root["req_type"] = "clusterserver_join";
    root["ip"] = getIp();
    root["port"] = getPort();
    Json::StyledWriter writer;
    std::string output_config = writer.write(root);
    std::string reply = comm_client.sendString(output_config.c_str());
    std::cout << "cluster server join response is: " << reply << std::endl;

    //重构集群服务器
    Json::Reader reader;
    root.clear();
    //将reply的内容， 解析到root 中去
    if (!reader.parse(reply, root))
    {
        std::cerr << "\033[31m failed to parse cluster server config response \033[0m"
                  << std::endl;
        exit(1);
    }
    root = root["result"];
    if (pthread_mutex_lock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
    updateClusterState(root);
    if (pthread_mutex_unlock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
}

//发到集群服务器的心跳包错误处理函数
void ClusterServer::heartBeatClusterServerErrHandle(ip_port dead_cluster_server)
{
    if (pthread_mutex_lock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
    m_existing_cs_set.erase(dead_cluster_server);
    if (pthread_mutex_unlock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
}

//发到leveldb 服务器错误处理,清除levelDB
void ClusterServer::heartBeatLeveldbServerErrHandle(ip_port dead_leveldb_server)
{
    if (pthread_mutex_lock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
    //找到该levelDB 归属的集群
    uint16_t cluster_id = m_leveldb_2_cluster_map[dead_leveldb_server];
    //1. 在归属的集群中删除levelDB
    std::vector<ip_port>::iterator itr = m_cluster_table_leveldb[cluster_id].begin();
    while (itr != m_cluster_table_leveldb[cluster_id].end())
    {
        if (*itr == dead_leveldb_server)
        {
            m_cluster_table_leveldb[cluster_id].erase(itr);
            break;
        }
        itr++;
    }
    //更新堆的信息，维护最小堆
    m_cluster_min_heap.changeSize(cluster_id, m_cluster_min_heap.heap[m_cluster_min_heap.cluster_heap_idx[cluster_id]].second - 1);
    //在levelDB 2 cluster map 进行删除
    m_leveldb_2_cluster_map.erase(dead_leveldb_server);
    if (pthread_mutex_unlock(&m_index_lock) != 0)
    {
        throw K_THREAD_ERROR;
    }
}

void ClusterServer::newMasterResponseHandle(std::string response_msg)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(response_msg, root))
    {
        return;
    }
    if (root["result"] != "accept")
    {
        //回复不是接受，那便是假的master
        m_is_master = false;
    }
    else if (!m_new_master_ongoing)
    {
        m_is_master = true;
        m_new_master_ongoing = true;
    }
}

//接下来是private 中的内容了，第一个堆的管理
ClusterServer::ClusterMinHeap::ClusterMinHeap()
{
    cluster_id_size cis;
    //heap.push_back(cis);
    //初始化堆
    for (int i = 0; i < K_MAX_CLUSTER; i++)
    {
        cis.first = i;
        cis.second = 0;
        heap.push_back(cis);
        cluster_heap_idx.push_back(i);
    }
}
void ClusterServer::ClusterMinHeap::changeSize(uint16_t cluster_id, uint16_t new_cluster_size)
{
    //第一，找到集群服务器信息在堆里面的索引
    int cluster_id_index = cluster_heap_idx[cluster_id];

    //更新之后比之前的集群服务器大
    if (new_cluster_size > heap[cluster_id_index].second)
    {
        //变大了，所以需要swiftdown
        heap[cluster_id_index].second = new_cluster_size;
        //和左右子树节点比较，取一个上来
        while (cluster_id_index * 2 + 1 < heap.size())
        {
            int swap_index = 0;
            if (cluster_id_index * 2 + 2 < heap.size())
            {
                swap_index = (heap[cluster_id_index * 2 + 1].second < heap[cluster_id_index * 2 + 2].second) ? cluster_id_index * 2 + 1 : cluster_id_index * 2 + 2;
            }
            else if (cluster_id_index * 2 + 1 < heap.size())
            {
                //仅有左子树
                swap_index = swap_index * 2 + 1;
            }
            else
            {
                //已经是最后一个节点，没有最有子树
                swap_index = cluster_id_index;
            }
            //左右子树均存在，进行交换
            if (new_cluster_size > heap[swap_index].second)
            {
                cluster_heap_idx[heap[swap_index].first] = cluster_id_index;
                cluster_heap_idx[heap[cluster_id_index].first] = swap_index;
                //将原来的内容读取到tmp中
                cluster_id_size tmp = heap[cluster_id_index];
                heap[cluster_id_index] = heap[swap_index];
                heap[swap_index] = tmp;
                cluster_id_index = swap_index;
            }
            else
            {
                //不需要再继续往下了
                break;
            }
        }
    }
    else
    {
        //变小了，向上跑
        heap[cluster_id_index].second = new_cluster_size;
        while ((cluster_id_index - 1) / 2 >= 0)
        {
            int swap_index = cluster_id_index / 2;
            if (new_cluster_size < heap[swap_index].second)
            {

                cluster_heap_idx[heap[swap_index].first] = cluster_id_index;
                cluster_heap_idx[heap[cluster_id_index].first] = swap_index;

                cluster_id_size tmp = heap[cluster_id_index];
                heap[cluster_id_index] = heap[swap_index];
                heap[swap_index] = tmp;
                cluster_id_index = swap_index;
            }
            else
            {
                break;
            }
        }
    }
}
uint16_t ClusterServer::ClusterMinHeap::getMinClusterId()
{
    return heap[0].first;
}

//堆管理结束
void *ClusterServer::main_thread(void *arg)
{
    //主线程
    std::vector<void *> *argv = (std::vector<void *> *)arg;
    int *clfd_ptr = (int *)((*argv)[0]);
    int clfd = *clfd_ptr;
    //线程传递了本对象的指针
    ClusterServer *cluster_server_ptr = (ClusterServer *)((*argv)[1]);
    ThreadVar *thread_var = new ThreadVar(2L, 2L, 1L);
    std::string *reply_msg = new std::string;
    std::vector<void *> thread_arg;
    thread_arg.push_back((void *)&clfd);
    thread_arg.push_back((void *)thread_var);
    thread_arg.push_back((void *)reply_msg);
    thread_arg.push_back((void *)cluster_server_ptr);

    if (pthread_create(&thread_var->m_thread_obj_arr[0], 0, &recv_thread, (void *)&thread_arg) != 0)
    {
        throw K_THREAD_ERROR;
    }
    if (pthread_create(&thread_var->m_thread_obj_arr[1], 0, &send_thread, (void *)&thread_arg) != 0)
    {
        throw K_THREAD_ERROR;
    }

    if (pthread_join(thread_var->m_thread_obj_arr[0], 0) != 0)
    {
        throw K_THREAD_ERROR;
    }
    if (pthread_join(thread_var->m_thread_obj_arr[1], 0) != 0)
    {
        throw K_THREAD_ERROR;
    }
    //通信socket由发送线程进行关闭，避免当一个新的请求出现的时候，出现竞争条件
    delete thread_var;
    delete reply_msg;
    delete (std::vector<void *> *)arg;
    delete clfd_ptr;
    return 0;
}

//发送线程， 未完全明白？？？？
void *ClusterServer::send_thread(void *arg)
{
    std::vector<void *> &argv = *(std::vector<void *> *)arg;
    int clfd = *(int *)argv[0];
    pthread_mutex_t &socket_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[0];
    pthread_mutex_t &cv_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[1];
    pthread_cond_t &cv = ((ThreadVar *)argv[1])->m_cv_arr[0];
    std::string &response_str = (*(std::string *)argv[2]);
    //等待接收线程完成接收并且从levelDB 获取到一个响应之后
    if (pthread_mutex_lock(&cv_mutex) != 0)
    {
        throw K_THREAD_ERROR;
    }
    while (response_str.empty())
    {
        //如果没有接收到响应进程的信息，直接睡眠到该线程
        pthread_cond_wait(&cv, &cv_mutex);
    }
    if (pthread_mutex_unlock(&cv_mutex) != 0)
    {
        throw K_THREAD_ERROR;
    }
    //这个跟那个communicate 类难道不是一模一样的吗
    const char *response = response_str.c_str();
    size_t resp_len = strlen(response) + 1;
    size_t remind_len = resp_len;
    size_t byte_send = -1;
    while (remind_len > 0)
    {
        if (pthread_mutex_lock(&socket_mutex) != 0)
        {
            throw K_THREAD_ERROR;
        }
        //检查是否有发生错误,如果发送失败，则自动重发
        NO_EINTR(byte_send = write(clfd, response, remind_len));
        if (pthread_mutex_unlock(&socket_mutex) != 0)
        {
            throw K_THREAD_ERROR;
        }
        if (byte_send < 0)
        {
            throw K_FILE_IO_ERROR;
        }
        remind_len -= byte_send;
        resp_len += byte_send;
    }
    if (close(clfd) < 0)
    {
        throw K_SOCKET_CLOSE_ERROR;
    }
    return 0;
}

//接收线程
void *ClusterServer::recv_thread(void *arg)
{
    std::vector<void *> &argv = *(std::vector<void *> *)arg;
    int clfd = *(int *)argv[0];
    pthread_mutex_t &socket_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[0];
    int byte_received = -1;
    std::string request;
    char buf[K_BUF_SIZE];
    bool done = false;
    pthread_cond_t &cv = ((ThreadVar *)argv[1])->m_cv_arr[0];
    pthread_mutex_t &cv_mutex = ((ThreadVar *)argv[1])->m_mutex_arr[1];
    std::string *reply_msg = (std::string *)argv[2];
    ClusterServer *cs = (ClusterServer *)argv[3];
    while (!done)
    {
        memset(buf, 0, K_BUF_SIZE);
        if (pthread_mutex_lock(&socket_mutex) != 0)
        {
            throw K_THREAD_ERROR;
        }
        NO_EINTR(byte_received = read(clfd, buf, K_BUF_SIZE));
        if (pthread_mutex_unlock(&socket_mutex) != 0)
        {
            throw K_THREAD_ERROR;
        }
        if (buf[byte_received - 1] == '\0')
        {
            done = true;
            request = request + std::string(buf);
        }
    }
    std::string response;
    processClusterRequest(request, response, cs);
    if (pthread_mutex_lock(&cv_mutex) != 0)
    {
        throw K_THREAD_ERROR;
    }

    *reply_msg = response;
    if (pthread_mutex_unlock(&cv_mutex) != 0)
    {
        throw K_THREAD_ERROR;
    }

    if (pthread_cond_signal(&cv) != 0)
    {
        throw K_THREAD_ERROR;
    }

    return 0;
}

//请求处理.处理结果的信息放在reponse 中
void ClusterServer::processClusterRequest(std::string request, std::string &response, ClusterServer *cs)
{
    Json::Value root;
    Json::Reader reader;
    Json::StyledWriter writer;
    if (!reader.parse(request, root))
    {
        root.clear();
        root["result"] = "";
        response = writer.write(root);
        return;
    }
    std::string req_type = root["req_type"].asString();
    //将req_type 内容转换为小写字符串
    std::transform(req_type.begin(), req_type.end(), req_type.begin(), ::tolower);
    if (req_type == "leveldbserver_join")
    {
        std::string ip = root["req_args"]["ip"].asString();
        uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
        if (pthread_mutex_lock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        //更新cluster_table 与 cluster min heap
        uint16_t cluster_id = cs->registerServer(ip, port);
        //更新levelDB cluster_map 映射
        cs->m_leveldb_2_cluster_map[ip_port(ip, port)] = cluster_id;
        time(&cs->m_timestamp);
        if (pthread_mutex_unlock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        root.clear();
        root["result"] = "ok";
        root["cluster_id"] = cluster_id;
        ip_port exclude;
        //全网广播加入成功
        cs->broadcastUpdateClusterState(exclude);
    }
    else if (req_type == "leveldbserver_leave")
    {
        std::string ip = root["req_args"]["ip"].asString();
        uint16_t port = (uint16_t)root["req_args"]["port"].asInt();
        bool update_info = false;
        root.clear();
        ip_port leveldb_server = ip_port(ip, port);
        if (pthread_mutex_lock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        std::map<ip_port, uint16_t>::iterator itr = cs->m_leveldb_2_cluster_map.find(leveldb_server);
        //在 映射中没有找到
        if (itr == cs->m_leveldb_2_cluster_map.end())
        {
            root["result"] = "";
        }
        else
        {
            root["result"] = "ok";
            //从cluster table 里面中对应id 的集群服务器列表中进行查找，找到就直接删除
            cs->m_cluster_table_leveldb[itr->second].erase(std::find(cs->m_cluster_table_leveldb[itr->second].begin(), cs->m_cluster_table_leveldb[itr->second].end(), itr->first));
            //更新cluster_min_heap 的内容
            cs->m_cluster_min_heap.changeSize(itr->second, cs->m_cluster_min_heap.heap[cs->m_cluster_min_heap.cluster_heap_idx[itr->second]].second - 1);
            cs->m_leveldb_2_cluster_map.erase(itr);
            time(&cs->m_timestamp);
            update_info = true;
        }
        if (pthread_mutex_unlock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        if (update_info)
        {
            //整一空对象，方便传参
            ip_port exclude;
            cs->broadcastUpdateClusterState(exclude);
        }
    }
    else if (req_type == "get_cluster_list")
    {
        std::string key = root["req_args"]["key"].asString();
        root.clear();
        if (pthread_mutex_lock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        std::vector<ip_port> *server_list = cs->getServerList(hash(key));
        if (pthread_mutex_unlock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        std::vector<ip_port>::iterator it = server_list->begin();
        int i = 0;
        while (it != server_list->end())
        {
            root["result"][i]["ip"] = it->first;
            root["result"][i]["port"] = (int)it->second;
            it++;
            i++;
        }
        //and then ??? 喵喵喵
    }
    else if (req_type == "broadcast_update_cluster_state")
    {
        time_t remote_ts = static_cast<time_t>(root["req_args"]["timestamp"].asDouble());
        if (pthread_mutex_lock(&cs->m_index_lock)!= 0)
        {
            throw K_THREAD_ERROR;
        }
        if (difftime(remote_ts, cs->m_timestamp) > 0)
        {
            //如果请求时间比集群服务器记录的时间晚，则更新
            cs->updateClusterState(root["req_args"]);
        }
        if (pthread_mutex_unlock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        root.clear();
        root["status"] = "ok";
    }
    else if (req_type == "clusterserver_join")
    {
        //一个新的集群服务器请求加入集群，喵喵喵？？？？
        ip_port peer_ip_port(root["ip"].asString(), root["port"].asUInt());
        if (pthread_mutex_lock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        cs->m_existing_cs_set[peer_ip_port] = true;
        if (pthread_mutex_unlock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
        root.clear();
        root["status"] = "ok";
        root["result"] = cs->getSerializedState();
        cs->broadcastUpdateClusterState(peer_ip_port);
    }
    else if (req_type == "clusterserver_leave")
    {
        ip_port peer_ip_port(root["ip"].asString(), root["port"].asUInt());
        if (pthread_mutex_lock(&cs->m_index_lock) != 0)
        {
            throw K_THREAD_ERROR;
        }
    }
    else if (req_type == "heartbeat")
    {
        cs->m_get_heartbeat_msg = true;
    }
    else if (req_type == "")
    {
        if (cs->m_is_master || cs->m_get_heartbeat_msg)
        {
            root["result"] = "reject";
        }
        else
        {
            cs->m_get_heartbeat_msg = true;
            root["result"] = "accept";
        }
    }
    else
    {
        root["result"] = "invalid request";
    }
    response = writer.write(root);
}

//向堆中的最小的集群中加入levelDB 服务器
uint16_t ClusterServer::registerServer(const std::string ip, const uint16_t port)
{
    int cluster_id = m_cluster_min_heap.getMinClusterId();
    m_cluster_table_leveldb[cluster_id].push_back(ip_port(ip, port));
    m_cluster_min_heap.changeSize(cluster_id, m_cluster_min_heap.heap[m_cluster_min_heap.cluster_heap_idx[cluster_id]].second + 1);
    return cluster_id;
}

void ClusterServer::broadcastUpdateClusterState(const ip_port peer_ip_port)
{
    Json::Value msg;
    msg["req_type"] = "broadcast_update_cluster_state";
    msg["req_type"] = getSerializedState();
    broadcast(peer_ip_port, msg);
}
//通用broadcast 接口
void ClusterServer::broadcast(const std::vector<ip_port> &receiver_set, const Json::Value &msg, void (*err_handle)(void *), void (*response_handle)(void *))
{
    std::vector<ip_port>::const_iterator itr = receiver_set.begin();
    while (itr != receiver_set.end())
    {
        //初始化一个通信tcp 连接
        Communicate comm(itr->first.c_str(), itr->second);
        if (errno)
        {
            //如果有错误处理函数
            if (err_handle)
            {
                ip_port dead_peer = *itr;
                err_handle((void *)&dead_peer);
            }
            else
            {
                errno = 0;
            }
            itr++;
            continue;
        }
        Json::StyledWriter writer;
        std::string output_config = writer.write(msg);
        std::string resp = comm.sendString(output_config.c_str());
        if (response_handle)
        {
            response_handle((void *)&resp);
        }
        itr++;
    }
}

//向所有的cluster server 进行广播通信
void ClusterServer::broadcast(const ip_port &exclude, const Json::Value &msg, void (*err_handle)(void *), void (*response_handle)(void *))
{
    std::map<ip_port, bool>::iterator itr = m_existing_cs_set.begin();
    std::map<ip_port, bool>::iterator itr_end = m_existing_cs_set.end();
    ip_port self_ip_port(getIp(), getPort());
    while (itr != itr_end)
    {
        if (itr->first == exclude || itr->first == self_ip_port)
        {
            itr++;
            continue;
        }
        Communicate comm(itr->first.first.c_str(), itr->first.second);
        if (errno)
        {
            {
                if (err_handle)
                {
                    ip_port dead_peer = itr->first;
                    err_handle((void *)&dead_peer);
                }
                else
                {
                    errno = 0;
                }
                continue;
            }
        }
        Json::StyledWriter writer;
        std::string output_config = writer.write(msg);
        std::string resp = comm.sendString(output_config.c_str());
        if (response_handle)
        {
            response_handle((void *)&resp);
        }
        itr++;
    }
}


void ClusterServer::updateClusterState(const Json::Value& root) {
    std::cout << "\033[32m update cluster state\033[0m" <<std::endl;
    int i = 0;
    for(Json::Value::const_iterator itr1 = root["ctbl"].begin(); itr1 != root["ctbl"].end(); itr1++) {
        const Json::Value& vec = *itr1;
        m_cluster_table_leveldb[i].clear();
        for(Json::Value::const_iterator itr2 = vec.begin(); itr2 != vec.end(); itr2++) {
            m_cluster_table_leveldb[i].push_back( ip_port( (*itr2)["ip"].asString(), (*itr2)["port"].asUInt() ) );
        }
        i++;
    }
    //更新堆
    m_cluster_min_heap.heap.clear();
    //m_cluster_min_heap.heap.push_back(cluster_id_size());
    m_cluster_min_heap.cluster_heap_idx.clear();
    for(Json::Value::const_iterator itr1 = root["cmh"]["heap"].begin(); itr1 != root["cmh"]["heap"].end();itr1++) {
        m_cluster_min_heap.heap.push_back(cluster_id_size((*itr1)["cluster_id"].asUInt(), (*itr1)["cluster_sz"].asUInt()));
    }
    const Json::Value& cluster_index_ref = root["cmh"]["cluster_heap_idx"];
    int cluster_index_ref_size = cluster_index_ref.size();
    for(int i =0; i < cluster_index_ref_size; i++) {
        m_cluster_min_heap.cluster_heap_idx.push_back(cluster_index_ref[i].asUInt());
    }
    //更新leveldb_cluster_map
    m_leveldb_2_cluster_map.clear();
    for(Json::Value::const_iterator itr1= root["ldbsvr_cluster_map"].begin();itr1 != root["ldbsvr_cluster_map"].end();itr1++) {
        m_leveldb_2_cluster_map.insert(std::pair<ip_port, uint16_t>(ip_port((*itr1)["ip"].asString(), (*itr1)["port"].asUInt()), (*itr1)["cluster_id"].asUInt());
    }
    //更新存在的集群服务器列表
    m_existing_cs_set.clear();
    for(Json::Value::const_iterator itr1 = root["existing_cs_set"].begin(); itr1 != root["existing_cs_set"].end();itr1++) {
         m_existing_cs_set.insert( std::pair<ip_port, bool>(ip_port((*itr1)["ip"].asString(),(*itr1)["port"].asUInt()),true));
    }
    m_timestamp = static_cast<time_t>(root["timestamp"].asDouble());

}
void ClusterServer::heartbeatHandler(int sign_num) {
    if(sign_num == SIGALRM) {
        //需要注意,注意注意
        if(cluster_server_obj->m_is_master){
            //当前节点是主节点
            std::cout << "\033[32m will send heart \033[0m" << std::endl;
            Json::Value heart_beat_msg;
            heart_beat_msg["req_type"] = "heartbeat";
            //将心跳包发给所有的levelDB
            std::map<ip_port, uint16_t>::iterator leveldb_itr = cluster_server_obj->m_leveldb_2_cluster_map.begin();
            std::map<ip_port, uint16_t>::iterator leveldb_itr_end = cluster_server_obj->m_leveldb_2_cluster_map.end();
            std::vector<ip_port>leveldb_set;
            while(leveldb_itr != leveldb_itr_end) {
                leveldb_set.push_back(leveldb_itr->first);
                leveldb_itr++;
            }
            //向所有levelDB 服务器发出广播
            cluster_server_obj->broadcast(leveldb_set, heart_beat_msg,_heartbeatLeveldbServerErrHandle);

            //向所有集群服务器发送心跳包
            ip_port dummy;
            cluster_server_obj->broadcast(dummy, heart_beat_msg, _heartbeatClusterServerErrHandle);
            if(errno) {
                time(&cluster_server_obj->m_timestamp);
                ip_port dummy_exclude;
                cluster_server_obj->broadcastUpdateClusterState(dummy_exclude);
            }

        }
        else {
            //不是masster 节点
            if(cluster_server_obj->m_get_heartbeat_msg) {
                cluster_server_obj->m_get_heartbeat_msg = false;
            }
            else {
                //假设主节点已死，自己自动申请为master 节点
                std::cout << "\033[31m Not received heart msg, assume to be master dead \033[0m" << std::endl;
                Json::Value new_master;
                new_master["req_type"] = "newmaster";
                ip_port dummy;
                cluster_server_obj->broadcast(dummy, new_master, _heartbeatClusterServerErrHandle, _newMasterResponseHandle);
                cluster_server_obj->m_new_master_ongoing  = false;
            }
        }
        alarm(K_HEARTBEAT_RATE);
    }
}

void _heartbeatClusterServerErrHandle(void* arg) {
    if(!arg){
        return ;
    }
    cluster_server_obj->heartBeatClusterServerErrHandle(*(ip_port *)arg);
}
void _heartbeatLeveldbServerErrHandle(void* arg) {
    if(!arg) {
        return ;
    }
    cluster_server_obj->heartBeatLeveldbServerErrHandle(*(ip_port*)arg);
}

void _newMasterResponseHandle(void* arg) {
    if(!arg) {
        return;
    }
    cluster_server_obj->newMasterResponseHandle(*(std::string* )arg);
}