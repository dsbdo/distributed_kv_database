#ifndef _GATESERVER_H_
#define _GATESERVER_H_

#include "common.h"
#include "ThreadVar/ThreadVar.h"
#include "Server/Server.h"
#include "Communicate/Communicate.h"
#include "ClusterServer/ClusterServer.h"
#include <jsoncpp/json/json.h>

class GateServer : public Server
{
    public :
        GateSever(const uint16_t port_gs,  //作为gateserver的端口号
                  const uint16_t port_cs, //作为clusterserver的端口号
                  const char* ip = null , //ip地址，默认为null，可以通过检索网卡自动获取
                  bool master = false );    //是否要成为主服务器，因为主服务器负责各个cluster的广播

        virtual void requestHandler(int fd_client); //客户端的filedescriptor
        virtual ~GateServer();
        void setsync(){sync_client = true;};
        void setasync(){sync_client = false;};
        void joinCluster(std::string & ip_join ,uint16_t port_join );

    private:
        static void *main_thread(void *); //主线程(+static表示静态函数，只能给本代码文件看到，其他代码文件不可用，避免了名字相同时候的冲突)
        static void *send_thread(void *);//用于发送信息的通讯线程
        static void *rec_thread(void *); //用于接受信息的线程
        static void *cluster_server_Init(void*); //用于初始化相对应的cluster
        static void *clean_thread(void*); //用于清楚所有线程
        ClusterServer *cs; //???
        bool sysc_client;











};





#endif