#ifndef _leveldbserver_h
#define _leveldbserver_h

#ifndef _commom_h
#include "common.h"
#endif

#ifndef _server_h
#include "Server/Server.h"
#endif

#ifndef _chord_h
#include "Chord/Chord.h"
#endif

#include <sys/wait.h>
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);


class LevelDbServer:public Server
{
public:
    LevelDbServer(const uint16_t cluster_svr_port,
        const uint16_t self_port,
        std::string cluster_svr_ip,
        const char self_ip[]=NULL,
        std::string dbdir="/database/database_0");//这个数据库地址可以修改
    virtual void requestHandle(int clfd);
    virtual ~LevelDbServer();

private:
    static void processLeveldbRequest(std::string& request,
                    std::string& response,
                    LevelDbServer* handle);
    static void* mainThread(void*);
    static void* sendThread(void*);
    static void* recvThread(void*);

    //leveldb 接口
    leveldb::DB* m_database=NULL;
    leveldb::Options m_options;
    leveldb::Status m_status;

    //cluster 接口
    std::string m_cluster_svr_ip;
    uint16_t m_cluster_svr_port;
    void joinCluster();
    void leaveCluster();

    //chord接口
    chordModuleNS::chordModule* m_chordhd1;

    Syncobj m_chordso;
    static void* chordInit(void*);
};

#endif
