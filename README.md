# distributed_kv_database
A distributed key value database for database training design.
## 项目编译运行说明
项目依赖库有两个，一个是 levelDB， 另外一个是jsoncpp
    levelDB 下载地址： https://github.com/google/leveldb/releases
    jsoncpp 下载命令： sudo apt install libjsoncpp
相关库下载结束之后，执行make命令即可。
#测试说明
# 系统测试报告，运行截图， 与用户手册
## 系统测试报告
### 测试目标
- 检查程序是否可以正常运行，各个服务器能否正常开启并监听网络请求。
- 检查gateServer(网关服务器，自己起的名字)能否正常开启并监听来自clusterServer 与 client 的网络请求。该服务器主要用以接收客户端请求，并转发到clusterServer(集群服务器)。
- 检查clusterServer(集群服务器)能否正常开启并监听来自gateServer 与 levelDBServer 的请求。该服务器主要接收gateServer的请求，并将信息广播到集群中的所有levelDB服务器。
- 检查levelDBServer(数据库服务器)能否接收来自clusterServer的请求，主要作用完成数据库增删查改的功能。



### 一、测试gateServer
> gateServer服务器启动命令如下：(不指定参数，则默认作为master节点，gateServer 从本地启动，绑定端口为9001；clusterServer 从本地启动，绑定端口为8001)
```
./gateserver.out  --joip 127.0.0.1 --joinport 8001 --gsport 9001 --csport 8001
```
> 启动结果如下:
![](./picture/运行截图/gs.png)


### 二、测试clusterServer集群服务器
> 集群服务器主要用来管理集群中的levelDBServer，主要用以管理数据库服务器的加入与离开，全局服务器信息的同步，接受来自gateServer的请求，并且负责转发并同步到集群内所有levelDB服务器中。同时clusterServer中会根据最先申请原则选举出一个master集群服务器节点，主要负责用以向向全局中的levelDB服务器与clusterServer发送心跳包（heartbeat），用来确定与更新全局在线服务器的信息。
集群服务器的启动是在启动gateServer时一并启动的。下面是全局机器信息同步时，发送心跳包的截图：
![](../picture/运行截图/heartBeat.png)

### 三、测试levelDB服务器
> levelDB服务器主要用以加入集群，同时用以存储来自客户端的消息。启动命令如下：（--clusterport 要加入的集群服务器的端口， --clusterip 要加入集群的服务器ip地址； --selfip 自身的ip， --selfport 自身的端口）
```
./leveldbserver.out --clusterport 8001 --selfport 7001 --clusterip 127.0.0.1 --selfip 127.0.0.1 --dbdir ./../DB/db1
```
> 启动后，结果截图如下：
![](../picture/运行截图/addLevelDb.png)
### 四、测试客户端
> 客户端主要目的在于向gateServer 发起请求，并将准备存储的信息以 JSON 格式发送给gateServer, 由其转发给集群服务器。这个的启动命令便是直接运行我们的客户端测试程序，同时指定gateServer的ip与端口：
```
./client.out 127.0.0.1 9001
```
> 启动结果截图：
![](../picture/运行截图/clientTest.png)

### 五、运行截图与用户手册
> 运行截图均在上面的测试报告中，因为时间比较紧，我们没有完成用户GUI客户端，就只有命令行，所以用户手册也就是上面的服务器启动命令与客户端测试命令。


## 使用说明
未完，待续...