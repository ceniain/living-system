#ifndef CLOGIC_H
#define CLOGIC_H

#include "TCPKernel.h"

// 独立存储：房间号 → 房间名，隔离内存踩踏
//std::map<unsigned long long, std::string> g_roomNameMap;
// 全局锁：保护所有全局映射表
extern pthread_mutex_t g_globalMapMutex;
// 函数指针类型
typedef void (CLogic::*DealNetPack)(sock_fd clientfd, char* szbuf, int nlen);

class CLogic
{
public:
    CLogic( TcpKernel* pkernel )
    {
        m_pKernel = pkernel;
        m_sql = pkernel->m_sql;
        m_tcp = pkernel->m_tcp;
    }

public:
    void setNetPackMap();

    void SendData( sock_fd clientfd, char*szbuf, int nlen )
    {
        m_pKernel->SendData( clientfd ,szbuf , nlen );
    }

    // 注册
    void RegisterRq(sock_fd clientfd, char*szbuf, int nlen);
    // 登录
    void LoginRq(sock_fd clientfd, char*szbuf, int nlen);

    // 直播
    void CreateRoomRq(sock_fd clientfd, char* szbuf, int nlen);
    void JoinRoomRq(sock_fd clientfd, char* szbuf, int nlen);
    void SendDanmuRq(sock_fd clientfd, char* szbuf, int nlen);
    void StopLiveRq(sock_fd clientfd, char* szbuf, int nlen);
    void SendOnlineCountToAnchor(unsigned long long room_no);
    // 禁言操作请求处理
    void MuteCtrlRq(sock_fd clientfd,char* szbuf,int nlen);
    // 增设/移除房管 请求处理
    void SetAdminRq(sock_fd clientfd, char* szbuf, int nlen);

    // 封装：广播房间房管列表
    void BroadcastAdminList(unsigned long long roomNo);
    // 封装：广播房间在线用户(UID+昵称)列表
    void BroadcastOnlineUserList(unsigned long long roomNo);
    int GetUidByFd(sock_fd fd);
    const char* GetNameByUid(int uid);
    // 发送房间系统公告广播
    void SendSysNotice(unsigned long long roomNo, int noticeType, int uid, const char* name);
    void LeaveRoomRq(sock_fd clientfd,char* szbuf,int nlen);
    void OnHeartBeat(sock_fd clientfd, char* szbuf, int nlen);
     void ClearClientResource(sock_fd clientfd);//统一回收单个客户端所有资源（fd 断开时调用）
     //获取房间列表请求处理函数
     void GetRoomListRq(sock_fd clientfd, char* szbuf, int nlen);

private:
    TcpKernel* m_pKernel;
    CMysql * m_sql;
    Block_Epoll_Net * m_tcp;
};

#endif
