#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SATELLITE_PORT 8080
#define DATA_SIZE 1024
#define CNT 10

char data[CNT + 1][DATA_SIZE];
int received[CNT + 1] = {0};

void* receive_thread(void* arg) {
    int sockfd = *(int*)arg;
    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);
    char buffer[4 + 4 + DATA_SIZE];

    while (1) {
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &len);
        if (n < 0) continue;

        int kind, fragment_id;
        memcpy(&kind, buffer, sizeof(int));
        memcpy(&fragment_id, buffer + sizeof(int), sizeof(int));

        if (kind == 0 && fragment_id >= 1 && fragment_id <= CNT) {
            if (!received[fragment_id]) {
                memcpy(data[fragment_id], buffer + 8, DATA_SIZE);
                received[fragment_id] = 1;
                printf("Received fragment %d from %s\n", fragment_id, inet_ntoa(sender.sin_addr));
            }
        }
    }

    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SATELLITE_PORT);

    if (bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, receive_thread, &sockfd);
    pthread_detach(tid);

    while (1) {
        sleep(5);
        int complete = 1;
        for (int i = 1; i <= CNT; i++) {
            if (!received[i]) complete = 0;
        }
        if (complete) {
            printf("All fragments received!\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}
