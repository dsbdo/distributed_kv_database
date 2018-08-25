# class 接口说明
## Communicate 接口说明：
1. 该类是用来负责两台计算机的通信，Communicate* comm = new Communicate (remote_ip, remote_port); 
创建一个tcp连接。（已经过测试）
2. 通过string ackmsg  = comm->sendString(str_content); 可以收到服务器响应的信息（已经过测试）
3. comm->sendStringNoBlock(args)；用以创建异步的tcp发送与信息监听（未经过测试）


## Server 接口说明
1. 该类用以创建绑定端口。进行tcp连接监听
2. Server* ser = new Server(local_ip, local_port);该类是纯虚类，必须被继承才可以被使用。
3. ser->acceptConnect(); 这个接口是在启动服务器后，监听来自其他进程或者其他计算机的tcp连接信息
4. ser->getPort() 获取该server绑定的端口
5. ser->getIP() 获取该server设定的ip
6. ser->getServerName() 获取server所在机器的机器名称
7. ser->getSocketFd() 获取套接字文件描述符