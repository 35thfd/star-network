#ifndef SATELLITE_H
#define SATELLITE_H

#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <csignal>
#include <map>  
#include <pthread.h>
#include <netinet/in.h> // For INET_ADDRSTRLEN
#include <arpa/inet.h>  // For inet_pton, inet_ntop
#include <string>       // Added for std::string
#include <algorithm>    // Added for std::sort
#include <time.h>       // Added for time_t


#define MAX_NEIGHBORS 10
#define DATA_SIZE 1024

#define BROADCAST_INTERVAL_SECONDS 15 // Discovery broadcast interval
#define NEIGHBOR_TIMEOUT_SECONDS 5    // Established neighbor timeout
#define STABILITY_THRESHOLD_SECONDS 15 // Time to become stable
#define POTENTIAL_NEIGHBOR_TIMEOUT_SECONDS 60

// 定义卫星类型
typedef enum {
    SATELLITE_DIAMOND,  // 菱形卫星：只接收不发送
    SATELLITE_CIRCLE,   // 圆形卫星：传输能力最强，可作为空中基站
    SATELLITE_TRIANGLE  // 三角卫星：可接收可发送，但传输速度慢
} SatelliteType;

/// --- Neighbor Structures ---
typedef struct {
    //std::string ip; // IP is the key in the map
    int port;
    time_t first_seen;
    time_t last_seen;
} PotentialNeighbor;

typedef struct {
    char neighborip[INET_ADDRSTRLEN]; // Use C-string for consistency
    int neighbor_ports;
    time_t established_time;
    time_t last_seen;
    bool is_stable;
} EstablishedNeighbor;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    time_t established_time; // 第一次收到 Type 3 的时间
    time_t last_seen;        // 最后一次收到任何消息的时间
    bool is_stable;          // 是否为稳定邻居
    // std::vector<int> missing_blocks; // 可以选择在此结构体中直接存储其缺失列表
    // time_t missing_blocks_last_update;
} NeighborInfo;

// Structure for sorting blocks by frequency
typedef struct {
    int block_id;
    int frequency;
} BlockFrequency;

// Structure to hold neighbor's missing info
typedef struct {
    std::vector<int> missing_blocks;
    time_t last_update_time;
} NeighborMissingInfoData;

typedef struct {
    SatelliteType type;  
    //int id;
    char neighborip[INET_ADDRSTRLEN];  // 存储邻居 IP
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
        int neighbor_count;

        //---修改/新增的邻居管理部分---
        std::vector<NeighborInfo> neighbors; // List of established neighbors
        pthread_mutex_t neighbor_lock; // Existing lock for established neighbors

        //Satellite neighbors[MAX_NEIGHBORS];

        //--- 频次管理部分 ---
        int frequency[1024];                       // 存储计算出的频次
        std::vector<int> prioritized_missing_list; // Cache for sorted missing list
        bool frequency_needs_update;               // Flag for recalculation
        pthread_mutex_t frequency_lock;

        std::map<std::string, NeighborMissingInfoData> neighbor_missing_data; // Key: "IP:Port"
        pthread_mutex_t missing_data_lock; // Existing lock

        pthread_t monitor_thread; // Existing thread - rename or repurpose if needed
        pthread_t neighbor_management_thread_id; // Dedicated thread for neighbor checks
        pthread_t periodic_task_thread_id; // Existing thread - rename or repurpose if needed

        
    //void save_fragment(fragment_id, data);
    void initialize() {
        memset(missing_blocks, 0, sizeof(missing_blocks));
        for(int i = 0 ; i <= 10; i++)
        {
            missing_blocks[i] = i;
        }
        //此处先定义为10,后续改为-1，发送时如果发现是-1代表还没有接收到数据包数量信息，需要等待
        missing_count = 10;
        neighbors.clear();

        // for (int i = 0; i < 1024; i++) {
        //     frequency[i] = 1; 
        // }
        neighbors.clear();
        neighbor_missing_data.clear();
        prioritized_missing_list.clear();

        frequency_needs_update = true; // 初始状态需要计算一次频次
        memset(frequency, 0, sizeof(frequency)); // 初始化频次数组
        
        //strncpy(neighbors[0].neighborip, "127.0.0.1", INET_ADDRSTRLEN);  
        //neighbors[0].neighbor_ports = 10010;  // 假设第一个邻居监听 5001 端口
        pthread_mutex_init(&neighbor_lock, NULL);
        pthread_mutex_init(&frequency_lock, NULL);
        pthread_mutex_init(&missing_data_lock, NULL);

        memset(data, 0, sizeof(data));
    }

    ~Sate() {
        pthread_mutex_destroy(&neighbor_lock);
        pthread_mutex_destroy(&frequency_lock);
        pthread_mutex_destroy(&missing_data_lock);
    }

    // 查找已确认邻居的索引
    std::string make_neighbor_key(const char* ip, int port);
    void calculate_frequency_and_prioritize();
    void send_prioritized_missing_list(int sock, const struct sockaddr_in* dest_addr);
    static bool compare_block_frequency(const BlockFrequency& a, const BlockFrequency& b);

    void add_or_update_neighbor(const char* ip, int port, time_t current_time) {
        pthread_mutex_lock(&neighbor_lock);
        int index = find_neighbor_index(ip, port);
        if (index != -1) {
            neighbors[index].last_seen = current_time;
        }
        else {
            NeighborInfo new_neighbor;
            strncpy(new_neighbor.ip, ip, INET_ADDRSTRLEN - 1);
            new_neighbor.ip[INET_ADDRSTRLEN - 1] = '\0';
            new_neighbor.neighbor_port = port;
            new_neighbor.last_seen = current_time;
            neighbors.push_back(new_neighbor);
            // 注意：这里无法轻易获得新邻居的 SatelliteID (如果还需要的话)
            // 如果 map 的 key 只需要 IP:Port，则没问题
            pthread_mutex_lock(&frequency_lock);
            frequency_needs_update = true; // 新邻居加入
            pthread_mutex_unlock(&frequency_lock);
            printf("  添加新邻居 %s:%d。当前邻居数: %lu\n", ip, port, neighbors.size());
        }
        pthread_mutex_unlock(&neighbor_lock);
    }
    

    void update_neighbor_missing_info(const std::string& neighbor_key, const int* blocks, int count, bool trigger_freq_recalc) {
        pthread_mutex_lock(&this->missing_data_lock); // ---- BEGIN CRITICAL SECTION for neighbor_missing_data ----

        // 获取对 map 中元素的引用；如果键不存在，会自动创建一个新的 NeighborMissingInfoData 对象
        NeighborMissingInfoData& info = this->neighbor_missing_data[neighbor_key];

        // 更新缺失块列表
        info.missing_blocks.clear(); // 先清空旧的列表
        if (count > 0 && blocks != nullptr) {
            info.missing_blocks.assign(blocks, blocks + count); // 使用新数据填充
        }
        // 如果 count 为 0，则 missing_blocks 将保持为空，表示该邻居没有报告缺失

        info.last_update_time = time(NULL); // 记录本次更新的时间

        pthread_mutex_unlock(&this->missing_data_lock); // ---- END CRITICAL SECTION for neighbor_missing_data ----

        // 调试打印 (可选)
        // printf("  已更新邻居 %s 的缺失信息，包含 %d 个块。上次更新时间: %ld\n",
        //        neighbor_key.c_str(), count, info.last_update_time);


        // 2. 如果需要，设置频次重算标志 (受 frequency_lock 保护)
        if (trigger_freq_recalc) {
            pthread_mutex_lock(&this->frequency_lock); // ---- BEGIN CRITICAL SECTION for frequency_needs_update ----
            this->frequency_needs_update = true;
            pthread_mutex_unlock(&this->frequency_lock); // ---- END CRITICAL SECTION for frequency_needs_update ----

            // printf("    由于邻居 %s (被认为是稳定的或重要更新) 的信息更新，已标记频次需要重算 (frequency_needs_update = true)。\n",
            //        neighbor_key.c_str());
        }
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

    int find_neighbor_index(const char* ip, int port);
    
};

#endif 