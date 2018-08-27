#ifndef _COMMON_H_
#define _COMMON_H_
#include <jsoncpp/json/json.h>
#include <leveldb/db.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include<algorithm>
#include<unistd.h>
//这一个系统各种变量的基本值
#include <sys/types.h>
//这一个是系统socket的头文件
#include <sys/socket.h>
//sockaddr_in
#include <netinet/in.h>

//inet_pton, htons, ...
#include <arpa/inet.h> 
//gethostbyname
#include <netdb.h> 

//维护各种错误值的标识
#include<errno.h>

//用以获取本机ip port 信息
#include <ifaddrs.h>

//与系统信号相关的头文件
#include <signal.h>
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);

inline size_t hash(std::string& s)
{
  size_t len = s.length();
  size_t val = 0;
  for (int i=0; i<len; i++)
    val += (size_t)s[i];
  val %= K_MAX_CLUSTER;
  return val;
}

const u_int16_t K_SOCKET_CONSTRUCT_ERROR = 1;
const uint16_t K_SOCKET_BIND_ERROR = 2;
const uint16_t K_SOCKET_LISTEN_ERROR = 4;
const uint16_t K_SOCKET_ACCEPT_ERROR = 5;
const u_int16_t K_SOCKET_CLOSE_ERROR = 8;

const uint16_t K_THREAD_ERROR = 11;
const uint16_t K_DB_FAIL = 12;
const uint16_t K_FILE_IO_ERROR = 7;
const int K_BUF_SIZE = 99999;
const uint K_MAX_CLUSTER = 16;

const uint16_t K_MAX_CONNECT = 5;
//每十秒发送一个心跳包
const uint16_t K_HEARTBEAT_RATE = 10;
#endif
