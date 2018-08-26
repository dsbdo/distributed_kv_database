#include "LevelDbServer/LevelDbServer.h"
#include <cstdlib>
#include <stdlib.h> 
#include <iostream>
using namespace std;

LevelDbServer* ls;

void signalCallbackHandle(int signum)
{
    delete ls;
    exit(signum);
}

void printusage()
{
      cout<<"./leveldbserver.out [--clusterport] [--selfport] "<<
                "[--clusterip] [--selfip] [--dbdir]"<<endl;

}

int main(int argc,char** argv)
{
    try
    {
        signal(SIGINT,signalCallbackHandle);
        uint16_t cluster_port=9998;
        uint16_t self_port=8888;
        std::string cluster_ip="";
        char* self_ip=NULL;
        std::string database_dir="/tmp/testdb";
        
        for (int i=1; i<argc; i++)
        {
            char* option = argv[i];
            if (!strcmp(option,"--help"))
            {
                printusage();
                return 0;
            }
            if (!strcmp(option,"--clusterport"))
            {
                cluster_port = atoi(argv[++i]);
                continue;
            }
            if (!strcmp(option,"--selfport"))
            {
                self_port = atoi(argv[++i]);
                continue;
            }
            if (!strcmp(option,"--clusterip"))
            {
                cluster_ip = std::string(argv[++i]);
                continue;
            }
            if (!strcmp(option,"--selfip"))
            {
                self_ip = argv[++i];
                continue;
            }
            if (!strcmp(option,"--dbdir"))
            {
                database_dir = std::string(argv[++i]);
                continue;
            }
        }

        ls=new LevelDbServer(cluster_port,self_port,cluster_ip,self_ip,database_dir);

        while(true)
        {
            int clfd=ls->acceptConnect();
            cout<<clfd<<endl;
            if(clfd<0)
                throw K_SOCKET_ACCEPT_ERROR;
            ls->requestHandle(clfd);
        }
        cout<<"dfdsaffdasadfsdaf";

    }
    catch(int e)
    {
        delete ls;
        std::cerr<<"e = "<<e<<std::endl;
    }
    return 0;
}