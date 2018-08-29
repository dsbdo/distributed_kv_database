#include<iostream>
#include<string>
#include <jsoncpp/json/json.h>
#include "Communicate/Communicate.cpp"

int main(int argc, char const *argv[])
{
    /* code */
    Json::Value root;
    Json::StyledWriter writer;
    root["req_type"] = "heartbeat";

    std::string msg = writer.write(root);
    Communicate comm("127.0.0.1", 7001);
    std::cout<<"xinxi "<<comm.sendString(msg.c_str())<<" xinxi"<<std::endl;
    Communicate comm2("127.0.0.1", 7001);
    std::cout << "response: "<<comm2.sendString(msg.c_str()) << std::endl; 
    
    return 0;
}
