#ifndef _CLUSTER_SERVER_H_
#define _CLUSTER_SERVER_H_
#include "common.h"
#include "Server/Server.h"
#include "Communicate/Communicate.h"

#include <jsoncpp/json/json.h>
//用以记录一个pair中哪一个cluster 对应的 size(levelDB数量)
typedef std::pair<uint16_t, uint16_t> cluster_id_size;

class ClusterServer : public Server{
public:
    //负责发送心跳包的就是master 节点
    ClusterServer(const char* ip, const uint16_t port,  bool master = false);
    virtual void requestHandler(int cluster_fd);
    virtual ~ClusterServer();
    //获取线程对象
     pthread_t* getThreadObj();
     //获取对应的集群的所有levelDB服务器
     std::vector<ip_port>* getServerList(const size_t cluster_id);
     
     //获取集群服务器的全局信息
     Json::Value getSerializedState();
     
     //一个levelDB申请加入一个cluster 中
     void joinCluster(const std::string join_ip, const uint16_t join_port);

     // 心跳包发送处理 发给集群服务器心跳包的错误处理
     void heartBeatClusterServerErrHandle(ip_port dead_cluster_server);
     //心跳包发送处理， 发给leveldb服务器错误处理
     void heartBeatLeveldbServerErrHandle(ip_port dead_leveldb_server);

     //申请成为一个master 节点
     void newMasterResponseHandle(std::string response_msg);
private:
    struct ClusterMinHeap {
        ClusterMinHeap();
        void changeSize(uint16_t cluster_id, uint16_t cluster_size);
        //获取最小堆的堆顶元素
        uint16_t getMinClusterId();
        //堆的数据结构
        std::vector<cluster_id_size> heap;
        //heap index
        std::vector<int> cluster_heap_idx;
    };
    pthread_t m_thread_obj;

    //维护集群服务器的数据结构
    std::vector<std::vector<ip_port> > m_cluster_table_leveldb; //集群服务器表， 记录各个集群中的所有信息
    ClusterMinHeap m_cluster_min_heap;
    std::map<ip_port, uint16_t> m_leveldb_2_cluster_map;
    std::map<ip_port, bool> m_existing_cs_set;
    pthread_mutex_t m_index_lock;
    //维护集群服务器的数据结构的信息

    static void* main_thread(void*);
    static void* send_thread(void*);
    static void* recv_thread(void*);

    static void processClusterRequest(std::string request, std::string& response, ClusterServer* cluster_server );

    //添加一台levelDB服务器到集群中
    uint16_t registerServer(const std::string ip, const uint16_t port);
    time_t m_timestamp;
    void broadcastUpdateClusterState(const ip_port peer_ip_port);
    //brocast to cluster server
    void broadcast(const ip_port& exclude, const Json::Value& msg, void (*err_handle)(void*) = NULL, void (*response_handle)(void*)=NULL);
    //brocast to leveldb server
    void broadcast(const std::vector<ip_port>& receiver_set, const Json::Value& msg,void (*err_handle)(void*) = NULL, void (*response_handle)(void*)=NULL);

    void updateClusterState(const Json::Value& root);

    static void heartbeatHandler(int sign_num);
    static void _heartbeatClusterServerErrHandle(void* arg);
    static void _heartbeatLeveldbServerErrHandle(void* arg);
    static void _newMasterResponseHandle(void* arg);
public:
    bool m_is_master;
    bool m_get_heartbeat_msg;
    bool m_new_master_ongoing;
};
#endif