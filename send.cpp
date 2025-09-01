#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <csignal>
#include <string>
#include <sstream> 
#include "satellite.h"

void* send_data(void *arg){
    //读取文件



    //获取当前时间戳判断是否可以连接，当前时间戳进入设定时间内，设置卫星到已连接数组中
    //当时间超出连接时间，将其删掉


    //遍历所有建立连接的卫星，或基站



    //1、发送缺失频次优先列表，不是稳定邻居的发送方法


    //2、发送缺失分片，是稳定邻居的发送方法



    return NULL;
}