#include<iostream>
#include<ctime>
#include<jsoncpp/json/json.h>
#include<string>
int main(int argc, char** argv) {
    using namespace std;
    time_t timestamp;
    cout << time(NULL) << endl;
    cout << time(&timestamp) << endl;
    cout << timestamp << endl;
        Json::Value root;
    root["req_type"] = "clusterserver_join" ;
    root["ip"] = "127.0.0.1";
    root["port"] = "8080";
    Json::StyledWriter writer;
    std::string str = writer.write(root);
    cout << str;
    return 0;

}