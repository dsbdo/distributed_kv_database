#include "GateServer/GateServer.h"
#include "ClusterServer/ClusterServer.h"
#include <jsoncpp/json/json.h>

using namespace std;
GateServer *gs = nullptr;

void signalCallbackHandle(int signum)
{
    delete gs;
    exit(signum);
}
int main(int argc, char **argv)
{
    try
    {
        signal(SIGINT, signalCallbackHandle);
        char *ip = NULL;
        uint16_t gsport = 9001;
        uint16_t csport = 8001;
        //默认 leveldb_port 7001 gsport 9001 csport 8001
        string join_ip;
        uint16_t join_port = 0;
        //解析命令
        for (int i = 1; i < argc; i++)
        {
            char *option = argv[i];
            if (!strcmp(option, "--joinip"))
            {
                join_ip = std::string(argv[++i]);
                continue;
            }
            if (!strcmp(option, "--joinport"))
            {
                join_port = atoi(argv[++i]);
                continue;
            }
            if (!strcmp(option, "--gsport"))
            {
                gsport = atoi(argv[++i]);
                continue;
            }
            if (!strcmp(option, "--csport"))
            {
                csport = atoi(argv[++i]);
                continue;
            }
        }
        //没有指定ip与port进行加入,那就默许自己为master节点
        bool master = (join_ip == "" && !join_port);
        gs = new GateServer(gsport, csport, ip, master);
        cout << "gateways server test" << endl;
        cout << "hostname: " << gs->getServerName() << endl;
        cout << "ip: " << gs->getIp() << endl;
        cout << "port: " << gs->getPort() << endl;
        if (join_ip != "" && join_port)
            gs->joinCluster(join_ip, join_port);

        while (true)
        {
            int clfd = gs->acceptConnect();
            std::cout << "go on!!!" << std::endl;
            if (clfd < 0)
                throw K_SOCKET_ACCEPT_ERROR;
            gs->requestHandle(clfd);
        }
    }

    catch (int e)
    {
        std::cerr << "e=" << e << std::endl;
    }
    return 0;
}