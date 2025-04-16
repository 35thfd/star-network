#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define BASE_STATION_PORT 8080
#define DATA_SIZE 1024
#define CNT 10  // 片段数量
int finished_satellites = 0;
pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;

void* listen_finish_signal(void* arg) {
    int sockfd;
    struct sockaddr_in addr, sender;
    socklen_t len = sizeof(sender);
    int buffer;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BASE_STATION_PORT);  // 监听完成信号
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));

    while (1) {
        recvfrom(sockfd, &buffer, sizeof(int), 0, (struct sockaddr*)&sender, &len);
        if (buffer == 99) {
            pthread_mutex_lock(&finish_mutex);
            finished_satellites++;
            printf("📥 Received finish signal (%d/3) from %s\n", 
                   finished_satellites, inet_ntoa(sender.sin_addr));
            if (finished_satellites >= 3) {
                printf("🎉 All satellites finished. Shutting down base station...\n");
                exit(0); // or return / break as needed
            }
            pthread_mutex_unlock(&finish_mutex);
        }
    }

    return NULL;
}


int main() {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    int total_sent_count = 0;

    pthread_t listener;
    pthread_create(&listener, NULL, listen_finish_signal, NULL);
    pthread_detach(listener);

    char packet[CNT][DATA_SIZE];

    // 模拟初始化数据
    for (int i = 0; i < CNT; i++) {
        memset(packet[i], 'A' + i, DATA_SIZE);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Error setting broadcast option");
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BASE_STATION_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("192.168.1.255");  // 广播地址（视网络环境修改）

    while (1) {
        for (int i = 1; i <= CNT; i++) {
            char buffer[4 + 4 + DATA_SIZE];
            int kind = 0;  // 数据包
            int fragment_id = i;
            memcpy(buffer, &kind, sizeof(int));
            memcpy(buffer + sizeof(int), &fragment_id, sizeof(int));
            memcpy(buffer + sizeof(int) * 2, packet[i], DATA_SIZE);

            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
            total_sent_count++;//频次统计
            printf("Broadcasted fragment %d\n 🐸🐸🐸(total_sent=%d)\n", fragment_id, total_sent_count);
            //printf("Broadcasted fragment %d\n", fragment_id);
            usleep(10000);  // 每片间隔短一点，避免太密集
        }
        sleep(2); // 每轮间隔
    }

    close(sockfd);
    return 0;
}
