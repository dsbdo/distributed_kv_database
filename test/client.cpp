#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#define MAXDATASIZE 1000
 
using namespace std;
 
int main(int argc, char *argv[]) {
 
    int sock_fd = -1;
    char buf[MAXDATASIZE];
    int recvbytes, sendbytes, len;
 
    if (argc != 5) {
        printf("Input format error\n");
        return 0;
    }
    // 参数解析
    in_addr_t server_ip = inet_addr(argv[1]);
    in_port_t server_port = atoi(argv[2]);
    in_addr_t my_ip = inet_addr(argv[3]);
    in_port_t my_port = atoi(argv[4]);
 
    // 建立socket连接
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        return 0;
    }
    long flag = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
 
    // 綁定自己的ip
    struct sockaddr_in my_addr;
    my_addr.sin_addr.s_addr = my_ip;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);
     
    if (bind(sock_fd, (sockaddr*)&my_addr, sizeof(sockaddr)) < 0) {
        perror("bind error");
        return 0;
    }
 
    // 连接服务器
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = server_ip;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    printf("Try to connect server(%s:%u)\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    // while(0 != connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(sockaddr))) {
    //  usleep(100*1000);
    // }
    if(connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect出错！");
        return 0;
    }  
 
    printf("Connect server success(%s:%u)\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
 
    if((recvbytes=recv(sock_fd, buf, MAXDATASIZE, 0)) == -1) {  
        perror("recv出错！");  
        return 0; 
    }
    buf[recvbytes] = '\0';  
    printf("Received: %s",buf);  
    close(sock_fd);  
    return 0;
}