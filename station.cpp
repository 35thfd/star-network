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


void send_response(int sockfd, const char* message, struct sockaddr_in client_addr) {
    sendto(sockfd, message, strlen(message), MSG_CONFIRM, (const struct sockaddr *)&client_addr, sizeof(client_addr));
    printf("Sent response to %s\n", inet_ntoa(client_addr.sin_addr));
}

void* Station::process_message(void *arg) {
    ProcessData *data = (ProcessData*)arg;
    char *control_message = data->control_message;
    int socket_fd = data->socket_fd;
    struct sockaddr_in satellite_addr = data->satellite_addr;
    Station *base_station = data->base_station;

    if (base_station->task_queue.size() < base_station->cnt) {
        base_station->init_queue();
    }

    int cnt;
    memcpy(&cnt, control_message, sizeof(int));
    int *missing_fragments = (int *)malloc(cnt* sizeof(int));  // 分配空间
    memcpy(missing_fragments, control_message + sizeof(int), cnt* sizeof(int));  // 复制数据


    int fragment_id = base_station->check(missing_fragments, cnt);
    if (fragment_id != -1) {
        send_response(socket_fd, base_station->packet[fragment_id], satellite_addr);
    }

    free(data->control_message);
    free(data);
    return NULL;
}