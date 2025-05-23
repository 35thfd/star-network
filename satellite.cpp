// #ifndef MSG_CONFIRM
// #define MSG_CONFIRM 0  // macOS 没有 MSG_CONFIRM，用 0 代替
// #endif

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <csignal>
#include <string>
#include <sstream> 
#include "satellite.h"

using namespace std;

#define MAX_FRAGMENTS 100
#define SLEEP_TIME 10
#define FRAGMENT_SIZE 512
#define BUFFER_SIZE (sizeof(int) + FRAGMENT_SIZE)
#define BASE_STATION_IP "192.168.1.5" 
//#define Neighbour_IP "127.0.0.1" 
#define BASE_STATION_PORT 8080
#define SATELLITE_PORT 9999 
#define Nei_PORT 10010

volatile int exit_flag = 0;

int received_count = 0;

void get_broadcast_address(char *interface_name, char *broadcast_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0'; 
    // 获取广播地址
    if (ioctl(sock, SIOCGIFBRDADDR, &ifr) < 0) {
        perror("ioctl failed to get broadcast address");
        close(sock);
        return;
    }

    struct sockaddr_in *bcast = (struct sockaddr_in *)&ifr.ifr_broadaddr;
    strcpy(broadcast_ip, inet_ntoa(bcast->sin_addr));

    close(sock);
}

int Sate::find_neighbor_index(const char* ip, int port) {
    // 注意：调用此函数的代码块通常应该已经获取了 neighbor_lock 互斥锁，
    // 以确保在遍历 neighbors 向量时线程安全。此函数本身不处理锁。

    for (size_t i = 0; i < neighbors.size(); ++i) {
        // neighbors[i].neighbor_ip 是一个 char 数组 (INET_ADDRSTRLEN)
        // ip 参数也是一个 char*
        // 比较 IP 地址和端口号
        if (neighbors[i].port == port && strcmp(neighbors[i].ip, ip) == 0) {
            return static_cast<int>(i); // 找到匹配项，返回索引
        }
    }
    return -1; // 未找到匹配项
}

std::string make_neighbor_key(const char* ip, int port) {
    return std::string(ip) + ":" + std::to_string(port);
}

bool Sate::compare_block_frequency(const BlockFrequency& a, const BlockFrequency& b) {
    // 降序排序 (频次高的在前)
    if (a.frequency != b.frequency) {
        return a.frequency > b.frequency;
    }

    return a.block_id < b.block_id;
}

void Sate::calculate_frequency_and_prioritize() {
    // 假设调用者已经获取了 this->frequency_lock

    printf("PID %lu: 开始计算频次和优先级...\n", (unsigned long)pthread_self());

    // --- 1. 初始化本卫星的全局频次统计数组 ---
    // 假设块ID的范围是0到1023。如果不是，请调整数组大小和索引逻辑。
    memset(this->frequency, 0, sizeof(this->frequency));

    // --- 2. 统计自身缺失的块，计入全局频次 ---
    // 注意：这里的 this->frequency 数组是用来计算 *全局* 缺失频次的，
    // 而不是仅本卫星的。本卫星的缺失块是最终排序的对象。
    for (int i = 0; i < this->missing_count; ++i) {
        int block_id = this->missing_blocks[i];
        if (block_id >= 0 && block_id < 1024) { // 基本的边界检查
            this->frequency[block_id]++;
        } else {
            // fprintf(stderr, "警告: 自身缺失列表中发现无效块ID %d\n", block_id);
        }
    }
    // printf("  自身缺失 %d 个块已计入频次统计。\n", this->missing_count);


    // --- 3. 统计所有“稳定邻居”缺失的块，计入全局频次 ---
    pthread_mutex_lock(&this->neighbor_lock);       // 访问 neighbors 列表前加锁
    pthread_mutex_lock(&this->missing_data_lock); // 访问 neighbor_missing_data 前加锁

    int stable_neighbors_counted = 0;
    for (const auto& neighbor : this->neighbors) { // 遍历统一的邻居列表
        if (neighbor.is_stable) { // 只考虑稳定邻居
            stable_neighbors_counted++;
            std::string neighbor_key = make_neighbor_key(neighbor.ip, neighbor.port);
            auto it = this->neighbor_missing_data.find(neighbor_key);

            if (it != this->neighbor_missing_data.end()) {
                const NeighborMissingInfoData& missing_info = it->second;
                // printf("  稳定邻居 %s 数据: %zu 个缺失块. 上次更新: %ld\n",
                //        neighbor_key.c_str(), missing_info.missing_blocks.size(), missing_info.last_update_time);
                for (int block_id : missing_info.missing_blocks) {
                    if (block_id >= 0 && block_id < 1024) { // 边界检查
                        this->frequency[block_id]++;
                    } else {
                        // fprintf(stderr, "警告: 稳定邻居 %s 的缺失列表中发现无效块ID %d\n", neighbor_key.c_str(), block_id);
                    }
                }
            } else {
                // printf("  警告: 稳定邻居 %s 在 neighbor_missing_data 中未找到记录。\n", neighbor_key.c_str());
            }
        }
    }

    pthread_mutex_unlock(&this->missing_data_lock);
    pthread_mutex_unlock(&this->neighbor_lock);
    // printf("  共计 %d 个稳定邻居的缺失信息已计入频次统计。\n", stable_neighbors_counted);


    // --- 4. 根据计算出的全局频次，对本卫星“自身缺失的块”进行排序 ---
    //    目标是得到 this->prioritized_missing_list
    std::vector<BlockFrequency> self_missing_blocks_with_their_global_freq;
    for (int i = 0; i < this->missing_count; ++i) {
        int block_id = this->missing_blocks[i];
        if (block_id >= 0 && block_id < 1024) { // 再次进行边界检查
            // 对于本卫星缺失的每个块，查找它在全局频次统计中的值
            self_missing_blocks_with_their_global_freq.push_back({block_id, this->frequency[block_id]});
        }
    }

    // 使用 std::sort 和自定义比较函数进行排序
    std::sort(self_missing_blocks_with_their_global_freq.begin(),
              self_missing_blocks_with_their_global_freq.end(),
              Sate::compare_block_frequency);

    // --- 5. 更新本卫星的 prioritized_missing_list ---
    this->prioritized_missing_list.clear();
    for (const auto& bf : self_missing_blocks_with_their_global_freq) {
        this->prioritized_missing_list.push_back(bf.block_id);
    }

    // 打印调试信息 (可选)
    // printf("  频次计算和优先级排序完成。本卫星的优先缺失列表 (%zu 个条目): ", this->prioritized_missing_list.size());
    // for(int id : this->prioritized_missing_list) {
    //     printf("%d(全局freq:%d) ", id, (id >=0 && id < 1024 ? this->frequency[id] : -1));
    // }
    // printf("\n");

    // --- 6. 更新标志位 ---
    // 这一步应该在调用此函数的临界区内完成，或者此函数是唯一修改它的地方（在持有frequency_lock时）
    this->frequency_needs_update = false;
    printf("PID %lu: 频次计算完成，frequency_needs_update 设置为 false。\n", (unsigned long)pthread_self());
}


void Sate::send_prioritized_missing_list(int sock, const struct sockaddr_in* dest_addr) {
    // 0. 确保 prioritized_missing_list 是最新的
    //    调用者（例如 case 3 处理逻辑）应该在调用此函数前，
    //    根据 frequency_needs_update 标志决定是否调用 calculate_frequency_and_prioritize()。
    //    这里我们假设调用时列表已经是合理的。

    pthread_mutex_lock(&this->frequency_lock); // 保护对 prioritized_missing_list 的访问

    if (this->prioritized_missing_list.empty()) {
        pthread_mutex_unlock(&this->frequency_lock);
        // printf("优先缺失列表为空，不发送 Type 1 消息给 %s:%d\n",
        //        inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
        return; // 如果列表为空，则不发送
    }

    // 1. 准备消息内容
    int message_type = 1; // Type 1 消息
    int count = static_cast<int>(this->prioritized_missing_list.size());

    // 计算缓冲区大小: 消息类型(int) + 数量(int) + 块ID列表(count * int)
    size_t buffer_size = sizeof(int) + sizeof(int) + (count * sizeof(int));
    char* buffer = new (std::nothrow) char[buffer_size]; // 使用 new(std::nothrow) 避免异常

    if (!buffer) {
        pthread_mutex_unlock(&this->frequency_lock);
        perror("send_prioritized_missing_list: 内存分配失败");
        return;
    }

    // 2. 填充缓冲区
    char* ptr = buffer;
    memcpy(ptr, &message_type, sizeof(int));
    ptr += sizeof(int);
    memcpy(ptr, &count, sizeof(int));
    ptr += sizeof(int);
    memcpy(ptr, this->prioritized_missing_list.data(), count * sizeof(int));

    // 暂存列表内容后即可解锁，不必持有锁到 sendto 完成
    pthread_mutex_unlock(&this->frequency_lock);

    // 3. 发送消息
    ssize_t bytes_sent = sendto(sock, buffer, buffer_size, 0,
                                (const struct sockaddr*)dest_addr, sizeof(struct sockaddr_in));

    if (bytes_sent < 0) {
        perror("send_prioritized_missing_list: sendto 失败");
    } else if (static_cast<size_t>(bytes_sent) != buffer_size) {
        fprintf(stderr, "send_prioritized_missing_list: 警告 - 发送的字节数 (%zd) 与预期 (%zu) 不符给 %s:%d\n",
                bytes_sent, buffer_size, inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
    } else {
        printf("已成功发送 Type 1 优先缺失列表 (%d 个条目) 给 %s:%d\n",
               count, inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
    }

    delete[] buffer; // 释放动态分配的内存
}

//发送随机顺序的数据包,对比方案
void send_control_info(int sockfd, struct sockaddr_in *des_addr, int *missing_blocks, int missing_count){
    //分离出发送控制信息的函数，代表的含义是目前自己所拥有的
    int temp_i = 1;
    char buffer[sizeof(int) * 2 + MAX_FRAGMENTS * sizeof(int)];
    memcpy(buffer, &temp_i, sizeof(int));  
    memcpy(buffer + sizeof(int), &missing_count, sizeof(int)); 
    memcpy(buffer + sizeof(int) * 2, (missing_blocks+1), missing_count * sizeof(int));
    cout << "Control info sent: ";
    for (int i = 0; i < sizeof(int) * 2 + missing_count * sizeof(int); i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
    sendto(sockfd, buffer, sizeof(int) * 2 + missing_count * sizeof(int), MSG_CONFIRM, 
           (const struct sockaddr *)des_addr, sizeof(*des_addr));

    printf("Sent control info: %d fragments\n", missing_count);
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

void send_to_neighbor_satellite(int sockfd, int* missing_blocks, int missing_count, int neighborcount, Satellite neighbors[]) {
    struct sockaddr_in neighbor_satellite_addr;

    for (int i = 0; i < neighborcount; i++) {
        memset(&neighbor_satellite_addr, 0, sizeof(neighbor_satellite_addr));
        neighbor_satellite_addr.sin_family = AF_INET;
        neighbor_satellite_addr.sin_port = htons(neighbors[i].neighbor_ports);  // 目标端口

        // 将字符串 IP 地址转换为 sockaddr 结构
        if (inet_pton(AF_INET, neighbors[i].neighborip, &neighbor_satellite_addr.sin_addr) <= 0) {
            perror("Invalid neighbor IP address");
            continue;  // 跳过这个邻居，继续下一个
        }

        // 发送控制信息
        send_control_info(sockfd, &neighbor_satellite_addr, missing_blocks, missing_count);
        printf("Sent control info to neighbor: %s:%d\n", neighbors[i].neighborip, neighbors[i].neighbor_ports);
    }
}

//TODO:更改报文结构，维护第一位为报文区分
void* receive_data(void *arg){
    //接受数据包的receive代码
    Sate *satellite = (Sate *) arg;
    char control_buffer[10000];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    
    while (1) {
        int n = recvfrom(satellite->sockfd, control_buffer, BUFFER_SIZE, MSG_WAITALL,  (struct sockaddr *)&source_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        time_t current_time = time(NULL);

        int kind;
        memcpy(&kind, control_buffer, sizeof(int)); 
        printf("data kind : %d\n ", kind);


        //int fragment_id;
        //memcpy(&fragment_id, control_buffer + sizeof(int), sizeof(int));  // 解析分片编号
        //printf("%d\n", fragment_id);
        char *buffer = NULL;
        buffer = control_buffer + sizeof(int);
        switch (kind)
        {
        //case 0 接收到的是数据包信息
        //发送的数据格式：[4字节 分片编号] + [实际分片数据]
        case 0:
            {
                int fragment_id;
                memcpy(&fragment_id, control_buffer + sizeof(int), sizeof(int));  // 解析分片编号
                char *data = buffer + sizeof(int);  // 解析分片数据
                printf("Received fragment %d from %d\n", fragment_id, ntohs(source_addr.sin_port));
                
                int a;
                if(satellite->missing_count != 0)  
                    a = satellite->missing_blocks[satellite->missing_count];

                for(int i = 1; i <= satellite->missing_count; i++)
                {
                    if(satellite->missing_blocks[i] == fragment_id)
                    {
                        satellite->missing_blocks[i] = a;
                        satellite->missing_count--;
                        memcpy(satellite->data[fragment_id], data, 1024); 
                        //使用卫星见使用发送进程定期维护彼此数据保持有信息之后，在数据包信息发生变化后才发送请求信息
                        //数据包信息改变后立刻与所有连接的基站或其他卫星进行update
                        if(source_addr.sin_port == htons(BASE_STATION_PORT)&&source_addr.sin_addr.s_addr == inet_addr(BASE_STATION_IP))
                        {
                            //向基站发送请求
                            send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
                        }
                        else
                        {
                            //向邻居卫星发送请求
                            send_control_info(satellite->sockfd, &source_addr, satellite->missing_blocks, satellite->missing_count);
                        }
                        break;
                    }
                }
            }
        break;
        //case 1 接收到的是其他卫星的数据流信息
        //默认能发过来控制流信息的卫星都是自己的邻居，已经建立连接的邻居
        case 1:
        {
            //在这个地方只储存稳定邻居的内容
            int cnt;
            memcpy(&cnt, buffer, sizeof(int));  // 解析分片数量
            printf("Received control message from neighbor satellite, cnt: %d\n", cnt);

            int *missing_fragments = (int *)malloc(cnt * sizeof(int));
            memcpy(missing_fragments, buffer + sizeof(int), cnt * sizeof(int));

            // 2. 检查发送方是否为已"认识"的邻居，并据此处理其缺失信息和 last_seen
            int neighbor_idx = -1;
            bool sender_is_currently_stable = false;
            
            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(source_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
            int sender_port = ntohs(source_addr.sin_port);
            std::string current_neighbor_key = make_neighbor_key(sender_ip, sender_port); // For neighbor_missing_data

            pthread_mutex_lock(&satellite->neighbor_lock); // ---- BEGIN CRITICAL SECTION for neighbors list ----
            neighbor_idx = satellite->find_neighbor_index(sender_ip, sender_port);

            if (neighbor_idx != -1) {
                // ---- 发送方是已"认识"的邻居 ----
                satellite->neighbors[neighbor_idx].last_seen = current_time;
                sender_is_currently_stable = satellite->neighbors[neighbor_idx].is_stable;

                // 根据稳定性处理缺失信息更新和频次重算触发
                if (sender_is_currently_stable) {
                    printf("  稳定邻居 %s:%d 发来缺失列表。更新其缺失数据并标记频次重算。\n", sender_ip, sender_port);
                    satellite->update_neighbor_missing_info(current_neighbor_key, missing_fragments, cnt, true /* 触发频次重算 */);
                } else {
                    printf("  非稳定邻居 %s:%d 发来缺失列表。仅记录其缺失数据，不触发频次重算。\n", sender_ip, sender_port);
                    satellite->update_neighbor_missing_info(current_neighbor_key, missing_fragments, cnt, false /* 不触发频次重算 */);
                }
            } else {
                // ---- 发送方不是已"认识"的邻居 (之前未收到过它的 Type 3) ----
                // 我们仍然可能需要回复数据，但其缺失信息不会用来更新我们的邻居数据或触发频次计算。
                // 也可以选择将它的缺失信息存到一个临时的、非邻居的缓存中，如果需要的话。
                // 但根据当前规则，我们只关心对邻居列表成员的处理。
                printf("  收到来自非邻居 %s:%d 的 Type 1 消息。其缺失信息不会更新邻居状态或频次计算，但仍会尝试回复数据。\n", sender_ip, sender_port);
                // 注意：这里我们没有更新任何 neighbor_missing_data，因为对方不是邻居。
                // 如果有全局的 "所有收到过的缺失列表" 缓存，可以更新那里。
            }
            pthread_mutex_unlock(&satellite->neighbor_lock); // ---- END CRITICAL SECTION for neighbors list ----


            // 3. 回复数据分片给发送方 (无论发送方是否为邻居，只要对方有缺失且自己有对应数据)
            if (cnt > 0 && missing_fragments != nullptr) {
                int fragment_id_to_send = satellite->check(missing_fragments, cnt); // check() 选择要发送的片号

                if (fragment_id_to_send != -1) {
                    // 准备 Type 0 数据包: [int type=0][int fragment_id][char data[DATA_SIZE]]
                    size_t data_payload_size = DATA_SIZE;
                    size_t send_buffer_size = sizeof(int) * 2 + data_payload_size;
                    char* send_buffer = new (std::nothrow) char[send_buffer_size];

                    if (!send_buffer) {
                        perror("PID %lu: 为发送数据包分配内存失败 (Type 1 response)");
                    } else {
                        int type_0_response = 0;
                        memcpy(send_buffer, &type_0_response, sizeof(int));
                        memcpy(send_buffer + sizeof(int), &fragment_id_to_send, sizeof(int));
                        memcpy(send_buffer + sizeof(int) * 2, satellite->data[fragment_id_to_send], data_payload_size);

                        ssize_t bytes_sent = sendto(satellite->sockfd, send_buffer, send_buffer_size, 0,
                                                    (const struct sockaddr*)&source_addr, sizeof(struct sockaddr_in));
                        if (bytes_sent < 0) {
                            perror("PID %lu: 回复数据给对方 (Type 0) 失败");
                        } else if (static_cast<size_t>(bytes_sent) != send_buffer_size) {
                            fprintf(stderr, "PID %lu: 警告 - 回复数据时发送的字节数 (%zd) 与预期 (%zu) 不符给 %s:%d\n",
                                    (unsigned long)pthread_self(), bytes_sent, send_buffer_size, sender_ip, sender_port);
                        } else {
                            // printf("  已向 %s:%d 回复数据片断 %d\n", sender_ip, sender_port, fragment_id_to_send);
                        }
                        delete[] send_buffer;
                    }
                } else {
                    // printf("  对于 %s:%d 的缺失列表，没有合适的数据片断可回复。\n", sender_ip, sender_port);
                }
            }

            // 释放为 missing_fragments 分配的内存
            if (missing_fragments != nullptr) {
                free(missing_fragments);
            }

        }
        break;
        //case 2 接受到的是数据包数量信息
        case 2:
            {
                int blocknum;
                memcpy(&blocknum, buffer, sizeof(int));  // 解析分片编号
                satellite->missing_count = blocknum;
                for(int i = 1; i <= blocknum; i ++)
                {
                    satellite->missing_blocks[i] = i;//初始化
                }
            }
            break;

        // 卫星应该有自己维护星间路由的算法，这个应该已经由502所实现好了，所以这部分信息单独拿出一个控制序号仅用来模拟卫星间的连接，维护邻居信息
        // 也可以和数据请求信息结合
        // 报文只有控制序号，没有其他信息，需要记录recvfrom获得的addr
        case 3:
            {
                // 解析发送方地址
                char sender_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(source_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
                int sender_port = ntohs(source_addr.sin_port);

                printf("Received broadcast message from %s:%d\n", sender_ip, sender_port);

                int neighbor_idx = -1; // 用于存储邻居在列表中的索引

                pthread_mutex_lock(&satellite->neighbor_lock); // ---- BEGIN CRITICAL SECTION for neighbors list ----

                neighbor_idx = satellite->find_neighbor_index(sender_ip, sender_port); // 查找该发送方是否已经是邻居

                if (neighbor_idx == -1) {
                    // ---- 发送方是一个全新的邻居 (之前未收到过它的 Type 3) ----
                    NeighborInfo new_neighbor;
                    strncpy(new_neighbor.ip, sender_ip, INET_ADDRSTRLEN - 1);
                    new_neighbor.ip[INET_ADDRSTRLEN - 1] = '\0'; // 确保空字符结尾
                    new_neighbor.port = sender_port;
                    new_neighbor.established_time = current_time; // 记录“认识”此邻居的时间点
                    new_neighbor.last_seen = current_time;        // 更新最后看见时间
                    new_neighbor.is_stable = false;               // 新邻居初始状态为非稳定

                    satellite->neighbors.push_back(new_neighbor);
                    // neighbor_idx = this->neighbors.size() - 1; // 如果后续需要，可以这样获取新邻居的索引

                    printf("  新邻居 %s:%d 已通过 Type 3 消息加入列表。established_time: %ld, is_stable: false\n",
                        sender_ip, sender_port, new_neighbor.established_time);

                    // 注意：此时不改变 frequency_needs_update。
                    // 该邻居是否以及何时变为稳定，由后台线程判断。
                    // 当后台线程将其 is_stable 变为 true 时，才会触发频次重算。

                } else {
                    // ---- 发送方是已存在的邻居 ----
                    satellite->neighbors[neighbor_idx].last_seen = current_time; // 更新最后看见时间

                    // is_stable 和 established_time 状态由后台线程管理，此处不修改
                    // printf("  已存在邻居 %s:%d 的 last_seen 已通过 Type 3 消息更新为: %ld. 当前稳定状态: %s\n",
                    //        sender_ip, sender_port, this->neighbors[neighbor_idx].last_seen,
                    //        this->neighbors[neighbor_idx].is_stable ? "稳定" : "非稳定");
                }

                pthread_mutex_unlock(&satellite->neighbor_lock); // ---- END CRITICAL SECTION for neighbors list ----

                // 3. 触发交互: 立即向对方发送自己的 Type 1 优先级缺失列表
                // (无论对方是新是旧，只要收到 Type 3 就回复自己的缺失)
                if (satellite->missing_count > 0) {
                    bool needs_prioritized_list_recalculation = false;//这段看要不要
                    pthread_mutex_lock(&satellite->frequency_lock); // 安全读取 frequency_needs_update
                    if (satellite->frequency_needs_update) {
                        needs_prioritized_list_recalculation = true;
                    }
                    pthread_mutex_unlock(&satellite->frequency_lock);//这段看要不要

                    pthread_mutex_lock(&satellite->frequency_lock);
                    if (satellite->frequency_needs_update) { 
                        satellite->calculate_frequency_and_prioritize(); 
                    }
                    pthread_mutex_unlock(&satellite->frequency_lock);
                    satellite->send_prioritized_missing_list(satellite->sockfd, &source_addr);
                    printf("已向 %s:%d 发送 Type 1 优先缺失列表 (响应 Type 3)\n", sender_ip, sender_port);
                } else {
                    printf("自身没有缺失数据，不向 %s:%d 发送 Type 1 列表\n", sender_ip, sender_port);
                }
            }
            break;
        }
        
    }
    return NULL;
}

void* send_data(void *arg){
    Sate *satellite = (Sate *) arg;
    while(1){
        if(satellite->missing_count != -1){
            send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
            //能不能检测
            //发送连接请求
            //向其他已经连接的卫星发送请求
            //send_to_neighbor_satellite(satellite->sockfd, satellite->missing_blocks, satellite->missing_count, satellite->neighbor_count, satellite->neighbors);
        }
        sleep(SLEEP_TIME);
    }
    return NULL;
}

void* send_heartbeat(void *arg){
    printf("send_heartbeat\n");
    Sate *satellite = (Sate *) arg;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    int heartbeat_type = 3;  // 心跳报文类型

    // 配置广播地址
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(SATELLITE_PORT); // 目标端口

    // 使用广播地址 192.168.1.255
    broadcast_addr.sin_addr.s_addr = inet_addr("192.168.1.255"); // 广播地址

    // 允许 UDP 套接字进行广播
    if (setsockopt(satellite->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Error setting broadcast option");
        pthread_exit(NULL);
    }
    
    while (1) {
        // 发送心跳报文
        int bytes_sent = sendto(satellite->sockfd, &heartbeat_type, sizeof(int), MSG_CONFIRM,
                                (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        if (bytes_sent < 0) {
            perror("Heartbeat send failed");
        } else {
            printf("Heartbeat broadcast sent.\n");
        }

        sleep(SLEEP_TIME);
    }
    return NULL;
}

// void signal_handler(int signum) {
//     printf("\nTerminating satellite...\n");
//     exit_flag = 1;
// }

int main(){
    struct sockaddr_in satellite_addr;
    Sate *satellite = (Sate*) malloc(sizeof(Sate)); 

    if (!satellite) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    satellite->type = SATELLITE_CIRCLE;
    satellite->initialize();

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
    //把发送代码放入线程
    //使用每隔一段时间就发送一次数据的方式发送请求信息
    //初步使用有来自发送处的返回数据包信息就意味着是卫星的一个neighbor
    //后续可以优化为星间路由代码
    // pthread_t send_thread;
    // if (pthread_create(&send_thread, NULL, send_data, (void*)satellite) != 0) {
    //     perror("Failed to create send thread");
    //     exit(EXIT_FAILURE);
    // }
    // pthread_detach(send_thread);


    //这个地方需要分离出一个线程一直给基站发连接请求

    send_to_base_station(satellite->sockfd, satellite->missing_blocks, satellite->missing_count);
    
    //发送心跳报文，维护邻居信息
    pthread_t heartbeat_thread;
    if (pthread_create(&heartbeat_thread, NULL, send_heartbeat, (void*)satellite) != 0) {
        perror("Failed to create send thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(heartbeat_thread);


    //signal(SIGINT, signal_handler);
    while (!exit_flag) {
        sleep(1);
    }
    printf("Satellite shutting down...\n");
    close(satellite->sockfd);
    free(satellite);
    return 0;
}