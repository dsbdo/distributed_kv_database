#include "Server.h"
Server::Server(const char *local_ip, const uint16_t local_port)
{
    try
    {
        //启动服务器
        if ((m_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            throw K_SOCKET_CONSTRUCT_ERROR;
        }
        int option_value = 1;
        //设置通用套接字，允许地址重用
        setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&option_value, sizeof(int));

        memset(m_host_name, 0, 256);
        //系统函数，获取机器名称
        gethostname(m_host_name, 256);
        memset(&m_server_address, 0, sizeof(sockaddr_in));

        m_server_address.sin_family = AF_INET;
        m_server_address.sin_port = htons(local_port);

        if (!local_ip)
        {
            //如果没有指定ip，将从本地获取ip地址
            bool is_found_ip = false;
            ifaddrs *ifa_addr_struct = NULL;
            ifaddrs *ifa = NULL;
            getifaddrs(&ifa_addr_struct);
            std::cout << "获取的网卡名称是： " << ifa_addr_struct->ifa_name << std::endl;
            //遍历网卡
            for (ifa = ifa_addr_struct; ifa != NULL; ifa = ifa->ifa_next)
            {
                //ipv4 eth0 或者 lo
                if (ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "lo") == 0)
                {
                    //仅处理ipv4, 且目前支持本地跑
                    m_server_address.sin_addr = ((sockaddr_in *)ifa->ifa_addr)->sin_addr;
                    is_found_ip = true;
                    break;
                }
            }
            if (!is_found_ip)
            {
                throw K_SOCKET_BIND_ERROR;
            }
        }
        else
        {
            inet_pton(AF_INET, local_ip, (void *)&m_server_address.sin_addr);
            //端口绑定
            if (bind(m_socket_fd, (sockaddr *)&m_server_address, sizeof(sockaddr_in)) == -1)
            {
                throw K_SOCKET_BIND_ERROR;
            }
            //监听端口
            if (listen(m_socket_fd, K_MAX_CONNECT) == -1)
            {
                throw K_SOCKET_LISTEN_ERROR;
            }
        }
    }
    catch (int e)
    {
        throw;
    }
}

Server::~Server()
{
    if (close(m_socket_fd) == -1)
    {
        throw K_SOCKET_CLOSE_ERROR;
    }
}

int Server::acceptConnect()
{
    try
    {
        //：accept函数由TCP服务器调用，用于从完成连接队列头返回下一个已完成连接。如果已完成连接队列为空，那么进程被投入睡眠（假定套接字为默认的阻塞方式）。
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        //client host info
        hostent *client_host_info;
        //点分十进制ip
        char *client_host_addr_p;
        int comm_socket_fd;
        bool is_known_client = true;
        if ((comm_socket_fd = accept(m_socket_fd, (sockaddr *)&client_addr, &client_addr_len)) < 0)
        {
            std::cerr << "\033[31m"
                      << "socket accept error "
                      << "\033[0m" << std::endl;
            throw K_SOCKET_ACCEPT_ERROR;
        }
        //打印信息，同时返回消息$$$$$$$$
        printf("received a connection from %s:%u\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        //打印发送的请求
    //     char temp_buf[1024];
    //     int recvbytes = 0;
    //     if ((recvbytes = recv(comm_socket_fd, temp_buf, 1024, 0)) == -1)
    //     {
    //         perror("recv出错！");
    //         return 0;
    //     }
    //     temp_buf[recvbytes] = '\0';
    //     printf("Received: %s", temp_buf);

    //     //响应客户端
    //     if (send(comm_socket_fd, "Hello, you are connected!\n", 26, 0) == -1)
    //     {
    //         perror("send出错！");
    //     }
    //    // close(comm_socket_fd);
        //打印处理结束 $$$$$$$
        client_host_info = gethostbyaddr((const char *)&client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr), AF_INET);
        if (client_host_info == NULL)
        {
            is_known_client = false;
        }
        else
        {
            client_host_addr_p = inet_ntoa(client_addr.sin_addr);
            if (client_host_addr_p == NULL)
            {
                is_known_client = false;
            }
        }
        if (is_known_client)
        {
            std::cout << "\033[32m server established connection with: " << client_host_info->h_name << " " << client_host_addr_p << "\033[0m" << std::endl;
        }
        //返回socket 的文件描述符
        return comm_socket_fd;
    }
    catch (int e)
    {
        throw;
    }
}

uint16_t Server::getPort()
{
    return ntohs(m_server_address.sin_port);
}

std::string Server::getIp()
{
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, (void *)&m_server_address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip);
}

std::string Server::getServerName()
{
    return std::string(m_host_name);
}
int Server::getSocketFd()
{
    return m_socket_fd;
}