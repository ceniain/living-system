#include"../include/TCPKernel.h"

#include "./include/packdef.h"

#include<stdio.h>

#include<sys/time.h>

#include"./include/clogic.h"


extern std::map<unsigned long long, Room> g_roomMap;
using namespace std;





//设置网络协议映射

void TcpKernel::setNetPackMap()

{

    printf("setNetPackMap 1:进入函数\n");

    //清空映射

    bzero( m_NetPackMap , sizeof(m_NetPackMap) );

    printf("setNetPackMap 2:bzero完成\n");



    //协议映射赋值

    printf("setNetPackMap 3:准备调用m_logic->setNetPackMap()\n");

    m_logic->setNetPackMap();

    printf("setNetPackMap 4:m_logic->setNetPackMap执行完毕\n");



}





TcpKernel::TcpKernel()

{
    m_logic = nullptr;
    m_sql   = nullptr;
    m_tcp   = nullptr;
//    m_logic = new CLogic(this); // 创建并保存实例

}



TcpKernel::~TcpKernel()

{

    if( m_logic ) delete m_logic;

}



TcpKernel *TcpKernel::GetInstance()

{

    static TcpKernel kernel;

    return &kernel;

}



int TcpKernel::Open( int port)

{

    printf("【DEBUG1】进入Open，待启动端口:%d\n",port);

    initRand();

    printf("【DEBUG2】initRand完成\n");

    m_sql = new CMysql;

    // 数据库 使用127.0.0.1 地址  用户名root 密码colin123 数据库 wechat 没有的话需要创建 不然报错

    if(  !m_sql->ConnectMysql( _DEF_DB_IP , _DEF_DB_USER, _DEF_DB_PWD, _DEF_DB_NAME )  )

    {

        printf("Conncet Mysql Failed...\n");

        return FALSE;

    }

    else

    {

        printf("MySql Connect Success...\n");

        printf("【DEBUG3】数据库连接成功\n");

    }

    //初始网络

    m_tcp = new Block_Epoll_Net;

    printf("【DEBUG4】开始执行InitNet port=%d\n",port);

    bool res = m_tcp->InitNet( port , &TcpKernel::DealData ) ;

    printf("【DEBUG5】InitNet返回结果 res=%d\n",res);

    if( !res )

    {

        err_str( "net init fail:" ,-1);

        printf("【ERROR】InitNet初始化网络失败\n");

        return FALSE; //这里直接返回FALSE就是Open失败根源

    }

    printf("【DEBUG6】网络初始化成功\n");

    m_logic = new CLogic(this);

    printf("【DEBUG7】创建CLogic对象成功\n");

    setNetPackMap();



    printf("【DEBUG8】协议映射初始化完成，Open全部流程正常，返回TRUE\n");

    return TRUE;

}



void TcpKernel::Close()

{

    m_sql->DisConnect();



}



//随机数初始化

void TcpKernel::initRand()

{

    struct timeval time;

    gettimeofday( &time , NULL);

    srand( time.tv_sec + time.tv_usec );

}



void TcpKernel::DealData(sock_fd clientfd,char *szbuf,int nlen)

{

    printf("[DealData] 收到数据长度：%d\n", nlen);

    if(nlen <= 0)

    {

        printf("[DealData] 客户端断开连接\n");

        return;

    }

    if(nlen < sizeof(PackType))

    {

        printf("[DealData] 数据包太短丢弃\n");

        return;

    }



    // 1. 取出协议号

    PackType type = *(PackType*)szbuf;

    printf("[DealData] 协议号：%d\n", type);
    // ========== 心跳包处理 ==========
    printf("[DEBUG] 准备进入心跳包判断，当前协议号：%d\n", type);
    if (type == _DEF_PACK_HEART_BEAT)
    {
        printf("[DEBUG] 收到心跳包，fd=%d\n", clientfd);

        // 1. 获取 TcpKernel 单例
        TcpKernel* kernel = TcpKernel::GetInstance();
        // 2. 从单例中取出真正运行的网络对象
        Block_Epoll_Net* net = kernel->m_tcp;

        pthread_mutex_lock(&net->m_beatMutex);
        auto iter = net->m_fdCtxMap.find(clientfd);
        if (iter != net->m_fdCtxMap.end())
        {
            iter->second.lastBeatTime = time(NULL);
            printf("[心跳刷新] SUCCESS: fd=%d 更新时间戳成功！\n", clientfd);
        }
        else
        {
            printf("[心跳刷新] ERROR: fd=%d 不在 m_fdCtxMap 中！\n", clientfd);
        }
        pthread_mutex_unlock(&net->m_beatMutex);
        return;
    }


    // =================================



    // 2. 检查范围

    if( type >= _DEF_PACK_BASE && type < _DEF_PACK_BASE + _DEF_PACK_COUNT )

    {

        int idx = type - _DEF_PACK_BASE;



        extern DealNetPack g_netPackMap[];

        DealNetPack pf = g_netPackMap[idx];



        if(pf)

        {

            (GetInstance()->m_logic->*pf)(clientfd, szbuf, nlen);

        }

        else

        {

            printf("[DealData] 协议未绑定处理函数：%d\n", type);

        }

    }

    else

    {

        printf("[DealData] 无效协议号：%d\n", type);

    }

}





void TcpKernel::EventLoop()

{

    printf("event loop\n");

    m_tcp->EventLoop();

}



void TcpKernel::SendData(sock_fd clientfd, char *szbuf, int nlen)

{

    //    // ===================== 加调试 =====================
    //        printf("【SendData】要发送的数据长度 nlen = %d\n", nlen);
    //        //printf("【SendData】协议号 = %d\n", *(int*)szbuf);
    //        // ===================================================

    //        char tmp[4096] = {0};
    //        uint32_t len_le = htole32(nlen);
    //        memcpy(tmp, &len_le, 4);
    //        memcpy(tmp + 4, szbuf, nlen);

    //        // ===================== 加调试 =====================
    //        printf("【SendData】最终发出总长度 = %d\n", nlen + 4);
    //        // ===================================================

    //        m_tcp->SendData(clientfd, tmp, nlen + 4);
    m_tcp->SendData(clientfd, szbuf, nlen);

}
//alive
void TcpKernel::AddRoomInfo(unsigned long long roomNo, sock_fd fd)
{
    m_RoomAnchorMap[roomNo] = fd;
    Room stRoom;
    stRoom.room_no = roomNo;
    // 主播fd 对应主播ID，按需赋值
    stRoom.host_userid = m_logic->GetUidByFd(fd);
    stRoom.admin_cnt = 0;
    memset(stRoom.admin_uid_list, 0, sizeof(stRoom.admin_uid_list));
    pthread_mutex_lock(&g_globalMapMutex);
    g_roomMap[roomNo] = stRoom;
    pthread_mutex_unlock(&g_globalMapMutex);
    printf("[AddRoomInfo] 成功写入 g_roomMap，roomNo: %llu\n", roomNo);

}
//livealive
void TcpKernel::DelRoomInfo(unsigned long long roomNo)
{
    // 移除房间-主播映射
    m_RoomAnchorMap.erase(roomNo);
    // 移除全局房间表
    pthread_mutex_lock(&g_globalMapMutex);
    g_roomMap.erase(roomNo);
    pthread_mutex_unlock(&g_globalMapMutex);
    printf("[DelRoomInfo] 从容器移除房间 roomNo: %llu\n", roomNo);
}

void TcpKernel::OnClientDisconnect(sock_fd clientfd)
{
    pthread_mutex_lock(&g_globalMapMutex);
    // 1. 查找这个 fd 属于哪个房间
    auto it = g_userRoomMap.find(clientfd);
    if (it == g_userRoomMap.end()) {
        pthread_mutex_unlock(&g_globalMapMutex);
        return;
    }

    unsigned long long room_no = it->second;
    int uid = m_logic->GetUidByFd(clientfd);

    // 2. 从房间用户列表移除
    g_userRoomMap.erase(clientfd);
    pthread_mutex_unlock(&g_globalMapMutex);
    //发送【用户离开房间】系统公告
    const char* userName = m_logic->GetNameByUid(uid);
    m_logic->SendSysNotice(room_no, SYS_USER_LEAVE, uid, userName);

    // 3. 🔥 关键：通知逻辑层更新在线人数
    m_logic->SendOnlineCountToAnchor(room_no);
    //用户下线后，广播最新在线用户列表
    m_logic->BroadcastOnlineUserList(room_no);

    printf("【断开连接】fd=%d 退出房间，房间号：%llu\n", clientfd, room_no);

}
