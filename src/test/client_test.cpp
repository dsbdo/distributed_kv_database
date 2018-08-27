#include <iostream>
#include <cstdlib>
#include "Communicate/Communicate.h"
#include "Server/Server.h"
using namespace std;
int main(int argc, char **argv)
{

    int port = atoi(argv[1]);
    cout << port << endl;
    Communicate *comm = new Communicate("127.0.0.1", 8001);
    //自己开一个监听端口
    Server* server = new Server("127.0.0.1", 9001);
    Json::Value root;
    Json::StyledWriter writer;
    Json::Reader reader;
    root["req_type"] = "leveldbserver_join";
    root["req_args"]["ip"] ="127.0.0.1";
    root["req_args"]["port"] = 9001;
    std::string request = writer.write(root);
    cout << "request str is: " << request << endl;
    std::string response = comm->sendString(request.c_str());
    std::cout << "join cluster response: " << response << std::endl;
    root.clear();
    cout << "ack msg is: " << comm->sendString("test in Communicate") << endl;
    while(true) {
        server->acceptConnect();
    }
    return 0;
}