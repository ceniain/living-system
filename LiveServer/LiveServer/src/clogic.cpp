#include "./include/clogic.h"
#include "./include/packdef.h"
#include <algorithm>

typedef void (CLogic::*DealNetPack)(sock_fd clientfd,char* szbuf,int nlen);
DealNetPack g_netPackMap[_DEF_PACK_COUNT] = {nullptr};
std::map<int, unsigned long long> g_userRoomMap;
// 全局：fd 对应 登录UID
std::map<sock_fd, int> g_fdToUidMap;
// 全局房间列表：roomNo → Room 对象
std::map<unsigned long long, Room> g_roomMap;
pthread_mutex_t g_globalMapMutex = PTHREAD_MUTEX_INITIALIZER;
void CLogic::setNetPackMap()
{
    int off = _DEF_PACK_BASE;
    g_netPackMap[_DEF_PACK_REGISTER_RQ - off] = &CLogic::RegisterRq;
    g_netPackMap[_DEF_PACK_LOGIN_RQ    - off] = &CLogic::LoginRq;
    g_netPackMap[_DEF_PACK_CREATE_ROOM_RQ - off] = &CLogic::CreateRoomRq;
    g_netPackMap[_DEF_PACK_JOIN_ROOM_RQ   - off] = &CLogic::JoinRoomRq;
    g_netPackMap[_DEF_PACK_SEND_DANMU_RQ  - off] = &CLogic::SendDanmuRq;
    g_netPackMap[_DEF_PACK_STOP_LIVE_RQ   - off] = &CLogic::StopLiveRq;
    g_netPackMap[_DEF_PACK_MUTE_CTRL_RQ - off] = &CLogic::MuteCtrlRq;
    g_netPackMap[_DEF_PACK_SET_ADMIN_RQ - off] = &CLogic::SetAdminRq;
    g_netPackMap[_DEF_PACK_LEAVE_ROOM_RQ - off] = &CLogic::LeaveRoomRq;
    // 心跳协议分发
    g_netPackMap[_DEF_PACK_HEART_BEAT - off] = &CLogic::OnHeartBeat;
    g_netPackMap[_DEF_PACK_GET_ROOM_LIST_RQ - off] = &CLogic::GetRoomListRq;

}

// ==============================================
// 注册
// ==============================================
void CLogic::RegisterRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_REGISTER_RQ* pRq = (STRU_REGISTER_RQ*)szbuf;
    char sql[256];
    sprintf(sql, "select id from user where username='%s'", pRq->name);

    STRU_REGISTER_RS rs;
    memset(&rs, 0, sizeof(rs));
    rs.type = _DEF_PACK_REGISTER_RS;  // ✅ 不再转小端

    int cnt = m_sql->QueryMysql(sql);
    if (cnt > 0)
    {
        rs.result = user_is_exist;
    }
    else
    {
        sprintf(sql,"insert into user(username,password,tel,create_time) values('%s','%s','%s',now())",
                pRq->name,pRq->password,pRq->tel);
        m_sql->UpdataMysql(sql);
        rs.result = register_success;
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

// ==============================================
// 登录
// ==============================================
void CLogic::LoginRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_LOGIN_RQ* pRq = (STRU_LOGIN_RQ*)szbuf;

    char sql[256];
    sprintf(sql, "select id from user where username='%s' and password='%s'",
            pRq->name, pRq->password);

    STRU_LOGIN_RS rs;
    rs.type = _DEF_PACK_LOGIN_RS;  // ✅ 不再转小端

    list<string> lst;
    if (m_sql->SelectMysql(sql, 1, lst) && !lst.empty())
    {
        rs.result = login_success;
        rs.userid = atoi(lst.front().c_str());
        // LoginRq 登录成功处添加
        pthread_mutex_lock(&g_globalMapMutex);
        g_fdToUidMap[clientfd] = rs.userid;
        pthread_mutex_unlock(&g_globalMapMutex);
    }
    else
    {
        rs.result = password_error;
        rs.userid = 0;
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

// ==============================================
// 创建房间
// ==============================================
#include <cstdlib>
#include <ctime>

void CLogic::CreateRoomRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_CREATE_ROOM_RQ* rq = (STRU_CREATE_ROOM_RQ*)szbuf;
    STRU_CREATE_ROOM_RS rs;
    rs.type = _DEF_PACK_CREATE_ROOM_RS;  // ✅ 不再转小端
    rs.result = 0;
    unsigned long long newRoomNo = 0;

    char sql[512];
    sprintf(sql,
            "insert into room(room_name,anchor_uid,live_status,room_no) values('%s',%llu,1,0)",
            rq->room_name, le64toh(rq->userid));

    if(m_sql->UpdataMysql(sql))
    {
        char sql_getid[256];
        sprintf(sql_getid, "SELECT LAST_INSERT_ID()");
        list<string> lstId;
        m_sql->SelectMysql(sql_getid,1,lstId);

        if(!lstId.empty())
        {
            unsigned long long dbAutoId = stoull(lstId.front());
            srand((unsigned)time(NULL)+clientfd);
            newRoomNo = 1000000000ULL + rand()%9000000000ULL;

            char sql_update[512];
            sprintf(sql_update,
                    "UPDATE room SET room_no=%llu WHERE room_id=%llu",
                    newRoomNo, dbAutoId);
            m_sql->UpdataMysql(sql_update);

            rs.result = 1;
            rs.room_no = htole64(newRoomNo);

            m_pKernel->AddRoomInfo(newRoomNo, clientfd, rq->room_name);
            m_tcp->AddClientToRoom(newRoomNo, clientfd);
            printf("【创建房间成功】主播fd:%d 房间号:%llu 房间名:%s\n",clientfd,newRoomNo,rq->room_name);
            BroadcastAdminList(newRoomNo);
            BroadcastOnlineUserList(newRoomNo);

            // 通知所有客户端刷新大厅列表
            STRU_ROOM_LIST_UPDATE_NOTIFY notify;
            notify.update_type = 1; // 1=新房间开播
            m_tcp->BroadcastToAllClients((char*)&notify, sizeof(notify));
            printf("【大厅通知】已推送新房间开播通知\n");
        }
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}

// ==============================================
// 加入房间（核心修复）
// ==============================================
//void CLogic::JoinRoomRq(sock_fd clientfd, char* szbuf, int nlen)
//{
//    STRU_JOIN_ROOM_RQ* rq = (STRU_JOIN_ROOM_RQ*)szbuf;
//    cout << "STRU_JOIN_ROOM_RS size = " << sizeof(STRU_JOIN_ROOM_RS)<<endl;
//    // 转码（正确）
//    unsigned long long room_no = le64toh(rq->room_no);
//    unsigned long long userid = le64toh(rq->userid);

//    // 🔥 打印看看【真实房间号】到底是多少
//    printf("=====================================\n");
//    printf("🔥 客户端发来的原始 room_no: %llu\n", rq->room_no);
//    printf("🔥 转码后真实房间号        : %llu\n", room_no);
//    printf("=====================================\n");

//    STRU_JOIN_ROOM_RS rs;
//    rs.type = _DEF_PACK_JOIN_ROOM_RS;

//    char sql[512];
//    // ✅【修复】必须用转码后的 room_no
//    sprintf(sql,"select room_no from room where room_no=%llu", room_no);

//    // 🔥 打印最终SQL
//    printf("🔥 执行SQL: %s\n", sql);

//    list<string> lst;
//    int row_count = m_sql->SelectMysql(sql, 1, lst);

//    // 🔥 打印查询结果
//    printf("🔥 SQL查询结果行数: %d\n", row_count);

//    if(row_count > 0 && !lst.empty())
//    {
//        rs.result = 1;
//        rs.room_no = htole64(room_no);

//        if (lst.size() >= 2)
//            {
//                auto it = lst.begin();
//                ++it; // 移动到第二个元素
//                unsigned long long host_uid = stoull(*it);
//                rs.host_uid = host_uid;
//            }

//        sprintf(rs.rtmp_url, "rtmp://192.168.179.129:1935/videotest/user=%llu", room_no);

//        m_tcp->AddClientToRoom(room_no, clientfd);
//        g_userRoomMap[clientfd] = room_no;
//        SendOnlineCountToAnchor(room_no);
//        printf("✅【进房成功】房间：%llu\n", room_no);

//        BroadcastAdminList(room_no);
//        BroadcastOnlineUserList(room_no);
//        printf("用户进房后，主动推送房管列表 + 在线用户列表\n");

//    }
//    else
//    {
//        rs.result = 0;
//        printf("❌【进房失败】房间不存在：%llu\n", room_no);
//    }

//    SendData(clientfd, (char*)&rs, sizeof(rs));
//}
void CLogic::JoinRoomRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_JOIN_ROOM_RQ* rq = (STRU_JOIN_ROOM_RQ*)szbuf;
    cout << "STRU_JOIN_ROOM_RS size = " << sizeof(STRU_JOIN_ROOM_RS)<<endl;
    //进房前强制清理历史房间
    auto oldRoomIt = g_userRoomMap.find(clientfd);
    if (oldRoomIt != g_userRoomMap.end())
    {
        unsigned long long oldRoomNo = oldRoomIt->second;
        Block_Epoll_Net* pNet = TcpKernel::GetInstance()->GetTcpNet();
        if (pNet)
        {
            pNet->RemoveClientFromRoom(oldRoomNo, clientfd);
        }
        g_userRoomMap.erase(oldRoomIt);
        printf("[JoinRoomRq] 兜底清理旧房间 fd:%d oldRoom:%llu\n", clientfd, oldRoomNo);
    }

    unsigned long long room_no = le64toh(rq->room_no);
    unsigned long long userid = le64toh(rq->userid);

    printf("=====================================\n");
    printf("🔥 客户端发来的原始 room_no: %llu\n", rq->room_no);
    printf("🔥 转码后真实房间号        : %llu\n", room_no);
    printf("=====================================\n");

    STRU_JOIN_ROOM_RS rs;
    rs.type = _DEF_PACK_JOIN_ROOM_RS;

    // 🔥 关键：SQL必须同时查询 room_no 和 anchor_uid
    char sql[512];
    sprintf(sql,"select room_no, anchor_uid,live_status from room where room_no=%llu", room_no);
    printf("🔥 执行SQL: %s\n", sql);

    // 🔥 注意：第二个参数 2 表示查询2个字段，SelectMysql会按顺序存到list里
    list<string> lst;
    int row_count = m_sql->SelectMysql(sql, 3, lst);
    printf("🔥 SQL查询结果行数: %d\n", row_count);

    if(row_count > 0 && lst.size() >= 3)
    {
        // 🔥 关键：用迭代器取第二个字段 anchor_uid
        auto it = lst.begin();
        ++it; // 移动到第二个元素
        unsigned long long host_uid = stoull(*it);
        ++it;
        int live_status = atoi(it->c_str());
        if (live_status != 1)
        {
            rs.result = 0;
            printf("❌【进房拒绝】房间%llu 已下播/未开播，禁止进入\n", room_no);
            SendData(clientfd, (char*)&rs, sizeof(rs));
            return;
        }
        rs.result = 1;
        rs.room_no = htole64(room_no);
        rs.host_uid = host_uid;
        // 🔥 给 rtmp_url 赋值
        sprintf(rs.rtmp_url, "rtmp://192.168.179.129:1935/videotest/user=%llu", room_no);

        //****/m_tcp->AddClientToRoom(room_no, clientfd);
        Block_Epoll_Net* pNet = TcpKernel::GetInstance()->GetTcpNet();
        if(pNet)
        {
            pNet->AddClientToRoom(room_no, clientfd);
        }
        pthread_mutex_lock(&g_globalMapMutex);
        g_userRoomMap[clientfd] = room_no;
        pthread_mutex_unlock(&g_globalMapMutex);
        SendOnlineCountToAnchor(room_no);
        printf("✅【进房成功】房间：%llu 主播ID:%llu\n", room_no, host_uid);

        BroadcastAdminList(room_no);
        BroadcastOnlineUserList(room_no);
        printf("用户进房后，主动推送房管列表 + 在线用户列表\n");
        //sys notice
        const char* userName = GetNameByUid((int)userid);
        SendSysNotice(room_no, SYS_USER_ENTER, (int)userid, userName);
    }
    else
    {
        rs.result = 0;
        printf("❌【进房失败】房间不存在：%llu\n", room_no);
    }

    SendData(clientfd, (char*)&rs, sizeof(rs));
}
// ==============================================
// 弹幕
// ==============================================
void CLogic::SendDanmuRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_SEND_DANMU_RQ* rq = (STRU_SEND_DANMU_RQ*)szbuf;
    unsigned long long room_no = le64toh(rq->room_no);
    int userid = rq->userid;  // 你这里是int，不是unsigned long long
    // ========== 新增：禁言拦截逻辑 ==========
    auto roomIt = g_roomMap.find(room_no);
    if (roomIt != g_roomMap.end())
    {
        Room& stRoom = roomIt->second;
        // 1. 全体禁言 直接拦截
        if (stRoom.is_all_mute == 1)
        {
            printf("[SendDanmuRq] 全体禁言，拒绝发言 userid:%d\n", userid);
            return;
        }
        // 2. 判断是否在单人禁言列表
        bool bMuted = false;
        for (int i = 0; i < stRoom.mute_user_cnt; i++)
        {
            if (stRoom.mute_uid_list[i] == userid)
            {
                bMuted = true;
                break;
            }
        }
        if (bMuted)
        {
            printf("[SendDanmuRq] 用户已被禁言，拒绝发言 userid:%d\n", userid);
            return;
        }
    }

    // ======================
    //  根据userid查用户名
    // ======================
    char name[64] = "未知用户";
    char sql[256];
    sprintf(sql, "SELECT username FROM user WHERE id=%d", userid);

    list<string> res;
    if (m_sql->SelectMysql(sql, 1, res) && !res.empty()) {
        strncpy(name, res.front().c_str(), sizeof(name)-1);
    }

    // ======================
    // 拼接成：名字：内容
    // ======================
    char finalMsg[512];
    snprintf(finalMsg, sizeof(finalMsg), "%s：%s", name, rq->msg);

    // 广播
    STRU_DANMU_BROADCAST brd;
    brd.type = _DEF_PACK_DANMU_BROADCAST;
    brd.userid = htole64(userid);
    brd.room_no = htole64(room_no);
    strncpy(brd.msg, finalMsg, sizeof(brd.msg)-1);

    m_tcp->BroadcastRoomData(room_no, (char*)&brd, sizeof(brd));
    printf("【弹幕广播】房间%llu：%s\n", room_no, finalMsg);
}

// ==============================================
// 关闭直播
// ==============================================
void CLogic::StopLiveRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_STOP_LIVE_RQ* rq = (STRU_STOP_LIVE_RQ*)szbuf;
    STRU_STOP_LIVE_RS rs;
    rs.type = _DEF_PACK_STOP_LIVE_RS;

    unsigned long long roomNo = le64toh(rq->room_no);
    unsigned int hostUid = le64toh(rq->userid);

    printf("\n=========================================\n");
    printf("[StopLiveRq] 收到主播下播请求\n");
    printf("[StopLiveRq] 房间号(主机序)：%llu\n", roomNo);
    printf("[StopLiveRq] 主播UID(主机序)：%u\n", hostUid);
    printf("=========================================\n\n");

    char sql[512];
    sprintf(sql,"UPDATE room set live_status=0,end_time=NOW() where room_no=%llu and anchor_uid=%u",
            roomNo, hostUid);

    if(m_sql->UpdataMysql(sql))
    {
        rs.result = 1;
        printf("[StopLiveRq] ✅ 数据库下播状态更新成功！\n");

        // ==============================================
        // 【第一步：先广播下播包给所有观众！】🔥🔥🔥
        // ==============================================
        m_tcp->BroadcastRoomData(roomNo, (char*)&rs, sizeof(rs));
        printf("[StopLiveRq] ✅ 已全房间广播下播包给所有观众！\n");

        // ==============================================
        // 【第二步：再清空房间、踢人、删房间】
        // ==============================================
        m_tcp->ClearRoomClient(roomNo);
        printf("[StopLiveRq] ✅ 已清空房间所有在线观众！\n");

        m_pKernel->DelRoomInfo(roomNo);
        printf("[StopLiveRq] ✅ 房间信息已从内存删除！\n");
        //清理全局房间map
        pthread_mutex_lock(&g_globalMapMutex);
        g_roomMap.erase(roomNo);
        pthread_mutex_unlock(&g_globalMapMutex);

        // 通知所有客户端刷新大厅列表
        STRU_ROOM_LIST_UPDATE_NOTIFY notify;
        notify.update_type = 2; // 2=房间下播
        m_tcp->BroadcastToAllClients((char*)&notify, sizeof(notify));
        printf("【大厅通知】已推送房间下播通知\n");


        // ==============================================
        // 【第三步：最后给主播回包】
        // ==============================================
        SendData(clientfd, (char*)&rs, sizeof(rs));
        printf("[StopLiveRq] ✅ 单独下发下播回执给主播fd:%d\n",clientfd);

        printf("[StopLiveRq] 🎯 关闭直播成功！房间号：%llu\n", roomNo);
        int uid = GetUidByFd(clientfd);
        const char* userName = GetNameByUid(uid);
        SendSysNotice(roomNo, SYS_USER_LEAVE, uid, userName);

    }
    else
    {
        rs.result = 0;
        printf("[StopLiveRq] ❌ 数据库下播失败！SQL：%s\n", sql);
        SendData(clientfd, (char*)&rs, sizeof(rs));
    }
}

void CLogic::SendOnlineCountToAnchor(unsigned long long room_no)
{
    // 拿房间里的客户端列表
    auto& roomMap = m_tcp->m_RoomClientMap;
    auto it = roomMap.find(room_no);
    if (it == roomMap.end()) {
        return;
    }

    // 房间在线人数 = fd列表的大小
    int count = it->second.size();

    // 组包
    STRU_ROOM_COUNT_BROADCAST pkg;
    pkg.room_no = room_no;
    pkg.count = count;

    // 发给主播（全房间广播，主播会收到）
    m_tcp->BroadcastRoomData(room_no, (char*)&pkg, sizeof(pkg));
}
//禁言/解禁操作 请求处理
void CLogic::MuteCtrlRq(sock_fd clientfd, char *szbuf, int nlen)
{
    STRU_MUTE_CTRL_RQ* pRq = (STRU_MUTE_CTRL_RQ*)szbuf;

    // 字节序转换：房间号转主机序
    unsigned long long roomNo = le64toh(pRq->room_no);
    int opType = pRq->opType;
    int targetUid = pRq->targetUid;

    // 1. 查找对应房间
    auto& roomMap = m_tcp->m_RoomClientMap;
    auto roomIt = g_roomMap.find(roomNo);
    if (roomIt == g_roomMap.end())
    {
        printf("[MuteCtrlRq] 房间不存在 roomNo:%llu\n", roomNo);
        return;
    }
    Room& stRoom = roomIt->second;

    // 2. 根据操作类型修改房间禁言状态
    switch (opType)
    {
    case OP_MUTE_ALL:       // 全体禁言
        stRoom.is_all_mute = 1;
        printf("[MuteCtrlRq] 开启全体禁言 roomNo:%llu\n", roomNo);
        break;
    case OP_UNMUTE_ALL:     // 全体解禁
        stRoom.is_all_mute = 0;
        printf("[MuteCtrlRq] 解除全体禁言 roomNo:%llu\n", roomNo);
        break;
    case OP_MUTE_ONE:       // 单人禁言
    {
        // 数组未满且去重
        bool exist = false;
        for (int i = 0; i < stRoom.mute_user_cnt; i++)
        {
            if (stRoom.mute_uid_list[i] == targetUid)
            {
                exist = true;
                break;
            }
        }
        if (!exist && stRoom.mute_user_cnt < 64)
        {
            stRoom.mute_uid_list[stRoom.mute_user_cnt++] = targetUid;
            printf("[MuteCtrlRq] 禁言用户 uid:%d roomNo:%llu\n", targetUid, roomNo);
        }
        break;
    }
    case OP_UNMUTE_ONE:     // 单人解禁
    {
        for (int i = 0; i < stRoom.mute_user_cnt; i++)
        {
            if (stRoom.mute_uid_list[i] == targetUid)
            {
                // 末尾覆盖删除
                stRoom.mute_uid_list[i] = stRoom.mute_uid_list[--stRoom.mute_user_cnt];
                printf("[MuteCtrlRq] 解除用户禁言 uid:%d roomNo:%llu\n", targetUid, roomNo);
                break;
            }
        }
        break;
    }
    default:
        printf("[MuteCtrlRq] 未知操作类型:%d\n", opType);
        return;
    }

    // 3. 广播【房管操作原包】（客户端可展示房管操作弹幕）
    m_tcp->BroadcastRoomData(roomNo, szbuf, nlen);

    // 4. 组装【禁言状态同步包】，全房间广播最新状态
    STRU_MUTE_SYNC_BROAD stSync;
    stSync.room_no = roomNo;
    stSync.isAllMute = stRoom.is_all_mute;
    stSync.muteCount = stRoom.mute_user_cnt;
    memcpy(stSync.muteUid, stRoom.mute_uid_list, sizeof(stSync.muteUid));

    // 广播状态包
    m_tcp->BroadcastRoomData(roomNo, (char*)&stSync, sizeof(stSync));

}
// 增设/移除房管 请求处理
void CLogic::SetAdminRq(sock_fd clientfd, char *szbuf, int nlen)
{
    cout<<__func__<<endl;
    STRU_SET_ADMIN_RQ* pRq = (STRU_SET_ADMIN_RQ*)szbuf;

    // 字节序转换
    unsigned long long roomNo = le64toh(pRq->room_no);
    int opType = pRq->opType;
    int operUid = pRq->operUid;    // 操作人(主播/房管)
    int targetUid = pRq->targetUid;// 被操作人
    printf("[SetAdminRq] 收到请求 roomNo:%llu targetUid:%d operUid:%d\n",
           roomNo, pRq->targetUid, pRq->operUid);
    // 再打印当前所有房间号
    printf("[SetAdminRq] 当前所有房间号：\n");
    for (auto& pair : g_roomMap) {
        printf("  roomNo:%llu\n", pair.first);
    }
    // 查找房间
    auto roomIt = g_roomMap.find(roomNo);
    if (roomIt == g_roomMap.end())
    {
        printf("[SetAdminRq] 房间不存在 roomNo:%llu\n", roomNo);
        return;
    }
    Room& stRoom = roomIt->second;

    STRU_SET_ADMIN_RS rs;
    memset(&rs, 0, sizeof(rs));
    rs.type = _DEF_PACK_SET_ADMIN_RS;
    rs.result = 0;

    // ====================== 新增：权限校验 ======================
    bool hasPermission = false;
    // 主播默认拥有权限
    if (operUid == stRoom.host_userid)
    {
        hasPermission = true;
    }
    // 现有房管拥有权限
    for (int i = 0; i < stRoom.admin_cnt; ++i)
    {
        if (stRoom.admin_uid_list[i] == operUid)
        {
            hasPermission = true;
            break;
        }
    }
    if (!hasPermission)
    {
        printf("[SetAdminRq] 权限不足，操作人uid:%d 无法修改房管\n", operUid);
        SendData(clientfd, (char*)&rs, sizeof(rs));
        return;
    }

    bool bSuccess = false;
    // OP_ADD_ADMIN=1 添加房管  OP_DEL_ADMIN=2 移除房管
    if (opType == OP_ADD_ADMIN)
    {
        // 去重判断
        bool exist = false;
        for (int i = 0; i < stRoom.admin_cnt; ++i)
        {
            if (stRoom.admin_uid_list[i] == targetUid)
            {
                exist = true;
                break;
            }
        }
        if (exist)
        {
            printf("[SetAdminRq] 用户uid:%d 已是房管，无需重复添加\n", targetUid);
        }
        else if (stRoom.admin_cnt >= 32)
        {
            printf("[SetAdminRq] 房管数量已达上限(32人)，添加失败\n");
        }
        else
        {
            stRoom.admin_uid_list[stRoom.admin_cnt++] = targetUid;
            bSuccess = true;
            printf("[SetAdminRq] 成功添加房管 uid:%d room:%llu\n", targetUid, roomNo);
            // 发送【设为房管】系统公告
            const char* name = GetNameByUid(targetUid);
            SendSysNotice(roomNo, SYS_SET_ADMIN, targetUid, name);
        }
    }
    else if (opType == OP_DEL_ADMIN)
    {
        bool exist = false;
        int delIndex = -1;
        for (int i = 0; i < stRoom.admin_cnt; ++i)
        {
            if (stRoom.admin_uid_list[i] == targetUid)
            {
                exist = true;
                delIndex = i;
                break;
            }
        }
        if (!exist)
        {
            printf("[SetAdminRq] 用户uid:%d 不是房管，移除失败\n", targetUid);
        }
        else
        {
            // 末尾覆盖删除
            stRoom.admin_uid_list[delIndex] = stRoom.admin_uid_list[--stRoom.admin_cnt];
            bSuccess = true;
            printf("[SetAdminRq] 成功移除房管 uid:%d room:%llu\n", targetUid, roomNo);
            // 发送【移除房管】系统公告
            const char* name = GetNameByUid(targetUid);
            SendSysNotice(roomNo, SYS_DEL_ADMIN, targetUid, name);
        }
    }
    else
    {
        printf("[SetAdminRq] 未知操作类型 opType:%d\n", opType);
        SendData(clientfd, (char*)&rs, sizeof(rs));
        return;
    }

    rs.result = bSuccess ? 1 : 0;
    // 1. 给操作人单发回执
    SendData(clientfd, (char*)&rs, sizeof(rs));

    // 2. 房管列表变更，全房间广播最新房管列表
    BroadcastAdminList(roomNo);
}
// 广播房间房管列表
void CLogic::BroadcastAdminList(unsigned long long roomNo)
{
    auto roomIt = g_roomMap.find(roomNo);
    if (roomIt == g_roomMap.end()) return;
    Room& stRoom = roomIt->second;

    STRU_ADMIN_SYNC_BROAD brd;
    memset(&brd, 0, sizeof(brd));
    brd.type = _DEF_PACK_ADMIN_SYNC_BROAD;
    brd.room_no = roomNo;
    brd.adminCnt = stRoom.admin_cnt;
    memcpy(brd.adminUid, stRoom.admin_uid_list, sizeof(brd.adminUid));

    m_tcp->BroadcastRoomData(roomNo, (char*)&brd, sizeof(brd));
    printf("[BroadcastAdminList] 广播房管列表 数量:%d\n", brd.adminCnt);
}
// 广播房间在线用户(UID+昵称)列表
void CLogic::BroadcastOnlineUserList(unsigned long long roomNo)
{
    auto& roomClientMap = m_tcp->m_RoomClientMap;
    auto it = roomClientMap.find(roomNo);
    if (it == roomClientMap.end()) return;

    auto& fdList = it->second;
    STRU_ONLINE_USER_SYNC brd;
    memset(&brd, 0, sizeof(brd));
    brd.type = _DEF_PACK_ONLINE_USER_SYNC;
    brd.room_no = roomNo;

    int idx = 0;
    // 遍历房间所有在线fd
    for (sock_fd fd : fdList)
    {
        if (idx >= 64) break;

        // 1. 通过fd拿到用户UID（替换成你项目对应的接口/映射）
        int uid = GetUidByFd(fd);
        brd.userUid[idx] = uid;

        // 2. 通过UID查昵称（替换成你项目对应的接口/映射）
        const char* name = GetNameByUid(uid);
        if (name)
        {
            strncpy(brd.userName[idx], name, 31);
        }
        idx++;
    }
    brd.userCnt = idx;

    m_tcp->BroadcastRoomData(roomNo, (char*)&brd, sizeof(brd));
    printf("[BroadcastOnlineUserList] 广播在线用户 人数:%d\n", brd.userCnt);
}
int CLogic::GetUidByFd(sock_fd fd)
{
    auto it = g_fdToUidMap.find(fd);
    if (it != g_fdToUidMap.end())
    {
        return it->second;
    }
    return 0;
}
const char* CLogic::GetNameByUid(int uid)
{
    static char tmpName[32] = {0};
    memset(tmpName, 0, sizeof(tmpName));

    char sql[256];
    sprintf(sql, "SELECT username FROM user WHERE id=%d", uid);
    list<string> res;
    if (m_sql->SelectMysql(sql, 1, res) && !res.empty())
    {
        strncpy(tmpName, res.front().c_str(), 31);
    }
    else
    {
        strcpy(tmpName, "未知用户");
    }
    return tmpName;
}
void CLogic::SendSysNotice(unsigned long long roomNo, int noticeType, int uid, const char* name)
{
    STRU_SYS_NOTICE_BROAD pkg;
    memset(&pkg, 0, sizeof(pkg));

    pkg.type = _DEF_PACK_SYS_NOTICE_BROAD;
    pkg.room_no = htole64(roomNo);  // 统一转小端
    pkg.notice_type = noticeType;
    pkg.target_uid = uid;
    strncpy(pkg.target_name, name, 31);

    // 复用你现有房间广播接口
    m_tcp->BroadcastRoomData(roomNo, (char*)&pkg, sizeof(pkg));
    printf("[SendSysNotice] 系统通知 type:%d, uid:%d\n", noticeType, uid);
}
// 客户端主动退房请求
void CLogic::LeaveRoomRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_LEAVE_ROOM_RQ* pRq = (STRU_LEAVE_ROOM_RQ*)szbuf;
    unsigned long long room_no = le64toh(pRq->room_no);

    printf("[LeaveRoomRq] 收到客户端主动退房 fd:%d roomNo:%llu\n", clientfd, room_no);

    // 1. 先发送退房回执（在清理资源之前发送，确保 fd 还有效）
    STRU_LEAVE_ROOM_RS rs;
    memset(&rs, 0, sizeof(rs));
    rs.type = _DEF_PACK_LEAVE_ROOM_RS;
    rs.result = 1;
    SendData(clientfd, (char*)&rs, sizeof(rs));

    // 2. 获取 uid（在清理 fd->uid 映射之前获取）
    int uid = GetUidByFd(clientfd);
    const char* userName = GetNameByUid(uid);

    // 3. 调用现有接口：将fd从房间客户端列表移除
    Block_Epoll_Net* pNet = TcpKernel::GetInstance()->m_tcp;
    if (pNet)
    {
        pNet->RemoveClientFromRoom(room_no, clientfd);
    }

    // 4. 清理 fd -> 房间号 映射
    auto userRoomIt = g_userRoomMap.find(clientfd);
    if (userRoomIt != g_userRoomMap.end())
    {
        g_userRoomMap.erase(userRoomIt);
    }

    // 5. 发送用户下线系统公告
    SendSysNotice(room_no, SYS_USER_LEAVE, uid, userName);

    // 6. 广播在线列表、在线人数
    BroadcastOnlineUserList(room_no);
    SendOnlineCountToAnchor(room_no);

    // 7. 清理剩余资源（fd->uid 映射等）
    ClearClientResource(clientfd);

    printf("[LeaveRoomRq] ✅ 退房完成 fd:%d 已退出房间 %llu\n", clientfd, room_no);
}
void CLogic::OnHeartBeat(sock_fd clientfd, char* szbuf, int nlen)
{
    // 消除未使用参数警告
    (void)szbuf;
    (void)nlen;

    // 1. 把引用改成指针，接收 GetInstance() 的返回值
    Block_Epoll_Net* net = Block_Epoll_Net::GetInstance();
    if (!net)
    {
        return;
    }

    // 2. 加锁保护心跳容器
    pthread_mutex_lock(&net->m_beatMutex);

    // 3. 调用公共方法更新心跳时间（不再直接访问私有成员）
    auto it = net->m_fdCtxMap.find(clientfd);
    if (it != net->m_fdCtxMap.end())
    {
        it->second.lastBeatTime = time(NULL);
        // printf("fd=%d 收到心跳包\n", clientfd); // 测试用
    }

    pthread_mutex_unlock(&net->m_beatMutex);
}
// 统一清理单个客户端所有全局映射、房间关联、状态
void CLogic::ClearClientResource(sock_fd clientfd)
{
    pthread_mutex_lock(&g_globalMapMutex);

    // 1. 清理 fd -> UID 映射
    g_fdToUidMap.erase(clientfd);

    // 2. 清理 fd -> 房间号映射，并从对应房间移除用户
    auto userRoomIt = g_userRoomMap.find(clientfd);
    if (userRoomIt != g_userRoomMap.end())
    {
        unsigned long long roomNo = userRoomIt->second;
        g_userRoomMap.erase(userRoomIt);

        // 从房间客户端列表移除
        Block_Epoll_Net* pNet = TcpKernel::GetInstance()->GetTcpNet();
        if (pNet)
        {
            pNet->RemoveClientFromRoom(roomNo, clientfd);
        }

        // 广播在线列表、在线人数更新
        BroadcastOnlineUserList(roomNo);
        SendOnlineCountToAnchor(roomNo);

        // 可选：用户下线系统通知
        int uid = GetUidByFd(clientfd);
        const char* userName = GetNameByUid(uid);
        SendSysNotice(roomNo, SYS_USER_LEAVE, uid, userName);
    }

    pthread_mutex_unlock(&g_globalMapMutex);
    printf("[ClearClientResource] 已回收 fd:%d 所有资源\n", clientfd);
}
// ==============================================
// 获取房间列表 请求处理（适配当前Room结构体）
// ==============================================
//void CLogic::GetRoomListRq(sock_fd clientfd, char* szbuf, int nlen)
//{
//    STRU_GET_ROOM_LIST_RQ* pRq = (STRU_GET_ROOM_LIST_RQ*)szbuf;
//    STRU_GET_ROOM_LIST_RS rsp;
//    memset(&rsp, 0, sizeof(rsp));
//    rsp.type = _DEF_PACK_GET_ROOM_LIST_RS; // 协议号不转字节序

//    // 解析客户端请求参数
//    int reqPage     = pRq->page_index;
//    int pageSize    = pRq->page_size;
//    int sortType    = pRq->sort_type;
//    std::string searchKey(pRq->search_key);

//    printf("[GetRoomListRq] 请求房间列表 页码:%d 每页:%d 排序:%d 关键词:%s\n",
//           reqPage, pageSize, sortType, pRq->search_key);

//    // 全局容器加锁，保证线程安全
//    pthread_mutex_lock(&g_globalMapMutex);

//    // 临时容器：存储 房间号 + 房间信息 + 实时在线人数
//    using RoomInfo = std::tuple<unsigned long long, Room, int>;
//    std::vector<RoomInfo> tempList;

//    // 1. 遍历所有房间，执行关键词筛选
//    for (auto& pair : g_roomMap)
//    {
//        unsigned long long roomNo = pair.first;
//        Room& room = pair.second;
//        int onlineNum = (int)room.client_fds.size(); // 在线人数 = fd列表大小

//        // 关键词筛选：空关键词则全量展示
//        if (!searchKey.empty())
//        {
//            // 匹配房间号（字符串对比）
//            bool matchRoomNo = (std::to_string(roomNo) == searchKey);
//            // 匹配房间名（模糊匹配）
//            bool matchRoomName = (strstr(room.room_name, searchKey.c_str()) != nullptr);

//            if (!matchRoomNo && !matchRoomName)
//            {
//                continue;
//            }
//        }

//        tempList.emplace_back(roomNo, room, onlineNum);
//    }

//    // 2. 根据在线人数排序
//    if (sortType == 1)
//    {
//        // 1 = 在线人数升序
//        std::sort(tempList.begin(), tempList.end(),
//            [](const RoomInfo& a, const RoomInfo& b)
//            {
//                return std::get<2>(a) < std::get<2>(b);
//            });
//    }
//    else if (sortType == 2)
//    {
//        // 2 = 在线人数降序
//        std::sort(tempList.begin(), tempList.end(),
//            [](const RoomInfo& a, const RoomInfo& b)
//            {
//                return std::get<2>(a) > std::get<2>(b);
//            });
//    }
//    // sortType == 0 保持原始顺序，不排序

//    // 3. 分页计算 + 边界容错
//    int totalCount = (int)tempList.size();
//    int totalPage = (totalCount + pageSize - 1) / pageSize; // 向上取整

//    if (reqPage < 1) reqPage = 1;
//    if (reqPage > totalPage && totalPage > 0) reqPage = totalPage;

//    rsp.cur_page = reqPage;
//    rsp.total_page = totalPage;

//    // 4. 组装当前页返回数据
//    int startIndex = (reqPage - 1) * pageSize;
//    int endIndex = startIndex + pageSize;
//    if (endIndex > totalCount) endIndex = totalCount;

//    int itemIndex = 0;
//    for (int i = startIndex; i < endIndex && itemIndex < 64; ++i)
//    {
//        auto& info = tempList[i];
//        unsigned long long roomNo = std::get<0>(info);
//        Room& room = std::get<1>(info);
//        int onlineNum = std::get<2>(info);

//        // 填充返回结构体 + 大小端转换（沿用你项目规则）
//        rsp.items[itemIndex].room_no = htole64(roomNo);
//        strncpy(rsp.items[itemIndex].room_name, room.room_name, 63);
//        rsp.items[itemIndex].host_userid = htole64((unsigned long long)room.host_userid);
//        rsp.items[itemIndex].online_count = onlineNum;

//        itemIndex++;
//    }
//    rsp.item_cnt = itemIndex;

//    pthread_mutex_unlock(&g_globalMapMutex);

//    // 5. 发送响应包给客户端
//    SendData(clientfd, (char*)&rsp, sizeof(rsp));
//    printf("[GetRoomListRq] 房间列表返回完成 本页条数:%d 总页数:%d\n",
//           rsp.item_cnt, rsp.total_page);
//}
void CLogic::GetRoomListRq(sock_fd clientfd, char* szbuf, int nlen)
{
    STRU_GET_ROOM_LIST_RQ* pRq = (STRU_GET_ROOM_LIST_RQ*)szbuf;
    STRU_GET_ROOM_LIST_RS rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.type = _DEF_PACK_GET_ROOM_LIST_RS;

    int reqPage     = pRq->page_index;
    int pageSize    = pRq->page_size;
    int sortType    = pRq->sort_type;
    std::string searchKey(pRq->search_key);

    printf("[GetRoomListRq] 客户端请求 页码:%d 每页:%d 排序:%d 关键词:%s\n",
           reqPage, pageSize, sortType, pRq->search_key);

    pthread_mutex_lock(&g_globalMapMutex);
    using RoomInfo = std::tuple<unsigned long long, Room, int>;
    std::vector<RoomInfo> tempList;

    // 打印全局总房间数量
    printf("[GetRoomListRq] 内存全局g_roomMap总房间数量：%d\n", (int)g_roomMap.size());

    // 遍历所有房间
    for (auto& pair : g_roomMap)
    {
        unsigned long long roomNo = pair.first;
        Room& room = pair.second;
        // 从 m_RoomClientMap 获取真实在线人数（room.client_fds 未同步更新）
        int onlineNum = 0;
        auto it = m_pKernel->GetTcpNet()->m_RoomClientMap.find(roomNo);
        if (it != m_pKernel->GetTcpNet()->m_RoomClientMap.end()) {
            onlineNum = (int)it->second.size();
        }

        printf("[GetRoomListRq] 遍历房间 roomNo=%llu 房间名=%s 在线人数=%d\n",
               roomNo, room.room_name, onlineNum);

        // 关键词筛选
        if (!searchKey.empty())
        {
            bool matchRoomNo = (std::to_string(roomNo) == searchKey);
            bool matchRoomName = (strstr(room.room_name, searchKey.c_str()) != nullptr);
            if (!matchRoomNo && !matchRoomName)
            {
                printf("[GetRoomListRq] 房间%llu 不匹配搜索词，跳过\n", roomNo);
                continue;
            }
        }
        tempList.emplace_back(roomNo, room, onlineNum);
        printf("[GetRoomListRq] 房间%llu 加入列表缓存\n", roomNo);
    }

    printf("[GetRoomListRq] 筛选后有效房间总数：%d\n", (int)tempList.size());

    // 排序逻辑省略...

    int totalCount = (int)tempList.size();
    int totalPage = (totalCount + pageSize - 1) / pageSize;
    if (reqPage < 1) reqPage = 1;
    if (reqPage > totalPage && totalPage > 0) reqPage = totalPage;

    rsp.cur_page = reqPage;
    rsp.total_page = totalPage;

    int startIndex = (reqPage - 1) * pageSize;
    int endIndex = startIndex + pageSize;
    if (endIndex > totalCount) endIndex = totalCount;

    int itemIndex = 0;
    for (int i = startIndex; i < endIndex && itemIndex < 64; ++i)
    {
        auto& info = tempList[i];
        unsigned long long roomNo = std::get<0>(info);
        Room& room = std::get<1>(info);
        int onlineNum = std::get<2>(info);

        rsp.items[itemIndex].room_no     = htole64(roomNo);
        strncpy(rsp.items[itemIndex].room_name, room.room_name, 63);
        rsp.items[itemIndex].host_userid = htole64((unsigned long long)room.host_userid);
        rsp.items[itemIndex].online_count = onlineNum;

        // 重点打印：组装进返回包的每条房间原始数据、网络字节序
        printf("[GetRoomListRq] 回包第%d条 原始roomNo=%llu 网络序room_no=%llu 房间名=%s 在线人数=%d hostUid=%d\n",
               itemIndex, roomNo, rsp.items[itemIndex].room_no, room.room_name, onlineNum, room.host_userid);
        itemIndex++;
    }
    rsp.item_cnt = itemIndex;

    pthread_mutex_unlock(&g_globalMapMutex);

    printf("[GetRoomListRq] 最终回包：当前页条数=%d 总页数=%d\n", rsp.item_cnt, rsp.total_page);
    SendData(clientfd, (char*)&rsp, sizeof(rsp));
    printf("[GetRoomListRq] 房间列表数据包已发送给客户端fd=%d\n", clientfd);
}
