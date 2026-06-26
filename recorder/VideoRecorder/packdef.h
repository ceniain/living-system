#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <iostream>
#include <map>
#include <list>
#include <QtGlobal>


//边界值
#define _DEF_SIZE           45
#define _DEF_BUFFERSIZE     1000
#define _DEF_PORT           8000
#define _DEF_SERVERIP       "0.0.0.0"
#define _DEF_LISTEN         128
#define _DEF_EPOLLSIZE      4096
#define _DEF_IPSIZE         16
#define _DEF_COUNT          10
#define _DEF_TIMEOUT        10
#define _DEF_SQLIEN         400
#define TRUE                true
#define FALSE               false



/*-------------数据库信息-----------------*/
#define _DEF_DB_NAME    "live_system"
#define _DEF_DB_IP      "localhost"
#define _DEF_DB_USER    "root"
#define _DEF_DB_PWD     "123456"
/*--------------------------------------*/
#define _MAX_PATH           (260)
#define _DEF_BUFFER         (4096)
#define _DEF_CONTENT_SIZE	(1024)
#define _MAX_SIZE           (40)

//自定义协议   先写协议头 再写协议结构
//登录 注册 获取好友信息 添加好友 聊天 发文件 下线请求
#define _DEF_PACK_BASE	(10000)
#define _DEF_PACK_COUNT (100)

//注册
#define _DEF_PACK_REGISTER_RQ	(_DEF_PACK_BASE + 0 )
#define _DEF_PACK_REGISTER_RS	(_DEF_PACK_BASE + 1 )
//登录
#define _DEF_PACK_LOGIN_RQ	(_DEF_PACK_BASE + 2 )
#define _DEF_PACK_LOGIN_RS	(_DEF_PACK_BASE + 3 )

//---------------- 直播业务 ----------------
#define _DEF_PACK_CREATE_ROOM_RQ      (_DEF_PACK_BASE + 10)  // 主播创建房间
#define _DEF_PACK_CREATE_ROOM_RS      (_DEF_PACK_BASE + 11)
#define _DEF_PACK_JOIN_ROOM_RQ        (_DEF_PACK_BASE + 12)  // 观众进房
#define _DEF_PACK_JOIN_ROOM_RS        (_DEF_PACK_BASE + 13)
#define _DEF_PACK_SEND_DANMU_RQ       (_DEF_PACK_BASE + 14)  // 发弹幕
#define _DEF_PACK_DANMU_BROADCAST     (_DEF_PACK_BASE + 15)  // 广播弹幕
#define _DEF_PACK_STOP_LIVE_RQ        (_DEF_PACK_BASE + 16)  // 关播
#define _DEF_PACK_STOP_LIVE_RS        (_DEF_PACK_BASE + 17)  // 关播
#define _DEF_ROOM_COUNT               (_DEF_PACK_BASE + 18)  //房间人数更新
#define _DEF_PACK_MUTE_CTRL_RQ        (_DEF_PACK_BASE + 19)  // 房管操作：全体禁言/解禁、单人禁言/解禁
#define _DEF_PACK_MUTE_SYNC_BROAD     (_DEF_PACK_BASE + 20)  // 房管操作：全体禁言/解禁、单人禁言/解禁
#define _DEF_PACK_SET_ADMIN_RQ        (_DEF_PACK_BASE + 21)
#define _DEF_PACK_SET_ADMIN_RS        (_DEF_PACK_BASE + 22)
#define _DEF_PACK_ADMIN_SYNC_BROAD     (_DEF_PACK_BASE + 23)
#define _DEF_PACK_ONLINE_USER_SYNC    (_DEF_PACK_BASE + 24)
#define _DEF_PACK_SYS_NOTICE_BROAD    (_DEF_PACK_BASE + 25)//系统上下线/房管变更公告 协议
#define _DEF_PACK_LEAVE_ROOM_RQ        (_DEF_PACK_BASE + 26)//退房请求
#define _DEF_PACK_LEAVE_ROOM_RS        (_DEF_PACK_BASE + 27)//退房回复
#define _DEF_PACK_HEART_BEAT            (_DEF_PACK_BASE + 28)
#define _DEF_PACK_GET_ROOM_LIST_RQ      (_DEF_PACK_BASE + 29)
#define _DEF_PACK_GET_ROOM_LIST_RS      (_DEF_PACK_BASE + 30)

// 结果定义
#define CREATE_ROOM_SUCCESS 1
#define JOIN_ROOM_SUCCESS 1
#define ROOM_NOT_EXIST 0

//返回的结果
//注册请求的结果
#define user_is_exist		(0)
#define register_success	(1)
//登录请求的结果
#define user_not_exist		(0)
#define password_error		(1)
#define login_success		(2)
//创建房间结果
#define create_room_success 1
#define create_room_fail    0
//加入房间结果
#define join_room_success   1
#define join_room_fail      0
//弹幕结果
#define danmu_success       1
//关播结果
#define stop_success        1
#define stop_fail           0

// 消息类型定义
#define MSG_COMMON    1  // 普通评论
#define MSG_GIFT      2  // 礼物消息

typedef int PackType;

//协议结构
//注册
typedef struct STRU_REGISTER_RQ
{
    STRU_REGISTER_RQ():type(_DEF_PACK_REGISTER_RQ)
    {
        memset( tel  , 0, sizeof(tel));
        memset( name  , 0, sizeof(name));
        memset( password , 0, sizeof(password) );
    }
    //需要手机号码 , 密码, 昵称
    PackType type;
    char tel[_MAX_SIZE];
    char name[_MAX_SIZE];
    char password[_MAX_SIZE];

}STRU_REGISTER_RQ;

typedef struct STRU_REGISTER_RS
{
    //回复结果
    STRU_REGISTER_RS(): type(_DEF_PACK_REGISTER_RS) , result(register_success)
    {
    }
    PackType type;
    int result;

}STRU_REGISTER_RS;

//登录
typedef struct STRU_LOGIN_RQ
{
    //登录需要: 手机号 密码
    STRU_LOGIN_RQ():type(_DEF_PACK_LOGIN_RQ)
    {
        memset( name , 0, sizeof(name) );
        memset( password , 0, sizeof(password) );
    }
    PackType type;
    char name[_MAX_SIZE];
    char password[_MAX_SIZE];

}STRU_LOGIN_RQ;

typedef struct STRU_LOGIN_RS
{
    // 需要 结果 , 用户的id
    STRU_LOGIN_RS(): type(_DEF_PACK_LOGIN_RS) , result(login_success),userid(0)
    {
    }
    PackType type;
    int result;
    int userid;

}STRU_LOGIN_RS;

//创建房间协议
typedef struct STRU_CREATE_ROOM_RQ
{
    STRU_CREATE_ROOM_RQ():type(_DEF_PACK_CREATE_ROOM_RQ)
    {
        memset(room_name,0,sizeof(room_name));
        userid=0;
    }
    PackType type;
    int userid;
    char room_name[64];
}STRU_CREATE_ROOM_RQ;

typedef struct STRU_CREATE_ROOM_RS
{
    STRU_CREATE_ROOM_RS():type(_DEF_PACK_CREATE_ROOM_RS),result(0){}
    PackType type;
    int result;
    unsigned long long room_no; //对外长房间号
}STRU_CREATE_ROOM_RS;

//加入房间协议
typedef struct STRU_JOIN_ROOM_RQ
{
    STRU_JOIN_ROOM_RQ():type(_DEF_PACK_JOIN_ROOM_RQ)
    {
        userid=0;
        room_no=0;
    }
    PackType type;
    int userid;
    unsigned long long room_no;
}STRU_JOIN_ROOM_RQ;

typedef struct STRU_JOIN_ROOM_RS
{
    STRU_JOIN_ROOM_RS():type(_DEF_PACK_JOIN_ROOM_RS),result(0),room_no(0)
    {
        memset(rtmp_url,0,sizeof(rtmp_url));
    }
    PackType type;
    int result;
    unsigned long long room_no;
    char rtmp_url[128];
    int host_uid;  // ✅ 必须加这一行！
}STRU_JOIN_ROOM_RS;

// 发送弹幕
typedef struct STRU_SEND_DANMU_RQ
{
    STRU_SEND_DANMU_RQ():type(_DEF_PACK_SEND_DANMU_RQ)
    {
        memset(msg,0,sizeof(msg));
        memset(username, 0, sizeof(username));  // 初始化用户名
        userid = 0;
        room_no = 0;
    }
    PackType type;
    int userid;
    unsigned long long room_no;
    char username[32];   // 新增：发送者用户名
    char msg[256];
}STRU_SEND_DANMU_RQ;

/// 弹幕广播（服务端→所有客户端）
typedef struct STRU_DANMU_BROADCAST
{
    STRU_DANMU_BROADCAST() : type(_DEF_PACK_DANMU_BROADCAST)
    {
        memset(msg, 0, sizeof(msg));
        memset(username, 0, sizeof(username));  // 初始化用户名
        userid = 0;
        room_no = 0;
    }
    PackType type;
    int userid;
    unsigned long long room_no;
    char username[32];   // 新增：发送者用户名
    char msg[256];
}STRU_DANMU_BROADCAST;
// 关闭直播（主播）
typedef struct STRU_STOP_LIVE_RQ
{
    STRU_STOP_LIVE_RQ() : type(_DEF_PACK_STOP_LIVE_RQ)
    {
        userid = 0;
        room_no = 0;
    }
    PackType type;
    int userid;
    unsigned long long room_no;  // 改成数字！
}STRU_STOP_LIVE_RQ;

typedef struct STRU_STOP_LIVE_RS
{
    STRU_STOP_LIVE_RS() : type(_DEF_PACK_STOP_LIVE_RQ), result(1)
    {}
    PackType type;
    int result;

}STRU_STOP_LIVE_RS;
//发送消息
struct ChatMsg {
    int type;         // 1=普通 2=礼物
    char content[512];// 消息内容
};
typedef struct STRU_ROOM_COUNT_BROADCAST
{
    STRU_ROOM_COUNT_BROADCAST()
        : type(_DEF_ROOM_COUNT),
          room_no(0),
          count(0)
    {}

    PackType type;       // 协议号（固定）
    qulonglong room_no;  // 房间号
    int count;           // 当前在线人数

} STRU_ROOM_COUNT_BROADCAST;

// 禁言操作类型
enum MuteOpType
{
    OP_MUTE_ALL     = 1,    // 全体禁言
    OP_UNMUTE_ALL   = 2,    // 全体解禁
    OP_MUTE_ONE     = 3,    // 禁言单个用户
    OP_UNMUTE_ONE   = 4     // 解除单个用户禁言
};
typedef struct STRU_MUTE_CTRL_RQ
{
    STRU_MUTE_CTRL_RQ()
        : type(_DEF_PACK_MUTE_CTRL_RQ)
    {
        opType      = 0;
        adminUid    = 0;
        targetUid   = 0;
        room_no     = 0;
        memset(adminName, 0, sizeof(adminName));
    }

    PackType type;          // 协议号 _DEF_PACK_MUTE_CTRL_RQ
    int opType;             // MuteOpType 操作类型
    int adminUid;           // 操作人ID(主播/房管)
    int targetUid;          // 目标用户ID(全体禁言填0)
    unsigned long long room_no; // 房间号
    char adminName[32];     // 操作人昵称
}STRU_MUTE_CTRL_RQ;
// 禁言状态同步广播包 10020
typedef struct STRU_MUTE_SYNC_BROAD
{
    STRU_MUTE_SYNC_BROAD()
        : type(_DEF_PACK_MUTE_SYNC_BROAD)
    {
        isAllMute   = 0;
        muteCount   = 0;
        room_no     = 0;
        memset(muteUid, 0, sizeof(muteUid));
    }

    PackType type;                  // 协议号 _DEF_PACK_MUTE_SYNC_BROAD
    int isAllMute;                  // 1=全体禁言  0=正常
    int muteCount;                  // 单人禁言人数
    unsigned long long room_no;     // 房间号
    int muteUid[64];                // 单人禁言ID列表(最大64人)
}STRU_MUTE_SYNC_BROAD;
// 房管列表同步广播包（服务端 → 所有客户端）
typedef struct STRU_ADMIN_SYNC_BROAD
{
    STRU_ADMIN_SYNC_BROAD()
        : type(_DEF_PACK_ADMIN_SYNC_BROAD)
    {
        adminCnt = 0;
        room_no  = 0;
        memset(adminUid, 0, sizeof(adminUid));
    }

    PackType type;                  // 协议号 _DEF_PACK_ADMIN_SYNC_BROAD
    int adminCnt;                   // 房管人数
    unsigned long long room_no;     // 房间号
    int adminUid[32];               // 房管UID列表（最多32人）
} STRU_ADMIN_SYNC_BROAD;

// 房间在线用户列表同步广播包（服务端 → 所有客户端）
typedef struct STRU_ONLINE_USER_SYNC
{
    STRU_ONLINE_USER_SYNC()
        : type(_DEF_PACK_ONLINE_USER_SYNC)
    {
        userCnt = 0;
        room_no = 0;
        memset(userUid, 0, sizeof(userUid));
        memset(userName, 0, sizeof(userName));
    }

    PackType type;                  // 协议号 _DEF_PACK_ONLINE_USER_SYNC
    int userCnt;                    // 在线用户数
    unsigned long long room_no;     // 房间号
    int userUid[64];                // 用户UID列表（最多64人）
    char userName[64][32];          // 用户名列表（每个昵称最多32字节）
} STRU_ONLINE_USER_SYNC;
// 设置/移除房管 请求包（客户端 → 服务端）
typedef struct STRU_SET_ADMIN_RQ
{
    STRU_SET_ADMIN_RQ()
        : type(_DEF_PACK_SET_ADMIN_RQ)
    {
        opType      = 0;
        operUid     = 0;
        targetUid   = 0;
        room_no     = 0;
        memset(operName, 0, sizeof(operName));
    }

    PackType type;          // 协议号 _DEF_PACK_SET_ADMIN_RQ
    int opType;             // AdminOpType 1添加 2移除
    int operUid;            // 操作人UID(主播/房管)
    int targetUid;          // 被操作用户UID
    unsigned long long room_no; // 房间号
    char operName[32];      // 操作人昵称
} STRU_SET_ADMIN_RQ;

// 设置/移除房管 回执包（服务端 → 操作客户端）
typedef struct STRU_SET_ADMIN_RS
{
    STRU_SET_ADMIN_RS()
        : type(_DEF_PACK_SET_ADMIN_RS), result(0)
    {
    }

    PackType type;          // 协议号 _DEF_PACK_SET_ADMIN_RS
    int result;             // 1成功 0失败
} STRU_SET_ADMIN_RS;
// 房管操作类型
enum AdminOpType
{
    OP_ADD_ADMIN    = 1,    // 添加房管
    OP_DEL_ADMIN    = 2     // 移除房管
};
// 系统通知类型
enum SYS_NOTICE_TYPE
{
    SYS_USER_ENTER  = 1,    // 用户进入房间
    SYS_USER_LEAVE  = 2,    // 用户退出房间
    SYS_SET_ADMIN   = 3,    // 被设为房管
    SYS_DEL_ADMIN   = 4     // 被移除房管
};
typedef struct Room
{
    unsigned long long room_no;    // 数字房间号
    char room_name[64];            // 房间名
    int host_userid;               // 主播ID
    std::list<int> client_fds;     // 房间内所有用户fd

    int is_all_mute;               // 1=全体禁言 0=正常
    int mute_user_cnt;             // 单人禁言人数
    int mute_uid_list[64];         // 单人禁言ID列表
    int admin_cnt;                 // 当前房管人数
    int admin_uid_list[32];        // 房管UID数组(最大32人)

    Room() : room_no(0), host_userid(0)
    {
        memset(room_name, 0, sizeof(room_name));
        // 初始化禁言状态
        is_all_mute = 0;
        mute_user_cnt = 0;
        memset(mute_uid_list, 0, sizeof(mute_uid_list));
        admin_cnt = 0;
        memset(admin_uid_list, 0, sizeof(admin_uid_list));

    }

} Room;
// 系统公告广播包
typedef struct tagSTRU_SYS_NOTICE_BROAD
{
    unsigned int        type;           // 协议号：_DEF_PACK_SYS_NOTICE_BROAD
    unsigned long long  room_no;        // 房间号(小端)
    int                 notice_type;    // 通知类型 SYS_NOTICE_TYPE
    int                 target_uid;     // 目标用户UID
    char                target_name[32];// 目标昵称
}STRU_SYS_NOTICE_BROAD;
// 退出房间请求协议
typedef struct STRU_LEAVE_ROOM_RQ
{
    STRU_LEAVE_ROOM_RQ():type(_DEF_PACK_LEAVE_ROOM_RQ)
    {
        room_no = 0;
    }
    PackType type;
    unsigned long long room_no;
}STRU_LEAVE_ROOM_RQ;
// 退出房间回执协议
typedef struct STRU_LEAVE_ROOM_RS
{
    STRU_LEAVE_ROOM_RS():type(_DEF_PACK_LEAVE_ROOM_RS)
    {
        result = 0;
    }
    PackType type;
    int result;
}STRU_LEAVE_ROOM_RS;
// 心跳结构体（极简，只需要协议头即可，无额外字段）
typedef struct STRU_HEART_BEAT
{
    STRU_HEART_BEAT():type(_DEF_PACK_HEART_BEAT)
    {

    }
    int type;
}STRU_HEART_BEAT;
// 单条房间信息项（对应Room核心字段，用于列表展示）
typedef struct RoomItem
{
    unsigned long long room_no;
    char room_name[64];
    int host_userid;
    int online_count;    // 在线人数 = client_fds.size()
    int live_status;     // 直播状态 0=下播 1=开播

    RoomItem() : room_no(0), host_userid(0), online_count(0), live_status(0)
    {
        memset(room_name, 0, sizeof(room_name));
    }
} RoomItem;

// 获取房间列表 请求包
typedef struct STRU_GET_ROOM_LIST_RQ
{
    PackType type;
    int page_index;
    int page_size;
    int sort_type;
    char search_key[32];

    STRU_GET_ROOM_LIST_RQ() : type(_DEF_PACK_GET_ROOM_LIST_RQ), page_index(0), page_size(0), sort_type(0)
    {
        memset(search_key, 0, sizeof(search_key));
    }
} STRU_GET_ROOM_LIST_RQ;

// 获取房间列表 响应包
typedef struct STRU_GET_ROOM_LIST_RS
{
    PackType type;
    int total_page;
    int cur_page;
    int item_cnt;
    RoomItem items[20];

    STRU_GET_ROOM_LIST_RS() : type(_DEF_PACK_GET_ROOM_LIST_RS), total_page(0), cur_page(0), item_cnt(0)
    {
        // 数组会逐个调用RoomItem构造，无需额外memset
    }
} STRU_GET_ROOM_LIST_RS;
