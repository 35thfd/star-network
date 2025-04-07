#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CNT 100  // 最大数据块数量
#define MAX_SATELLITES 10  // 最大卫星数量


typedef struct {
    uint16_t satellite_id;  
    uint16_t dst_id;
    uint16_t len;
    uint16_t checksum; 
    uint16_t missing_count;
    char* block_data; 
} Packet;


typedef struct {
    int block_id;
    int frequency;
}BlockFrequency;


void update_missing_frequency(Satellite* satellite, Satellite* neighbors, int neighbor_count) {
    for (int i = 0; i < satellite->missing_count; i++) {
        int block_id = satellite->missing_blocks[i];
        satellite->frequency[block_id]++;
    }

    // 统计邻居卫星的缺失分片
    for (int i = 0; i < neighbor_count; i++) {
        for (int j = 0; j < neighbors[i].missing_count; j++) {
            int block_id = neighbors[i].missing_blocks[j];
            satellite->frequency[block_id]++;
        }
    }
}

int compare(const void* a, const void* b) {
    BlockFrequency* block_a = (BlockFrequency*)a;
    BlockFrequency* block_b = (BlockFrequency*)b;
    return block_b->frequency - block_a->frequency;  
}


int* sort_blocks_by_frequency(int frequency[]) {
    BlockFrequency blocks[CNT];
    int* sorted_blocks = (int*)malloc(CNT * sizeof(int));  
    for (int i = 0; i < CNT; i++) {
        blocks[i].block_id = i;
        blocks[i].frequency = frequency[i];
    }


    qsort(blocks, CNT, sizeof(BlockFrequency), compare);

    for (int i = 0; i < CNT; i++) {
        sorted_blocks[i] = blocks[i].block_id;
    }

    return sorted_blocks;  
}

void send_control_message(Satellite* satellite, Satellite* neighbors, int neighbor_count) {
    for (int i = 0; i < CNT; i++) {
        if (satellite->frequency[i] > 0) {
            messages[message_count].block_id = i;
            messages[message_count].frequency = satellite->frequency[i];
            message_count++;
        }
    }

    int* temp = sort_blocks_by_frequency(satellite->frequency);
    char data = (char*) temp;
    // 发送控制流信息给邻居卫星
    for (int i = 0; i < neighbor_count; i++) {
        
        socket(neighbors[i]->satellite_id,id,checksum,data);
    }
}

void transmit_data(Satellite* satellite, Satellite* neighbors, int neighbor_count) {
    Packet* packet;
    Satellite* sat;
    packet = sat->receive();
    for (int i = 0; i < neighbor_count; i++) {
        printf("Satellite %d receiving control messages from Satellite %d\n", satellite->id, neighbors[i].id);
        // 这里可以替换为实际的接收逻辑
    }

    // 选择缺失频次最高的数据分片
    int max_frequency = 0;
    int selected_block = -1;

    for (int i = 0; i < message_count; i++) {
        if (messages[i].frequency > max_frequency) {
            max_frequency = messages[i].frequency;
            selected_block = messages[i].block_id;
        }
    }

    // 发送缺失频次最高的数据分片
    if (selected_block != -1) {
        printf("Satellite %d transmitting block %d\n", satellite->id, selected_block);
        
    }
}

void update_missing_blocks(Satellite* satellite, int block_id) {
    // 从缺失分片列表中移除已接收的数据分片
    for (int i = 0; i < satellite->missing_count; i++) {
        if (satellite->missing_blocks[i] == block_id) {
            for (int j = i; j < satellite->missing_count - 1; j++) {
                satellite->missing_blocks[j] = satellite->missing_blocks[j + 1];
            }
            satellite->missing_count--;
            break;
        }
    }
}