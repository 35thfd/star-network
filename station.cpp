#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0  // macOS 没有 MSG_CONFIRM，用 0 代替
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "station.h"  


void send_response(int sockfd, const char* message, size_t message_size, struct sockaddr_in client_addr) {
    sendto(sockfd, message, message_size, MSG_CONFIRM, (const struct sockaddr *)&client_addr, sizeof(client_addr));
    printf("Sent response to %s\n", inet_ntoa(client_addr.sin_addr));
}

void* Station::process_message(void *arg) {
    ProcessData *data = (ProcessData*)arg;
    char *control_message = data->control_message;
    int socket_fd = data->socket_fd;
    struct sockaddr_in satellite_addr = data->satellite_addr;
    Station *base_station = data->base_station;
    // for (int i = 0; i < sizeof(int) + 10 * sizeof(int); i++) {
    //     printf("%02x ", (unsigned char)control_message[i]);
    // }

    if (base_station->task_queue.size() < 2 * base_station->cnt) {
        base_station->init_queue();
    }

    int cnt,kind;
    memcpy(&kind, control_message, sizeof(int));
    memcpy(&cnt, control_message + sizeof(int), sizeof(int));
    printf("%d %d \n", kind, cnt);
    int *missing_fragments = (int *)malloc(cnt* sizeof(int));
    memcpy(missing_fragments, control_message + sizeof(int) * 2, cnt* sizeof(int));
    if(kind == 1)
    {
        int fragment_id = base_station->check(missing_fragments, cnt);
        printf("choose fragment %d to send\n", fragment_id);
        if (fragment_id != -1) {
            size_t data_size = DATA_SIZE;  // 修正：packet 是 char 数组，直接使用 DATA_SIZE
            char buffer[sizeof(int) * 2 + data_size];
    
            int control_flag = 0;  // 控制报文
            memcpy(buffer, &control_flag, sizeof(int));
            memcpy(buffer + sizeof(int), &fragment_id, sizeof(int));
            memcpy(buffer + sizeof(int) * 2, base_station->packet[fragment_id], DATA_SIZE);  // 修正：确保复制正确的大小
    
            // 输出数据包内容，确保数据正确
            printf("Sending packet (size = %lu): ", sizeof(int) * 2 + data_size);
            // for (size_t i = 0; i < sizeof(int) * 2 + data_size; i++) {
            //     printf("%02x ", (unsigned char)buffer[i]);
            // }
            // printf("\n");
    
            send_response(socket_fd, buffer, sizeof(int) * 2 + data_size, satellite_addr);  // 修正：使用二进制安全的发送
        }
    }
    

    free(data->control_message);
    free(data);
    return NULL;
}