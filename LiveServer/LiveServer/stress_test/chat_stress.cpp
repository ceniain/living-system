#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define SERVER_IP "192.168.1.100"  // 改成你的虚拟机IP
#define SERVER_PORT 8000

// 全局变量（从环境变量读取）
unsigned long long ROOM_NO = 1234567890;

#define USER_COUNT 1000             // 模拟用户数
#define MSG_PER_USER 10             // 每个用户发送的消息数

// 协议结构体
typedef struct {
    int len;
} PackHeader;

typedef struct {
    int type;          // 10028
} HeartBeat;

typedef struct {
    int type;          // 10012
    unsigned long long room_no;
} IntoRoomRq;

typedef struct {
    int type;          // 10014
    int uid;
    char text[128];
} ChatMsgRq;

// 统计
int g_total_msgs = 0;
int g_success_msgs = 0;
int g_failed_msgs = 0;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t g_start_time = 0;

// 发送数据（带长度头）
int send_data(int sock, void* data, int len) {
    PackHeader header;
    header.len = len;
    if (send(sock, &header, sizeof(header), 0) < 0) return -1;
    if (send(sock, data, len, 0) < 0) return -1;
    return 0;
}

// 单个用户行为
void* user_thread(void* arg) {
    int uid = *(int*)arg;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    
    // 加入房间
    IntoRoomRq rq;
    rq.type = 10012;
    rq.room_no = ROOM_NO;
    if (send_data(sock, &rq, sizeof(rq)) < 0) {
        close(sock);
        return NULL;
    }
    
    // 等待加入成功回执（简化处理）
    usleep(100000);  // 100ms
    
    // 发送弹幕
    for (int i = 0; i < MSG_PER_USER; i++) {
        ChatMsgRq msg;
        msg.type = 10014;
        msg.uid = uid;
        snprintf(msg.text, sizeof(msg.text), "用户%d 弹幕消息%d", uid, i+1);
        
        if (send_data(sock, &msg, sizeof(msg)) == 0) {
            pthread_mutex_lock(&g_mutex);
            g_success_msgs++;
            pthread_mutex_unlock(&g_mutex);
        } else {
            pthread_mutex_lock(&g_mutex);
            g_failed_msgs++;
            pthread_mutex_unlock(&g_mutex);
        }
        
        // 控制发送频率
        usleep(50000);  // 50ms间隔
    }
    
    close(sock);
    return NULL;
}

int main() {
    // 从环境变量读取 ROOM_NO
    char* env_val = getenv("ROOM_NO");
    if (env_val) ROOM_NO = atoll(env_val);

    printf("[*] 弹幕压测开始\n");
    printf("[*] 服务器: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("[*] 房间号: %llu\n", ROOM_NO);
    printf("[*] 模拟用户: %d\n", USER_COUNT);
    printf("[*] 每用户消息: %d\n", MSG_PER_USER);
    printf("[*] 总消息数: %d\n", USER_COUNT * MSG_PER_USER);
    
    pthread_t threads[USER_COUNT];
    int uids[USER_COUNT];
    
    g_start_time = time(NULL);
    
    for (int i = 0; i < USER_COUNT; i++) {
        uids[i] = i + 10000;  // 从10000开始
        pthread_create(&threads[i], NULL, user_thread, &uids[i]);
        usleep(10000);  // 每10ms启动一个用户
    }
    
    for (int i = 0; i < USER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    
    time_t end = time(NULL);
    int duration = end - g_start_time;
    
    printf("\n[=] 压测完成\n");
    printf("[=] 总消息数: %d\n", USER_COUNT * MSG_PER_USER);
    printf("[=] 成功发送: %d\n", g_success_msgs);
    printf("[=] 失败发送: %d\n", g_failed_msgs);
    printf("[=] 耗时: %d 秒\n", duration);
    if (duration > 0) {
        printf("[=] 吞吐量: %.2f 条/秒\n", (float)g_success_msgs / duration);
    }
    
    return 0;
}
