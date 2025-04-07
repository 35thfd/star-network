#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NEIGHBORS 10
#define DATA_SIZE 1024

// 定义卫星类型
typedef enum {
    SATELLITE_DIAMOND,  // 菱形卫星：只接收不发送
    SATELLITE_CIRCLE,   // 圆形卫星：传输能力最强，可作为空中基站
    SATELLITE_TRIANGLE  // 三角卫星：可接收可发送，但传输速度慢
} SatelliteType;

// 定义卫星结构体


typedef struct {
    SatelliteType type;  
    int id;
    char neighborip[16];  // 存储邻居 IP
    int neighbor_ports;  // 存储邻居端口
    int missing_blocks[1024]; 
    int missing_count; 
    int frequency[1024]; 
} Satellite;

class Sate {
    public:
        SatelliteType type;  
        int id;              
        int sockfd;
        int data[1024][100000]; 
        int missing_blocks[1024];  
        int missing_count;

        Satellite neighbors[MAX_NEIGHBORS];

        int neighbor_count;
        int frequency[1024]; 

        pthread_t monitor_thread;

    //void save_fragment(fragment_id, data);
    void initialize() {

        memset(missing_blocks, 0, sizeof(missing_blocks));
        memset(neighbors, 0 , sizeof(neighbors));
        for(int i = 0 ; i <= 10; i++)
        {
            missing_blocks[i] = i;
        }
        //此处先定义为10,后续改为-1，发送时如果发现是-1代表还没有接收到数据包数量信息，需要等待
        missing_count = 10;
        
        neighbor_count = 1;
        for (int i = 0; i < 1024; i++) {
            frequency[i] = 1; 
        }
        
        strncpy(neighbors[0].neighborip, "127.0.0.1", INET_ADDRSTRLEN);  
        neighbors[0].neighbor_ports = 9999;  // 假设第一个邻居监听 5001 端口

        memset(data, 0, sizeof(data));
    }

    int check(int* s, int s_size) {
        int best_fragment = -1;
        // 遍历接收到的缺失数据频次表 s[]
        for (int i = 0; i < s_size; i++) {
            int fragment_id = s[i];
    
            // 如果当前卫星拥有该数据分片，并且它在对方缺失频次最高
            if (data[fragment_id][0] != 0) {
                best_fragment = fragment_id;
            }
        }
        if (best_fragment != -1) {
            return best_fragment;
        }
    
        return -1;  // 如果没有合适的分片
    }
    
};

