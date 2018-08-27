//chord.h
//module for managing p2p distributed hashing using chord algo
#ifndef _chord_h
#define _chord_h

#ifndef _common_h
#include "common.h"
#endif

#ifndef _client_h
#include "Communicate/Communicate.h"
#endif

#ifndef _server_h
#include "Server/Server.h"
#endif

#ifndef _syncobj_h
#include "Syncobj/Syncobj.h"
#endif

#include <jsoncpp/json/json.h>
#include <iostream>
#include <sstream>

namespace chordModuleNS {

typedef std::pair<uint16_t, ip_port> ftentry; //finger table entry,
                                              //<hashval, ip_port>

const uint16_t m = 10; //2^m = max number of ldbsvr's allowed per cluster

inline std::string ipport2str(const ip_port& selfAddr)
{
  std::string selfAddrStr;
  std::stringstream ss;
  ss<<selfAddr.second;
  ss>>selfAddrStr;
  selfAddrStr+=selfAddr.first;
  return selfAddrStr;
}

class chordModule
{
public:
  chordModule(const ip_port& _selfAddr,
              const ip_port& _joinAddr,
              size_t (*_hash)(const std::string&) = NULL);
  ~chordModule();
  void lookup(const std::string& key, 
              ip_port& hostpeer,
              const ip_port& requester);
  void processRequest(const std::string& req);
private:
  void join(const ip_port& contact);
  void leave(const ip_port& contact);
  ip_port selfAddr;
  ip_port prevAddr;
  ip_port nextAddr;
  std::vector<ftentry> fingerTable;
  size_t (*hash)(const std::string&);
  Syncobj* chordso;
};

} //end namespace chordModuleNS

#endif
