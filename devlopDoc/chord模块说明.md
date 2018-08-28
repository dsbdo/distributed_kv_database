##chord模块

#chord算法介绍
chord算法是用来在p2p网络中查找特定节点的一种非线性算法
chord算法本质上也是一种路由算法，避免了在机器较多的情况下进行线性查找需要时间较长的弊端

#chord节点建立
在我们的算法中把一个ip_port作为一个节点，每一个节点都有一个直接前继节点predecessor和一个直接后继节点successor
每一个节点都维护一个predecessor和successor列表，该列表的作用是能高速定位前继和后继，并能周期性检測前继和后继的状态
每一个leveldbserver申请加入clusterserver的时候都需要将它与这个clusterserver连接起来，方便下次同步时进行查找
此功能由构造函数chordModule(const ip_port& _selfAddr,const ip_port& _joinAddr,size_t (*_hash)(const std::string&) = NULL)完成

#chord节点查找
lookup函数lookup(const std::string& key, ip_port& hostpeer, const ip_port& requester)功能描述
给定一个Key，按以下的步骤查找其相应的资源位于哪个节点，也就是查找该Key的successor：（假如查找是在节点n上进行）
1. 查看Key的哈希是否落在节点n和其直接successor之间，若是结束查找，n的successor即为所找
2. 在n的Finger表中，找出与hash(Key)距离近期且<hash(Key)的n的successor，该节点也是Finger表中最接近Key的predecessor，把查找请求转发到该节点
3. 继续上述过程，直至找到Key相应的节点