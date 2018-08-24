#ifndef _COMMUNICATE_H_
#define _COMMUNICATE_H_
/*
*对外提供统一的通信接口,仅提供通信层的借口,
*一个Communicate 对象仅提供一个连接
*/
#include "include/common.h"
#include "ThreadVar/ThreadVar.h"
class Communicate
{
public:
    //建立TCP连接
    Communicate(const char* remote_ip, const uint16_t remote_port);
    ~Communicate();
    
    //发送信息内容
    std::string sendString(const char* str);
    void sendStringNoBlock(const char* req, ThreadVar* client_thread_var, int* num_done, int* num_total, std::string* levelDB_back);
    bool isGoodConnect();

private:
    //用以标明tcp连接是否成功
    bool m_bIsGoodConnect;
    static void* send_thread(void* arg);
    static void* recv_thread(void* arg);
    static void* main_thread(void* arg);
    int m_iSocket_fd;
    sockaddr_in m_server_address;
};
#endif