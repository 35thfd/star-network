// #ifndef MSG_CONFIRM
// #define MSG_CONFIRM 0  // macOS 没有 MSG_CONFIRM，用 0 代替
// #endif

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <csignal>
#include "satellite.h"

using namespace std;

#define MAX_FRAGMENTS 100
#define SLEEP_TIME 10
#define FRAGMENT_SIZE 512
#define BUFFER_SIZE (sizeof(int) + FRAGMENT_SIZE)
#define BASE_STATION_IP "192.168.1.5" 
//#define Neighbour_IP "127.0.0.1" 
#define BASE_STATION_PORT 8080
#define SATELLITE_PORT 9999 
#define Nei_PORT 10010

volatile int exit_flag = 0;

int received_count = 0;

void get_broadcast_address(char *interface_name, char *broadcast_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0'; 
    // 获取广播地址
    if (ioctl(sock, SIOCGIFBRDADDR, &ifr) < 0) {
        perror("ioctl failed to get broadcast address");
        close(sock);
        return;
    }

    struct sockaddr_in *bcast = (struct sockaddr_in *)&ifr.ifr_broadaddr;
    strcpy(broadcast_ip, inet_ntoa(bcast->sin_addr));

    close(sock);
}

void send_control_info(int sockfd, struct sockaddr_in *des_addr, int *missing_blocks, int missing_count){
    //分离出发送控制信息的函数，代表的含义是目前自己所拥有的
    int temp_i = 1;
    char buffer[sizeof(int) * 2 + MAX_FRAGMENTS * sizeof(int)];
    memcpy(buffer, &temp_i, sizeof(int));  
    memcpy(buffer + sizeof(int), &missing_count, sizeof(int)); 
    memcpy(buffer + sizeof(int) * 2, (missing_blocks+1), missing_count * sizeof(int));
    cout << "Control info sent: ";
    for (int i = 0; i < sizeof(int) * 2 + missing_count * sizeof(int); i++) {
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

void send_to_neighbor_satellite(int sockfd, int* missing_blocks, int missing_count, int neighborcount, Satellite neighbors[]) {
    struct sockaddr_in neighbor_satellite_addr;

    for (int i = 0; i < neighborcount; i++) {
        memset(&neighbor_satellite_addr, 0, sizeof(neighbor_satellite_addr));
        neighbor_satellite_addr.sin_family = AF_INET;
        neighbor_satellite_addr.sin_port = htons(neighbors[i].neighbor_ports);  // 目标端口

        // 将字符串 IP 地址转换为 sockaddr 结构
        if (inet_pton(AF_INET, neighbors[i].neighborip, &neighbor_satellite_addr.sin_addr) <= 0) {
            perror("Invalid neighbor IP address");
            continue;  // 跳过这个邻居，继续下一个
        }

        // 发送控制信息
        send_control_info(sockfd, &neighbor_satellite_addr, missing_blocks, missing_count);
        printf("Sent control info to neighbor: %s:%d\n", neighbors[i].neighborip, neighbors[i].neighbor_ports);
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
                printf("Received fragment %d from %d\n", fragment_id, ntohs(source_addr.sin_port));
                
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
                        if(source_addr.sin_port == htons(BASE_STATION_PORT)&&source_addr.sin_addr.s_addr == inet_addr(BASE_STATION_IP))
                        {
                            //向基站发送请求
                            send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
                        }
                        else
                        {
                            //向邻居卫星发送请求
                            send_control_info(satellite->sockfd, &source_addr, satellite->missing_blocks, satellite->missing_count);
                        }
                        break;
                    }
                }
            }
        break;
        //case 1 接收到的是其他卫星的数据流信息
        //默认能发过来控制流信息的卫星都是自己的邻居，已经建立连接的邻居
        case 1:
        {
            int cnt;
            memcpy(&cnt, buffer, sizeof(int));  // 解析分片数量
            printf("Received control message from neighbor satellite, cnt: %d\n", cnt);

            int *missing_fragments = (int *)malloc(cnt * sizeof(int));
            memcpy(missing_fragments, buffer + sizeof(int), cnt * sizeof(int));

            // 打印 missing_fragments 数组
            // printf("missing_fragments: ");
            // for (int i = 0; i < cnt; i++) {
            //     printf("%d ", missing_fragments[i]);
            // }
            // printf("\n");

            int fragment_id = satellite->check(missing_fragments, cnt);
            printf("choose fragment %d to send\n", fragment_id);
            if (fragment_id != -1) {
                size_t data_size = DATA_SIZE;
                char send_buffer[sizeof(int) * 2 + data_size];

                int control_flag = 0;  // 数据报文
                memcpy(send_buffer, &control_flag, sizeof(int));
                memcpy(send_buffer + sizeof(int), &fragment_id, sizeof(int));
                memcpy(send_buffer + sizeof(int) * 2, satellite->data[fragment_id], data_size);

                // 输出数据包内容，确保数据正确
                // printf("Sending packet (size = %lu): ", sizeof(int) * 2 + data_size);
                // for (size_t i = 0; i < sizeof(int) * 2 + data_size; i++) {
                //     printf("%02x ", (unsigned char)send_buffer[i]);
                // }
                // printf("\n");

                sendto(satellite->sockfd, send_buffer, sizeof(int) * 2 + data_size, MSG_CONFIRM, 
                    (const struct sockaddr *)&source_addr, sizeof(source_addr));
            }

            free(missing_fragments);
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
                    satellite->missing_blocks[i] = i;//初始化
                }
            }
            break;

        // 卫星应该有自己维护星间路由的算法，这个应该已经由502所实现好了，所以这部分信息单独拿出一个控制序号仅用来模拟卫星间的连接，维护邻居信息
        // 也可以和数据请求信息结合
        // 报文只有控制序号，没有其他信息，需要记录recvfrom获得的addr
        case 3:
            {
                // 解析发送方地址
                char sender_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(source_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
                int sender_port = ntohs(source_addr.sin_port);

                printf("Received broadcast message from %s:%d\n", sender_ip, sender_port);

                bool exists = false;
                for (int i = 0; i < satellite->neighbor_count; i++) {
                    if (strcmp(satellite->neighbors[i].neighborip, sender_ip) == 0 && 
                        satellite->neighbors[i].neighbor_ports == sender_port) {
                        exists = true;
                        break;
                    }
                }

                // 如果是新邻居，则记录
                if (!exists) {
                    if (satellite->neighbor_count < MAX_NEIGHBORS) {
                        strncpy(satellite->neighbors[satellite->neighbor_count].neighborip, sender_ip, INET_ADDRSTRLEN);
                        satellite->neighbors[satellite->neighbor_count].neighbor_ports = sender_port;
                        satellite->neighbor_count++;
                        printf("New neighbor added: %s:%d\n", sender_ip, sender_port);
                    } else {
                        printf("Neighbor list full, cannot add new neighbor.\n");
                    }
                    //开始发送第一次请求
                    send_control_info(satellite->sockfd, &source_addr, satellite->missing_blocks, satellite->missing_count);
                    //send_to_neighbor_satellite(satellite->sockfd, satellite->missing_blocks, satellite->missing_count, satellite->neighbor_count, satellite->neighbors);
                }
            }
            break;
        }
        
    }
    return NULL;
}

void* send_data(void *arg){
    Sate *satellite = (Sate *) arg;
    while(1){
        if(satellite->missing_count != -1){
            send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
            //能不能检测
            //发送连接请求
            //向其他已经连接的卫星发送请求
            //send_to_neighbor_satellite(satellite->sockfd, satellite->missing_blocks, satellite->missing_count, satellite->neighbor_count, satellite->neighbors);
        }
        sleep(SLEEP_TIME);
    }
    return NULL;
}

void* send_heartbeat(void *arg){
    printf("send_heartbeat\n");
    Sate *satellite = (Sate *) arg;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    int heartbeat_type = 3;  // 心跳报文类型

    // 配置广播地址
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(SATELLITE_PORT); // 目标端口

    // 使用广播地址 192.168.1.255
    broadcast_addr.sin_addr.s_addr = inet_addr("192.168.1.255"); // 广播地址

    // 允许 UDP 套接字进行广播
    if (setsockopt(satellite->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Error setting broadcast option");
        pthread_exit(NULL);
    }

    while (1) {
        // 发送心跳报文
        int bytes_sent = sendto(satellite->sockfd, &heartbeat_type, sizeof(int), MSG_CONFIRM,
                                (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        if (bytes_sent < 0) {
            perror("Heartbeat send failed");
        } else {
            printf("Heartbeat broadcast sent.\n");
        }

        sleep(SLEEP_TIME);
    }
    return NULL;
}

// void signal_handler(int signum) {
//     printf("\nTerminating satellite...\n");
//     exit_flag = 1;
// }

int main(){
    struct sockaddr_in satellite_addr;
    Sate *satellite = (Sate*) malloc(sizeof(Sate)); 

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
    // pthread_t send_thread;
    // if (pthread_create(&send_thread, NULL, send_data, (void*)satellite) != 0) {
    //     perror("Failed to create send thread");
    //     exit(EXIT_FAILURE);
    // }
    // pthread_detach(send_thread);
    send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
    
    //发送心跳报文，维护邻居信息
    pthread_t heartbeat_thread;
    if (pthread_create(&heartbeat_thread, NULL, send_heartbeat, (void*)satellite) != 0) {
        perror("Failed to create send thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(heartbeat_thread);


    //signal(SIGINT, signal_handler);
    while (!exit_flag) {
        sleep(1);
    }
    printf("Satellite shutting down...\n");
    close(satellite->sockfd);
    free(satellite);
    return 0;
}