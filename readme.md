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

