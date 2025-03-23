// #ifndef MSG_CONFIRM
// #define MSG_CONFIRM 0  // macOS 没有 MSG_CONFIRM，用 0 代替
// #endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include "satellite.h"

#define MAX_FRAGMENTS 100
#define FRAGMENT_SIZE 512
#define BUFFER_SIZE (sizeof(int) + FRAGMENT_SIZE)
#define BASE_STATION_IP "192.168.1.100"
#define BASE_STATION_PORT 8888
#define SATELLITE_PORT 9999 

volatile int exit_flag = 0;

typedef struct {
    int fragment_id;
    char data[FRAGMENT_SIZE];
} Fragment;

Fragment received_fragments[MAX_FRAGMENTS];
int received_count = 0;

void send_control_info(int sockfd, struct sockaddr_in *des_addr, int *missing_blocks, int missing_count){
    //分离出发送控制信息的函数，代表的含义是目前自己所拥有的
    char buffer[sizeof(int) + MAX_FRAGMENTS * sizeof(int)];
    memcpy(buffer, &missing_count, sizeof(int));  // 复制分片数量
    memcpy(buffer + sizeof(int), missing_blocks, missing_count * sizeof(int));

    sendto(sockfd, buffer, sizeof(int) + missing_count * sizeof(int), MSG_CONFIRM, 
           (const struct sockaddr *)des_addr, sizeof(*des_addr));

    printf("Sent control info: %d fragments\n", missing_count);
}

//TODO:更改报文结构，维护第一位为报文区分
void* receive_data(void *arg){
    //接受数据包的receive代码
    Sate *satellite = (Sate *) arg;
    char buffer[1024];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);

    while (1) {
        int n = recvfrom(satellite->sockfd, buffer, BUFFER_SIZE, MSG_WAITALL,  (struct sockaddr *)&source_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        /*TODO：修改格式  [2bit 报文类型] + [4字节 分片编号] + [实际分片数据]，
        分离两个函数（或在我这继续写）一个是接受数据包信息，一个是接受其他卫星发来的请求控制信息*/
        
        //case 1 接收到的是数据包信息
        //发送的数据格式：[4字节 分片编号] + [实际分片数据]
        
        int fragment_id;
        memcpy(&fragment_id, buffer, sizeof(int));  // 解析分片编号
        //char *data = buffer + sizeof(int);  // 解析分片数据

        printf("Received fragment %d from base station\n", fragment_id);
        
        int a = satellite->missing_blocks[satellite->missing_count-1];

        for(int i = 0; i < satellite->missing_count; i++)
        {
            if(satellite->missing_blocks[i] == fragment_id)
            {
                satellite->missing_blocks[i] = a;
                continue;
            }
        }
        satellite->missing_count--;
        //数据包信息改变后立刻与所有连接的基站或其他卫星进行update
    }
    return NULL;
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

void signal_handler(int signum) {
    printf("\nTerminating satellite...\n");
    exit_flag = 1;
}

int main(){
    struct sockaddr_in satellite_addr;
    Sate *satellite = (Sate*) malloc(sizeof(Sate));  // ✅ 确保 satellite 被分配内存
    if (!satellite) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    satellite->type = SATELLITE_CIRCLE;

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
    send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
    
    signal(SIGINT, signal_handler);
    while (!exit_flag) {
        sleep(1);
    }
    printf("Satellite shutting down...\n");
    close(satellite->sockfd);
    free(satellite);
    return 0;
}