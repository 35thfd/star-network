#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int data[1024][10000];     
    int missing_blocks[1024]; 
    int missing_count; 
    int frequency[1024]; 
} Satellite;

class Sate {
    public:
        SatelliteType type;  
        Satellite* neighbors[20];
        int id;              
        int sockfd;
        int data[1024][100000]; 
        int missing_blocks[1024];  
        int missing_count;  
        int frequency[1024]; 
        pthread_t monitor_thread;


    //void save_fragment(fragment_id, data);
    
};

