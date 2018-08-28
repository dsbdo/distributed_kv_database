#include "Communicate.h"

Communicate::Communicate(const char *remote_ip, const uint16_t remote_port)
{
    try
    {
        //若connect失败则该套接字不再可用，必须关闭，我们不能对这样的套接字再次调用connect函数
        //在每次connect失败后，都必须close当前套接字描述符并重新调用socket。
        //如果连接成功，返回0 否则返回 -1
        //套接字分配失败
        if ((m_iSocket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            std::cout << "\033[31m Link remote ip: " << remote_ip << " remote port: " << remote_port << "  error" << std::endl;
            std::cout << "reason is socket alloc fail \033[0m" << std::endl;
            throw K_SOCKET_CONSTRUCT_ERROR;
        }

        memset(&m_server_address, 0, sizeof(sockaddr_in));
        m_server_address.sin_family = AF_INET; // ipv4 协议(一般用AF_INET表示tcp/ip协议,int类型)
        //p 标识presentation n表示number，将点分十进制转换为二进制，如果ip地址出错呢
        inet_pton(AF_INET, remote_ip, (void *)&m_server_address.sin_addr);
        //intel 架构均是小端序存储，库文件实现的估计也是小端序，这里是转换成网络字节流（大端序）
        m_server_address.sin_port = htons(remote_port);
        if (connect(m_iSocket_fd, (sockaddr *)&m_server_address, sizeof(sockaddr)) == 0)
        {
            //成功连接， 这里的连接行为是阻塞型，知道connect函数返回才停止
            m_bIsGoodConnect = true;
        }
        else
        {

            m_bIsGoodConnect = false;
        }
    }
    catch (int err_num)
    {
        throw;
    }
}

Communicate::~Communicate()
{
    //如果还有没完成的线程，应当一并关停
    //未完成
    if (close(m_iSocket_fd) == -1)
    {
        throw K_SOCKET_CLOSE_ERROR;
    }
}

std::string Communicate::sendString(const char *str)
{
    if (!m_bIsGoodConnect)
    {
        std::cerr << "\033[31m error: bad connection for client. errno=" << errno << " \033[0m" << std::endl;
        return "connection error!";
    }

    ThreadVar *thread_var = new ThreadVar(2L, 1L, 0L);
    std::string ackmsg;

    std::vector<void *> thread_arg; //发送的内容

    //需要发送的字符串
    thread_arg.push_back((void *)str);
    thread_arg.push_back((void *)&m_iSocket_fd);
    thread_arg.push_back((void *)thread_var);
    thread_arg.push_back((void *)&ackmsg);

    int thread_create_result = 0;
    //创建发送线程
    thread_create_result = pthread_create(&(thread_var->m_thread_obj_arr[0]), 0, &send_thread, (void *)&thread_arg);
    if (thread_create_result != 0)
    {
        throw K_THREAD_ERROR;
    }
    //阻塞当前线程直至其完成
    if (pthread_join(thread_var->m_thread_obj_arr[0], 0) != 0)
    {
        throw K_THREAD_ERROR;
    }

    //创建接受线程
    thread_create_result = pthread_create(&(thread_var->m_thread_obj_arr[1]), 0, &recv_thread, (void *)&thread_arg);
    if (thread_create_result != 0)
    {
        throw K_THREAD_ERROR;
    }
    //阻塞当前线程，直至接受线程完成
    if (pthread_join(thread_var->m_thread_obj_arr[1], 0) != 0)
    {
        throw K_THREAD_ERROR;
    }

    //删除线程相关变量内容
    delete thread_var;
    return ackmsg;
}

//用来被通知已经收到的函数  就是记录一下client发过的数据，下面在这个之下创建线程的操作？？？
void Communicate::sendStringNoBlock(const char *req, ThreadVar *client_thread_var, int *num_done, int *num_total, std::string *levelDB_back)
{
    if (!m_bIsGoodConnect)
    {
        std::cerr << "\033[31m error: bad connection for client. errno=" << errno << " \033[0m" << std::endl;
        return;
    }
    pthread_t main_thread_obj;
    std::vector<void *> *argv = new std::vector<void *>;
    argv->push_back((void *)req);
    argv->push_back((void *)client_thread_var);
    argv->push_back((void *)num_done);
    argv->push_back((void *)this);
    argv->push_back((void *)num_total);
    argv->push_back((void *)levelDB_back);
    if (pthread_create(&main_thread_obj, 0, &main_thread, (void *)argv) != 0)
        throw K_THREAD_ERROR;
    return;
}

bool Communicate::isGoodConnect()
{
    return m_bIsGoodConnect;
}

void *Communicate::send_thread(void *arg)
{
    //arg 是一个vector 变量    point
    //arg[0] = send_info     string
     //arg[1] = socket_info  int 
    //arg[2] = thread_var;   class
    //arg[3] = ackmsg        string
    //ssize_t ==> signed size_t
    ssize_t byte_sent = -1;
    char buf[K_BUF_SIZE];

    std::vector<void *> arg_content = *(std::vector<void *> *)arg; //拷贝一份内容给arg_content
    int sock_fd = *(int *)arg_content[1];
    pthread_mutex_t *sock_mutex = &(((ThreadVar *)arg_content[2])->m_mutex_arr[0]);
    try
    {
        if (!*(char *)arg_content[0])
        {
            //信息为空：
            std::cerr << "\033[31m error: send string is null for client. errno=" << errno << " \033[0m" << std::endl;
            throw K_FILE_IO_ERROR;
        }

        //加 1 保证字符创最后能有一个 \0
        size_t str_remind_len = strlen((char *)arg_content[0]) + 1;
        char *temp = const_cast<char *>((char *)arg_content[0]); //const_cast 提供强制转换，消除const
        while (str_remind_len > 0)
        {
            //如果要传送的内容过大，那就分多次传送
            size_t copy_size = (str_remind_len > K_BUF_SIZE) ? K_BUF_SIZE : str_remind_len;
            memset(buf, 0, K_BUF_SIZE);
            memcpy(buf, temp, copy_size);
            //写进套接字
            pthread_mutex_lock(sock_mutex); //现在套接字正在使用中，禁止其他人使用
            byte_sent = write(sock_fd, buf, copy_size);
            if (byte_sent < 0)
            {
                std::cerr << "\033[31m error: client send string fail errno=" << errno << " \033[0m" << std::endl;
                throw K_FILE_IO_ERROR;
            }
            pthread_mutex_unlock(sock_mutex);//使用完套接字了，解锁
            temp += byte_sent;//将temp中的字符串移动到后面还没发送的部分，这个直接移动数组指针的方法是个好方法
            str_remind_len -= byte_sent;
        }


        //在这里创建一个接收线程，并处理
        // char temp_buf[1024];
        // int recvbytes = 0;
        // if ((recvbytes = recv(sock_fd, temp_buf, 1024, 0)) == -1)
        // {
        //     perror("recv出错！");
        //     return 0;
        // }
        // temp_buf[recvbytes] = '\0';
        // printf("Received: %s", temp_buf);
    }
    catch (int e)
    {
        throw;
    }
    return 0;
}

void *Communicate::recv_thread(void *arg)
{
    std::cout << "Come in recv thread " << std::endl;
    char buf[K_BUF_SIZE];
    ssize_t byte_read = -1;
    std::vector<void *> arg_content = *(std::vector<void *> *)arg;
    std::cout << "str info is: " << *(char*)arg_content[0] << std::endl;
    int sock_fd = *(int *)arg_content[1];
    std::cout << "sock_fd is: " << sock_fd << std::endl;
    pthread_mutex_t *sock_mutex = &(((ThreadVar *)arg_content[2])->m_mutex_arr[0]);
    pthread_mutex_lock(sock_mutex);
    bool done = false;
    std::string* ackmsg = (std::string *)arg_content[3];
    while (!done)
    {
        memset(buf, 0, K_BUF_SIZE);
        byte_read = read(sock_fd, buf, K_BUF_SIZE); //当没有东西进来的时候，会自动阻塞当前线程，让它睡眠
        //这里的byte_read 感觉相当危险，容易数组越界
        if (buf[byte_read] == '\0')
        {
            
            done = true;
           
        }
         *ackmsg += std::string(buf);   //监听sock_fd中的内容，把sock_fd中的内容添加到ack中，这样子每一组send,socket_fd,thread_var,ack就是一起的
        std::cout << "DEBUG log is: " << buf << std::endl;
    }
    pthread_mutex_unlock(sock_mutex);

    if (byte_read == -1)
    {
        std::cerr << "\033[31m error waiting for ack errno=" << errno << " \033[0m" << std::endl;
    }
    std::cout << "leave out the recv thread" << std::endl;  
    return 0;
}

void *Communicate::main_thread(void *arg)
{
    std::vector<void *> arg_content = *(std::vector<void *> *)arg;
    char *req = (char *)arg_content[0];
    ThreadVar *client_thread_var = (ThreadVar *)arg_content[1];
    int *numdone = (int *)(arg_content[2]);
    Communicate *handle = (Communicate *)(arg_content[3]);
    int *numtotal = (int *)(arg_content[4]);
    std::string *levelDB_back = (std::string *)(arg_content[5]);

    pthread_cond_t &cv = client_thread_var->m_cv_arr[0];
    pthread_mutex_t &cv_mutex = client_thread_var->m_mutex_arr[0];

    *levelDB_back = handle->sendString(req); //this blocks

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(*levelDB_back, root))
    {
        printf("error: json parser failed at client::main_thread\n");
        throw K_THREAD_ERROR;
    }
    std::string status = root["status"].asString();

    if (pthread_mutex_lock(&cv_mutex) != 0)
        throw K_THREAD_ERROR;
    (*numdone)++;
    bool alldone = (*numdone == *numtotal);
    if (pthread_mutex_unlock(&cv_mutex) != 0)
        throw K_THREAD_ERROR;
    if (status == "OK" || alldone) //alldone will reactivate cleanup thread
    {
        if (pthread_cond_signal(&cv) != 0)
            throw K_THREAD_ERROR;
    }
}