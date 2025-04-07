目前进行的部分包括
1、基站类的定义与基站类中的函数封装
2、基站收发实现基于udp
3、使用pthread_t分离线程，目的：处理多卫星接入基站情况
4、基站负载均衡算法
5、基站解析卫星数据包，及发送数据包选择算法check
6、实现了卫星接受与发送报文的功能
7、实现了卫星对数据包的存储
8、卫星发送现有信息情况到基站
9、卫星处理频次统计逻辑
10、对卫星类型的定义


接收到心跳报文后更新当前路由表，之后发送自己缺少的数据包，1、接收到返回ack，发送方
                                                2、不管这么多直接发

一个进行发现的线程，不断的发现卫星，维护邻居结点 or TCP？
1、a发送广播报文，包含自己的分片信息（拥有分片数）
2、b接收广播报文，记入邻居，如果不在自己的neighbor里，发送ack，发送请求报文，记录a的分片信息
3、a接受ack，记入邻居，发送请求报文


接收到报文，拥有报文发送变化后，发送请求报文


数据报文不发ack，因为如果接收到了，请求数据包会发生改变


todo
卫星发现广播消息，每隔一定间隔时间主动发送广播信息
维护卫星的路由表，基站不需要，别人向基站发送的信息基站才考虑传输
路由状态机
卫星与卫星之间的传输
数据包报文类型的添加及处理
维护卫星neighbor
数据包信息改变后立刻与所有连接的基站或其他卫星进行update
卫星根据自己被定义的职责进行相关操作


注意主机序、字节序
502应该给我们每个卫星的ip地址



需要添加数最初的基站通报卫星数据分片数量，可以定义一个新的报文类型

0-数据报文
1-来自其他卫星的控制报文
2-基站通报卫星数据分片数量

