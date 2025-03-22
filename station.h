#ifndef STATION_H
#define STATION_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CNT 100   // 默认队列长度
#define PORT 8080 // 服务器监听端口

// 报文格式
typedef struct {
    uint16_t satellite_id;  
    uint16_t dst_id;
    uint16_t len;
    uint16_t checksum; 
    uint16_t missing_count;
    char* block_data; 
} Packet;

// 基站类
class Station {
public:
    char packet[100][10000];
    std::queue<int> task_queue; // 改名，避免与 std::queue 冲突
    int cnt;               
    pthread_t monitor_thread;
    int sockfd;

    Station() {
        cnt = CNT;
        init_queue();
    }
    
    int setup_and_listen(int *listen_sockfd, struct sockaddr_in *server_addr) {
        // 创建UDP Socket
        if ((*listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket creation failed");
            return -1;
        }
    
        memset(server_addr, 0, sizeof(*server_addr));
        
        // 设置服务器地址
        server_addr->sin_family = AF_INET;
        server_addr->sin_addr.s_addr = INADDR_ANY; 
        server_addr->sin_port = htons(PORT);
    
        // 绑定Socket到指定端口
        if (bind(*listen_sockfd, (const struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
            perror("bind failed");
            close(*listen_sockfd);
            return -1;
        }
    
        printf("Base Station: Listening on port %d...\n", PORT);
        return 0;
    }

    // 初始化队列
    void init_queue() {
        for (int i = 1; i <= cnt; i++) {
            task_queue.push(i);
        }
        printf("Queue initialized with %d blocks.\n", cnt);
    }

    // 动态调整队列
    void adjust_queue(int* a, int a_size) {
        std::queue<int> temp_queue;
        int count_dict[CNT + 1] = {0};

        while (!task_queue.empty()) {
            int block_id = task_queue.front();
            task_queue.pop();
            count_dict[block_id]++;
            if (count_dict[block_id] <= 3) {
                temp_queue.push(block_id);
            }
        }

        task_queue = temp_queue;
    }

    int check(int* s, int s_size) {
        std::queue<int> temp_queue;
        int send_data = -1;

        while (!task_queue.empty()) {
            int block_id = task_queue.front();
            task_queue.pop();
            bool found = false;
            for (int i = 0; i < s_size; i++) {
                if (block_id == s[i]) {
                    found = true;
                    break;
                }
            }
            if (found) {
                send_data = block_id;
                break;
            }
            temp_queue.push(block_id);
        }

        // 重新放回队列
        while (!temp_queue.empty()) {
            task_queue.push(temp_queue.front());
            temp_queue.pop();
        }

        return send_data;
    }
    
    static void* process_message(void* arg);
};

typedef struct {
    char *control_message;  
    int socket_fd;
    struct sockaddr_in satellite_addr;
    Station *base_station;
} ProcessData;

#endif // STATION_H