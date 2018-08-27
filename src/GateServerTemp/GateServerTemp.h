#ifndef _GATESERVER_TEMP_H_
#define _GATESERVER_TEMP_H_
#include "common.h"
#include "Server/Server.h"
#include "ClusterServer/ClusterServer.h"
#include "util/ThreadVar/ThreadVar.h"
class GateServer:public Server {
public:
    GateServer(uint16_t gsport, 
	     const uint16_t csport,
	     const char* ip = NULL,
             bool master = false //master=true iff send heartbeat
            );
  virtual void requestHandler(int clfd);
  virtual ~GateServer();
  void setsync(){sync_client = true;};
  void setasync(){sync_client = false;};
  void join_cluster(std::string& joinip, uint16_t joinport);
private:
  static void* main_thread(void*);
  static void* send_thread(void*);
  static void* recv_thread(void*);
  static void* cluster_server_init(void*);
  static void* cleanup_thread_handler(void*);
  ClusterServer* cs;
  bool sync_client;
};

#endif