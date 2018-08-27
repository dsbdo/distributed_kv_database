#ifndef _SERVER_H_
#define _SERVER_H_
#include "common.h"
#include "Communicate/Communicate.h"
typedef std::pair<std::string, uint16_t> ip_port;
class Server {
public:
    Server(const char* local_ip, const uint16_t local_port);
    virtual ~Server();
    //纯虚函数
    //virtual void requestHandle(int clfd) = 0;
    int acceptConnect();
    uint16_t getPort();
    std::string getIp();
    std::string getServerName();
    int getSocketFd();


//可以为子类看到，但是不能为其他类看到
protected:
    int m_socket_fd;
    sockaddr_in m_server_address;
    char m_host_name[256];
private:
    Server& operator=(Server&);
};

#endif