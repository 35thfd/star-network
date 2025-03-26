#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0  // macOS 没有 MSG_CONFIRM，用 0 代替
#endif

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <csignal>
#include <arpa/inet.h>
#include <unistd.h>
#include "satellite.h"

using namespace std;

#define MAX_FRAGMENTS 100
#define SLEEP_TIME 10
#define FRAGMENT_SIZE 512
#define BUFFER_SIZE (sizeof(int) + FRAGMENT_SIZE)
#define BASE_STATION_IP "127.0.0.1" 
#define Neighbour_IP "127.0.0.1" 
#define BASE_STATION_PORT 8080
#define SATELLITE_PORT 9999 
#define Nei_PORT 10010

volatile int exit_flag = 0;

int received_count = 0;

void send_control_info(int sockfd, struct sockaddr_in *des_addr, int *missing_blocks, int missing_count){
    //分离出发送控制信息的函数，代表的含义是目前自己所拥有的
    int temp_i = 1;
    char buffer[sizeof(int) * 2 + MAX_FRAGMENTS * sizeof(int)];
    memcpy(buffer, &temp_i, sizeof(int));  
    memcpy(buffer + sizeof(int), &missing_count, sizeof(int)); 
    memcpy(buffer + sizeof(int) * 2, (missing_blocks+1), missing_count * sizeof(int));
    cout << "Control info sent: ";
    for (size_t i = 0; i < sizeof(int) * 2 + missing_count * sizeof(int); i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
    sendto(sockfd, buffer, sizeof(int) * 2 + missing_count * sizeof(int), MSG_CONFIRM, 
           (const struct sockaddr *)des_addr, sizeof(*des_addr));

    printf("Sent control info: %d fragments\n", missing_count);
}

void send_to_base_station(int sockfd, int* missing_blocks, int missing_count){
    struct sockaddr_in base_station_addr;

    memset(&base_station_addr, 0, sizeof(base_station_addr));
    base_station_addr.sin_family = AF_INET;
    base_station_addr.sin_port = htons(BASE_STATION_PORT);
    if (inet_pton(AF_INET, BASE_STATION_IP, &base_station_addr.sin_addr) <= 0) {
        perror("Invalid base station address");
        exit(EXIT_FAILURE);
    }

    send_control_info(sockfd, &base_station_addr, missing_blocks, missing_count);
}

void send_to_neighbor_satellite(int sockfd, int* missing_blocks, int missing_count, int neighborcount, Satellite neighbors[]){
    struct sockaddr_in neighbor_satellite_addr;
    for(int i = 0; i < neighborcount; i ++)
    {
        neighbors[i];
        memset(&neighbor_satellite_addr, 0, sizeof(neighbor_satellite_addr));
        neighbor_satellite_addr.sin_family = AF_INET;
        //todo 修改端口号和ip地址
        neighbor_satellite_addr.sin_port = htons(Nei_PORT);
        if (inet_pton(AF_INET, Neighbour_IP, &neighbor_satellite_addr.sin_addr) <= 0) {
            perror("Invalid neighbor_satellite_addr address");
            exit(EXIT_FAILURE);
        }

        send_control_info(sockfd, &neighbor_satellite_addr, missing_blocks, missing_count);
    }
}

//TODO:更改报文结构，维护第一位为报文区分
void* receive_data(void *arg){
    //接受数据包的receive代码
    Sate *satellite = (Sate *) arg;
    char control_buffer[10000];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);

    while (1) {
        int n = recvfrom(satellite->sockfd, control_buffer, BUFFER_SIZE, MSG_WAITALL,  (struct sockaddr *)&source_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        /*TODO：修改格式  [4字节 报文类型] + [4字节 分片编号] + [实际分片数据]，
        分离两个函数（或在我这继续写）一个是接受数据包信息，一个是接受其他卫星发来的请求控制信息*/
        
        //将control_buffer中的信息代码读出来
        int kind;
        memcpy(&kind, control_buffer, sizeof(int)); 
        printf("data kind : %d\n ", kind);
        //int fragment_id;
        //memcpy(&fragment_id, control_buffer + sizeof(int), sizeof(int));  // 解析分片编号
        //printf("%d\n", fragment_id);
        char *buffer = NULL;
        buffer = control_buffer + sizeof(int);
        switch (kind)
        {
        //case 0 接收到的是数据包信息
        //发送的数据格式：[4字节 分片编号] + [实际分片数据]
        case 0:
            {
                int fragment_id;
                memcpy(&fragment_id, control_buffer + sizeof(int), sizeof(int));  // 解析分片编号
                char *data = buffer + sizeof(int);  // 解析分片数据
                printf("Received fragment %d from base station\n", fragment_id);
                
                int a;
                if(satellite->missing_count != 0)  
                    a = satellite->missing_blocks[satellite->missing_count];

                for(int i = 1; i <= satellite->missing_count; i++)
                {
                    if(satellite->missing_blocks[i] == fragment_id)
                    {
                        satellite->missing_blocks[i] = a;
                        satellite->missing_count--;
                        memcpy(satellite->data[fragment_id], data, 1024); 
                        //使用卫星见使用发送进程定期维护彼此数据保持有信息之后，在数据包信息发生变化后才发送请求信息
                        //数据包信息改变后立刻与所有连接的基站或其他卫星进行update
                        send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
                        //send_to_neighbor_satellite(satellite->sockfd, satellite->missing_blocks, satellite->missing_count, satellite->neighbor_count, satellite->neighbors);
                        break;
                    }
                }
            }
            break;
        //case 1 接收到的是其他卫星的数据流信息
        //默认能发过来控制流信息的卫星都是自己的邻居，已经建立连接的邻居
        case 1:
            {
                // if()
                // {
                //     int blocknum;
                //     memcpy(&blocknum, buffer, sizeof(int));  // 解析分片编号
                //     satellite->missing_count = blocknum;
                //     for(int i = 1; i <= blocknum; i ++)
                //     {
                //         satellite->missing_blocks[i] = i;
                //     }
                // }
            }
            break;

        //case 2 接受到的是数据包数量信息
        case 2:
            {
                int blocknum;
                memcpy(&blocknum, buffer, sizeof(int));  // 解析分片编号
                satellite->missing_count = blocknum;
                for(int i = 1; i <= blocknum; i ++)
                {
                    satellite->missing_blocks[i] = i;
                }
                break;
            }
        // 卫星应该有自己维护星间路由的算法，这个应该已经由502所实现好了，所以这部分信息单独拿出一个控制序号仅用来模拟卫星间的连接，维护邻居信息
        // 也可以和数据请求信息结合
        // 报文只有控制序号，没有其他信息，需要记录recvfrom获得的addr
        case 3:
            {
                
            }
        }
        
    }
    return NULL;
}

void* send_data(void *arg){
    Sate *satellite = (Sate *) arg;
    while(1){
        if(satellite->missing_count != -1){
            send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
            //发送连接请求
        }
        sleep(SLEEP_TIME);
    }
    return NULL;
}

void signal_handler(int signum) {
    printf("\nTerminating satellite...\n");
    exit_flag = 1;
}

int main(){
    //printf("hi\n");
    while(1){
        printf("running\n");
        fflush(stdout); 
        sleep(1);
    }
    struct sockaddr_in satellite_addr;
    Sate *satellite = (Sate*) malloc(sizeof(Sate));  // ✅ 确保 satellite 被分配内存

    if (!satellite) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    satellite->type = SATELLITE_CIRCLE;
    satellite->initialize();

    if ((satellite->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&satellite_addr, 0, sizeof(satellite_addr));
    satellite_addr.sin_family = AF_INET;
    satellite_addr.sin_addr.s_addr = INADDR_ANY;
    satellite_addr.sin_port = htons(SATELLITE_PORT);

    if (bind(satellite->sockfd, (const struct sockaddr *)&satellite_addr, sizeof(satellite_addr)) < 0) {
        perror("Bind failed");
        close(satellite->sockfd);
        exit(EXIT_FAILURE);
    }

    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_data, (void*)satellite) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(receive_thread);


    //进行与基站的第一次互通
    //把发送代码放入线程
    //使用每隔一段时间就发送一次数据的方式发送请求信息
    //初步使用有来自发送处的返回数据包信息就意味着是卫星的一个neighbor
    //后续可以优化为星间路由代码
    pthread_t send_thread;
    if (pthread_create(&send_thread, NULL, send_data, (void*)satellite) != 0) {
        perror("Failed to create send thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(send_thread);
    //send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
    
    //发送心跳报文，维护邻居信息
    pthread_t heartbeat_thread;
    // if (pthread_create(&heartbeat_thread, NULL, send_data, (void*)satellite) != 0) {
    //     perror("Failed to create send thread");
    //     exit(EXIT_FAILURE);
    // }
    // pthread_detach(heartbeat_thread);


    signal(SIGINT, signal_handler);
    while (1) {
        printf("hello\n");
        sleep(1);
    }
    printf("Satellite shutting down...\n");
    close(satellite->sockfd);
    free(satellite);
    return 0;
}