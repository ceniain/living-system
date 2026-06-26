#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 8000
#define _MAX_SIZE 40

unsigned long long ROOM_NO = 1234567890;

#define USER_COUNT 500
#define BEHAVIOR_DURATION 60

// ===== 协议结构体（与服务端 packdef.h 完全一致） =====

typedef struct {
    int type;
    char tel[_MAX_SIZE];
    char name[_MAX_SIZE];
    char password[_MAX_SIZE];
} STRU_REGISTER_RQ;

typedef struct {
    int type;
    int result;
} STRU_REGISTER_RS;

typedef struct {
    int type;
    char name[_MAX_SIZE];
    char password[_MAX_SIZE];
} STRU_LOGIN_RQ;

typedef struct {
    int type;
    int result;
    int userid;
} STRU_LOGIN_RS;

typedef struct {
    int type;
    int userid;
    unsigned long long room_no;
} STRU_JOIN_ROOM_RQ;

typedef struct {
    int type;
    int result;
    unsigned long long room_no;
    char rtmp_url[128];
    int host_uid;
} STRU_JOIN_ROOM_RS;

typedef struct {
    int type;
    int userid;
    unsigned long long room_no;
    char username[32];
    char msg[256];
} STRU_SEND_DANMU_RQ;

typedef struct {
    int type;
    unsigned long long room_no;
} STRU_LEAVE_ROOM_RQ;

typedef struct {
    int type;
    int result;
} STRU_LEAVE_ROOM_RS;

typedef struct {
    int type;
} STRU_HEART_BEAT;

// 统计
int g_online = 0;
int g_joined = 0;
int g_chatted = 0;
int g_left = 0;
int g_failed = 0;
int g_logged_in = 0;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// ===== 发送/接收辅助函数（带日志） =====

int send_data(int sock, void* data, int len, const char* tag) {
    printf("[LOG][%s] 准备发送 %d 字节数据...\n", tag, len);
    // 先发送4字节长度头
    int header = len;
    int sent = 0;
    while (sent < 4) {
        int n = send(sock, ((char*)&header) + sent, 4 - sent, 0);
        if (n <= 0) {
            printf("[LOG][%s] 发送长度头失败: %s\n", tag, strerror(errno));
            return -1;
        }
        sent += n;
    }
    printf("[LOG][%s] 长度头发送成功 (body_len=%d)\n", tag, header);
    // 再发送body
    int total = 0;
    while (total < len) {
        int n = send(sock, (char*)data + total, len - total, 0);
        if (n <= 0) {
            printf("[LOG][%s] 发送body失败: %s (已发%d/总%d)\n", tag, strerror(errno), total, len);
            return -1;
        }
        total += n;
    }
    printf("[LOG][%s] body发送成功 (%d字节)\n", tag, total);
    return 0;
}

int recv_data(int sock, void* data, int len) {
    int total = 0;
    while (total < len) {
        int n = recv(sock, (char*)data + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

// 读取完整协议（先读4字节长度头，再读body）
int recv_pack(int sock, void* data, int max_len, const char* tag) {
    int body_len = 0;
    printf("[LOG][%s] 等待接收长度头...\n", tag);
    if (recv_data(sock, &body_len, sizeof(body_len)) < 0) {
        printf("[LOG][%s] 接收长度头失败: %s\n", tag, strerror(errno));
        return -1;
    }
    printf("[LOG][%s] 收到长度头: body_len=%d\n", tag, body_len);
    if (body_len > max_len || body_len <= 0) {
        printf("[LOG][%s] 长度异常: body_len=%d (max=%d)\n", tag, body_len, max_len);
        return -1;
    }
    int ret = recv_data(sock, data, body_len);
    if (ret < 0) {
        printf("[LOG][%s] 接收body失败: %s\n", tag, strerror(errno));
        return -1;
    }
    printf("[LOG][%s] 收到body: %d字节, type=%d\n", tag, ret, *(int*)data);
    return ret;
}

// 单个用户完整行为
void* user_behavior(void* arg) {
    int uid = *(int*)arg;
    char log_tag[64];
    snprintf(log_tag, sizeof(log_tag), "User%d", uid);

    printf("[LOG][%s] === 开始执行 ===\n", log_tag);

    // 创建socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[LOG][%s] socket()失败: %s\n", log_tag, strerror(errno));
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }
    printf("[LOG][%s] socket创建成功, fd=%d\n", log_tag, sock);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("[LOG][%s] 正在连接 %s:%d...\n", log_tag, SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[LOG][%s] connect()失败: %s\n", log_tag, strerror(errno));
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }
    printf("[LOG][%s] TCP连接成功!\n", log_tag);

    // === 阶段0: 注册 ===
    char username[32];
    snprintf(username, sizeof(username), "压测用户%d", uid);
    char tel[32];
    snprintf(tel, sizeof(tel), "138%08d", uid);

    printf("[LOG][%s] --- 阶段0: 注册 (tel=%s, name=%s) ---\n", log_tag, tel, username);

    STRU_REGISTER_RQ reg;
    memset(&reg, 0, sizeof(reg));
    reg.type = 10000;
    strncpy(reg.tel, tel, _MAX_SIZE - 1);
    strncpy(reg.name, username, _MAX_SIZE - 1);
    strncpy(reg.password, "123456", _MAX_SIZE - 1);

    if (send_data(sock, &reg, sizeof(reg), log_tag) < 0) {
        printf("[LOG][%s] 注册发送失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }

    // 接收注册回执
    STRU_REGISTER_RS reg_rs;
    memset(&reg_rs, 0, sizeof(reg_rs));
    if (recv_pack(sock, &reg_rs, sizeof(reg_rs), log_tag) < 0) {
        printf("[LOG][%s] 注册回执接收失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }
    printf("[LOG][%s] 注册回执: type=%d, result=%d\n", log_tag, reg_rs.type, reg_rs.result);

    usleep(100000);  // 100ms

    // === 阶段1: 登录 ===
    printf("[LOG][%s] --- 阶段1: 登录 (name=%s) ---\n", log_tag, username);

    STRU_LOGIN_RQ login;
    memset(&login, 0, sizeof(login));
    login.type = 10002;
    strncpy(login.name, username, _MAX_SIZE - 1);
    strncpy(login.password, "123456", _MAX_SIZE - 1);

    if (send_data(sock, &login, sizeof(login), log_tag) < 0) {
        printf("[LOG][%s] 登录发送失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }

    // 接收登录回执
    STRU_LOGIN_RS login_rs;
    memset(&login_rs, 0, sizeof(login_rs));
    if (recv_pack(sock, &login_rs, sizeof(login_rs), log_tag) < 0) {
        printf("[LOG][%s] 登录回执接收失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }
    printf("[LOG][%s] 登录回执: type=%d, result=%d, userid=%d\n", log_tag, login_rs.type, login_rs.result, login_rs.userid);

    if (login_rs.result != 2) {  // login_success = 2
        printf("[LOG][%s] 登录失败! result=%d (期望2)\n", log_tag, login_rs.result);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }

    int real_uid = login_rs.userid;
    printf("[LOG][%s] 登录成功! real_uid=%d\n", log_tag, real_uid);

    pthread_mutex_lock(&g_mutex);
    g_logged_in++;
    pthread_mutex_unlock(&g_mutex);

    usleep(200000);  // 200ms

    // === 阶段2: 加入房间 ===
    printf("[LOG][%s] --- 阶段2: 加入房间 (uid=%d, room=%llu) ---\n", log_tag, real_uid, (unsigned long long)ROOM_NO);

    STRU_JOIN_ROOM_RQ join;
    memset(&join, 0, sizeof(join));
    join.type = 10012;
    join.userid = real_uid;
    join.room_no = ROOM_NO;

    if (send_data(sock, &join, sizeof(join), log_tag) < 0) {
        printf("[LOG][%s] 加房发送失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }

    // 接收加房回执
    STRU_JOIN_ROOM_RS join_rs;
    memset(&join_rs, 0, sizeof(join_rs));
    if (recv_pack(sock, &join_rs, sizeof(join_rs), log_tag) < 0) {
        printf("[LOG][%s] 加房回执接收失败!\n", log_tag);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }
    printf("[LOG][%s] 加房回执: type=%d, result=%d, room_no=%llu\n", log_tag, join_rs.type, join_rs.result, (unsigned long long)join_rs.room_no);

    if (join_rs.result != 1) {  // join_room_success = 1
        printf("[LOG][%s] 加房失败! result=%d (期望1)\n", log_tag, join_rs.result);
        close(sock);
        pthread_mutex_lock(&g_mutex);
        g_failed++;
        pthread_mutex_unlock(&g_mutex);
        return NULL;
    }

    printf("[LOG][%s] 加房成功!\n", log_tag);
    pthread_mutex_lock(&g_mutex);
    g_joined++;
    pthread_mutex_unlock(&g_mutex);

    usleep(200000);

    // === 阶段3: 发送弹幕 ===
    printf("[LOG][%s] --- 阶段3: 发送弹幕 ---\n", log_tag);
    int msg_count = rand() % 3 + 1;
    for (int i = 0; i < msg_count; i++) {
        STRU_SEND_DANMU_RQ msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = 10014;
        msg.userid = real_uid;
        msg.room_no = ROOM_NO;
        strncpy(msg.username, username, 31);
        snprintf(msg.msg, sizeof(msg.msg), "弹幕%d-%d", uid, i + 1);

        if (send_data(sock, &msg, sizeof(msg), log_tag) < 0) {
            printf("[LOG][%s] 弹幕发送失败! (第%d条)\n", log_tag, i + 1);
            break;
        }
        printf("[LOG][%s] 弹幕%d发送成功: %s\n", log_tag, i + 1, msg.msg);

        pthread_mutex_lock(&g_mutex);
        g_chatted++;
        pthread_mutex_unlock(&g_mutex);

        usleep(500000 + rand() % 1000000);
    }

    // === 阶段4: 停留 ===
    int stay_time = rand() % 10 + 5;
    printf("[LOG][%s] --- 阶段4: 停留 %d 秒 ---\n", log_tag, stay_time);
    sleep(stay_time);

    // === 阶段5: 退出房间 ===
    printf("[LOG][%s] --- 阶段5: 退出房间 ---\n", log_tag);
    STRU_LEAVE_ROOM_RQ leave;
    memset(&leave, 0, sizeof(leave));
    leave.type = 10026;
    leave.room_no = ROOM_NO;
    send_data(sock, &leave, sizeof(leave), log_tag);

    pthread_mutex_lock(&g_mutex);
    g_left++;
    pthread_mutex_unlock(&g_mutex);

    printf("[LOG][%s] === 执行完成，关闭连接 ===\n", log_tag);
    close(sock);
    return NULL;
}

int main() {
    char* env_val = getenv("ROOM_NO");
    if (env_val) ROOM_NO = atoll(env_val);

    printf("[*] 综合场景压测开始\n");
    printf("[*] 服务器: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("[*] 模拟用户: %d\n", USER_COUNT);
    printf("[*] 房间号: %llu\n", (unsigned long long)ROOM_NO);
    printf("[*] 日志说明: 只看 [LOG] 开头的行\n\n");

    srand(time(NULL));
    pthread_t threads[USER_COUNT];
    int uids[USER_COUNT];

    time_t start = time(NULL);

    for (int i = 0; i < USER_COUNT; i++) {
        uids[i] = i + 10000;
        pthread_create(&threads[i], NULL, user_behavior, &uids[i]);
        usleep(50000);  // 每50ms启动一个用户
    }

    for (int i = 0; i < USER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    time_t end = time(NULL);
    int duration = end - start;

    printf("\n[=] 压测完成\n");
    printf("[=] 登录成功: %d\n", g_logged_in);
    printf("[=] 加入房间: %d\n", g_joined);
    printf("[=] 发送弹幕: %d\n", g_chatted);
    printf("[=] 退出房间: %d\n", g_left);
    printf("[=] 失败: %d\n", g_failed);
    printf("[=] 耗时: %d 秒\n", duration);

    return 0;
}
