//cltst.cpp
//cltst.cpp
//client program
#include "Communicate/Communicate.h"
#include <jsoncpp/json/json.h>
using namespace std;
int main(int argc, char** argv)
{
  if (argc!=3)
  {
    printf("usage: c.out <gateway server> <gateway server port>");
    return 1;
  }
  try
  {
    cout<<"client test"<<endl;
    Json::Value root;
    root["req_type"] = "put";
    root["req_args"]["cluster_id"] = 0;
    root["req_args"]["key"] = "ly232";
    root["req_args"]["value"] = "Lin Yang";
    root["sync"] = "true"; //tell server to ensure consistency before ack
                           //alternatively, if this field is not specified,
                           //server will be eventually consistent (i.e. default false)
    Json::StyledWriter writer;
    std::string outputConfig = writer.write(root);

    Communicate comm(argv[1], atoi(argv[2]));
    using namespace std;
    cout << "send string is: " << outputConfig.c_str() << endl;
    std::string reply = comm.sendString(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;

    root.clear();
    root["req_type"] = "get";
    root["req_args"]["key"] = "ly232";
    outputConfig = writer.write(root);
    reply = comm.sendString(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;

    root.clear();
    root["req_type"] = "delete";
    root["req_args"]["key"] = "ly232";
    outputConfig = writer.write(root);
    reply = comm.sendString(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;

    root.clear();
    root["req_type"] = "get";

    root["req_args"]["key"] = "ly232";
    outputConfig = writer.write(root);
    reply = comm.sendString(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;

    root.clear();
    root["req_type"] = "exit"; 
    outputConfig = writer.write(root);
    reply = comm.sendString(outputConfig.c_str());
    std::cout<<"reply="<<reply<<std::endl;

  }
  catch(int e)
  {
    cout<<"error code = "<<e<<endl;
  }
  return 0;
}
