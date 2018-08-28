#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>
#include "ClusterServer/ClusterServer.h"
#include "debug.h"
int main(int argc, char **argv)
{
    using namespace std;
    //new 一个master 节点
    ClusterServer* cs = new ClusterServer("127.0.0.1", 8001, true); 
    //测试函数
    cout << "\033[32m DEBUG:: cluster_ip is: " << cs->getIp() << " cluster_port is: " << cs->getPort() << "\033[0m" << endl;



   // cs->
    //cout << "\033[32m DEBUG:: " << cs
    while(1) {
        int clfd = cs->acceptConnect();
        cout << clfd << endl;
        string str = "clfd is: ";
        str += to_string(clfd);
        cout << str << endl;
        debugInfo(str);
        if(clfd < 0) {
            throw K_SOCKET_ACCEPT_ERROR;
        }
        cs->requestHandle(clfd);
    }
    //先试一下加入集群服务器的函数看看表现正常不正常
    return 0;
}