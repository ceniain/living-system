#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define SERVER_IP "192.168.1.100"  // 改成你的虚拟机IP
#define SERVER_PORT 8000

unsigned long long ROOM_NO = 0;

#define TARGET_CONNECTIONS 5000
#define THREAD_COUNT 50

typedef struct {
    int len;
} PackHeader;

typedef struct {
    int type;
} HeartBeat;

int g_connected = 0;
int g_failed = 0;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int g_stop = 0;  // 停止标志

// 创建一个连接并发送心跳
void* create_connection(void* arg) {
    int thread_id = *(int*)arg;
    int start = thread_id * (TARGET_CONNECTIONS / THREAD_COUNT);
    int end = start + (TARGET_CONNECTIONS / THREAD_COUNT);

    for (int i = start; i < end; i++) {
        if (g_stop) break;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            g_failed++;
            continue;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = inet_addr(SERVER_IP);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            g_failed++;
            continue;
        }

        // 发送心跳包
        HeartBeat hb;
        hb.type = 10028;
        PackHeader header;
        header.len = sizeof(HeartBeat);

        if (send(sock, &header, sizeof(header), 0) < 0 ||
            send(sock, &hb, sizeof(hb), 0) < 0) {
            close(sock);
            g_failed++;
            continue;
        }

        pthread_mutex_lock(&g_mutex);
        g_connected++;
        if (g_connected % 100 == 0) {
            printf("[+] 已建立 %d 个连接\n", g_connected);
        }
        pthread_mutex_unlock(&g_mutex);

        // 保持连接，每3秒发一次心跳，直到收到停止信号
        while (!g_stop) {
            sleep(3);
            if (g_stop) break;
            if (send(sock, &header, sizeof(header), 0) < 0 ||
                send(sock, &hb, sizeof(hb), 0) < 0) {
                close(sock);
                break;
            }
        }
        close(sock);
    }
    return NULL;
}

int main() {
    char* env_val = getenv("ROOM_NO");
    if (env_val) ROOM_NO = atoll(env_val);

    printf("[*] 开始压测: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("[*] 目标连接数: %d\n", TARGET_CONNECTIONS);
    printf("[*] 线程数: %d\n", THREAD_COUNT);

    pthread_t threads[THREAD_COUNT];
    int thread_ids[THREAD_COUNT];

    time_t start = time(NULL);

    // 创建线程建立连接
    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, create_connection, &thread_ids[i]);
    }

    // 等待所有连接建立完成（最多等60秒）
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    time_t end = time(NULL);
    int duration = end - start;

    printf("\n[=] 压测完成\n");
    printf("[=] 成功连接: %d\n", g_connected);
    printf("[=] 失败连接: %d\n", g_failed);
    printf("[=] 耗时: %d 秒\n", duration);

    // 保持连接观察30秒，然后退出
    printf("[*] 保持连接观察30秒...\n");
    for (int i = 0; i < 10; i++) {
        sleep(3);
        printf("[*] 当前活跃连接: %d\n", g_connected);
    }

    // 停止所有心跳线程
    g_stop = 1;
    sleep(2);

    return 0;
}
