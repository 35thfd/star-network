#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "station.h"
#include "satellite.h"

#define MAX_BUFFER_SIZE 1024
#define BASE_STATION_PORT 12345  // 监听端口 
#define MAX_SATELLITES 100
#define MAX_PACKET_SIZE 256

char recv_packet[MAX_PACKET_SIZE];

int main() {
    int socket_fd;
    struct sockaddr_in base_station_addr;
    Station base_station;  

    if (base_station.setup_and_listen(&socket_fd, &base_station_addr) < 0) {
        fprintf(stderr, "Failed to setup and listen\n");
        return EXIT_FAILURE;
    }
    base_station.sockfd = socket_fd;

    char buffer[1024];
    struct sockaddr_in satellite_addr;
    socklen_t addr_len = sizeof(satellite_addr);

    while (1) {
        int n = recvfrom(socket_fd, (char *)buffer, 1024, MSG_WAITALL, 
                         (struct sockaddr *)&satellite_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        //buffer[n] = '\0';
        printf("Base Station: Received Control Message from Satellite: %s\n", buffer);
        
        //创建传输进程
        pthread_t process_thread;
        ProcessData *data = (ProcessData *)malloc(sizeof(ProcessData));
        data->control_message = (char*)malloc(n);  // 为接收的数据分配内存
        if (data->control_message == NULL) {
            perror("Memory allocation failed");
            continue;  // 如果内存分配失败，跳过此次处理
        }

        memcpy(data->control_message, buffer, n);  
        data->socket_fd = socket_fd;
        data->satellite_addr = satellite_addr;
        data->base_station = &base_station;

        if (pthread_create(&process_thread, NULL, Station::process_message, (void*)data) != 0) {
            perror("Failed to create process thread");
            free(data->control_message);
            free(data);
            continue;
        }

        pthread_detach(process_thread);
    }

    close(socket_fd);
    return 0;
}
