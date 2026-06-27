#include"./include/block_epoll_net.h"
#include"./include/TCPKernel.h"
#include "./include/clogic.h"
bool Block_Epoll_Net::InitNet(int port, void (*recv_callback)(int, char *, int))
{
    m_recv_callback = recv_callback;
    InitThreadPool();

    int flags = 1;
    int ret = 0;
    m_listenEv = new myevent_s(this);

    //creat a tcp socket
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( m_listenfd  == -1 ){
        perror("create socket error");
        return false;
    }

    //set REUSERADDR
    ret = setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&flags, sizeof(flags));
    if ( ret == -1 ){
        perror("setsockopt error");
        return false;
    }

    //监听套接字m_listenfd 采用 LT 非阻塞模式
    //set NONBLOCK
    setNonBlockFd( m_listenfd );

    struct sockaddr_in local_addr;
    bzero( &local_addr , sizeof(sockaddr_in) );
    //set address
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    //bind addr
    ret = bind(m_listenfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in));
    if( ret == -1 ) {
        perror("bind error");
        close(m_listenfd);
        return false;
    }

    if (listen(m_listenfd, 128) == -1 ){
        perror("listen error");
        close(m_listenfd);
        return false;
    }

    //create epoll
    m_epoll_fd = epoll_create( MAX_EVENTS );

    m_listenEv->eventset(m_listenfd ,m_epoll_fd );
    //将监听套接字 添加到epoll中 , 模式LT 非阻塞
    m_listenEv->eventadd( EPOLLIN);

    return true;
}

bool Block_Epoll_Net::InitThreadPool()
{

    m_threadpool = new thread_pool;

    //创建拥有10个线程的线程池 最大线程数200 环形队列最大值50000
    if( (m_threadpool->Pool_create(200,10,50000)) == false )
    {
        perror("Create Thread_Pool Failed:");
        exit(-1);
    }

    return true;
}

void Block_Epoll_Net::EventLoop()
{
    printf("EventLoop:server running\n");
    int  i = 0;
    while (1) {

        /* 等待事件发生 */
        int nfd = epoll_wait( m_epoll_fd, events, MAX_EVENTS+1, 1000);
        if (nfd < 0) {
            printf("epoll_wait error, exit\n");
//            break;
            continue;
        }
        for (i = 0; i < nfd; i++) {
            struct myevent_s *ev = (struct myevent_s *)events[i].data.ptr;
            int fd = ev->fd;
            if ( (events[i].events & EPOLLIN) ) {
                if( fd == m_listenfd )
                    accept_event();
                else
                    recv_event( ev );
            }
            if ((events[i].events & EPOLLOUT) ) {
                epollout_event( ev );
            }
        }
        // ===================== 新增：执行心跳超时检测 =====================
        // epoll_wait 超时1s，等价 1秒轮询一次心跳
        CheckHeartBeatTimeout();
        // =================================================================
    }
}

void Block_Epoll_Net::accept_event()
{
    struct sockaddr_in caddr;
    socklen_t len = sizeof(caddr);
    int clientfd ;
    if ((clientfd = accept(m_listenfd, (struct sockaddr *)&caddr, &len)) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            /* 暂时不做出错处理 */
        }
        printf("%s: accept, %s\n", __func__, strerror(errno));
        return;
    }
//    //客户端接收使用阻塞模式 更简单 适合入门
//    //设置非阻塞
//    setNonBlockFd( clientfd );

    //设置接收缓冲区大小
    setRecvBufSize( clientfd );
    //设置发送缓冲区大小
    setSendBufSize( clientfd );
    //设置 无延迟
    setNoDelay( clientfd );

    myevent_s * clientEv = new myevent_s(this);
    clientEv->eventset( clientfd , m_epoll_fd );
    // 使用EPOLLONESHOT epoll监听不会重复检测 避免多线程并发 使得同一个套接字接收是排队的
    clientEv->eventadd(  EPOLLIN/*|EPOLLET*/|EPOLLONESHOT );

    //m_mapSockfdToEvent[clientfd] = clientEv;
    m_mapSockfdToEvent.insert(clientfd , clientEv);

    printf("new connect [%s:%d][time:%ld] \n",
           inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port), time(NULL) );
    // ===================== 新增：初始化心跳时间 =====================
        ClientContext ctx;
        ctx.fd = clientfd;
        ctx.lastBeatTime = time(NULL); // 刚连接，标记当前时间为最后心跳
        pthread_mutex_lock(&m_beatMutex);
        m_fdCtxMap[clientfd] = ctx;
        pthread_mutex_unlock(&m_beatMutex);

        // =================================================================

    return;
}




void Block_Epoll_Net::recv_event( myevent_s *ev)
{
    //接收事件采用线程池 多线程处理, 由于使用EPOLLONESHOT , 避免同一套机字的线程并发问题
     m_threadpool->Producer_add(  recv_task , (void*) ev );
}

//void* Block_Epoll_Net::recv_task(void* arg)
//{
//    myevent_s * ev = (myevent_s*)arg;
//    //利用全局指针 方便操作
//    Block_Epoll_Net * pthis = ev->pNet;

//    //接收和处理分离
//    int nRelReadNum = 0;
//    int nPackSize = 0;
//    char *pSzBuf = NULL;
//    do
//    {
//        nRelReadNum = read(ev->fd,&nPackSize,sizeof(nPackSize) );
//        if(nRelReadNum <= 0)
//            break;

//        pSzBuf = new char[nPackSize];
//        int nOffSet = 0;
//        nRelReadNum = 0;
//        //接收包的数据
//        while(nPackSize)
//        {
//            nRelReadNum = recv(ev->fd,pSzBuf+nOffSet,nPackSize,0);
//            if(nRelReadNum <= 0)
//                break;

//            nOffSet += nRelReadNum;
//            nPackSize -= nRelReadNum;
//        }
//        //接收和处理分离 跑线程池里其他线程处理 , 避免处理影响接收
//        DataBuffer * buffer = new DataBuffer(ev->pNet , ev->fd , pSzBuf , nOffSet );
//        pthis->m_threadpool->Producer_add(  Buffer_Deal , (void*) buffer );

//        // 这次接收完 要重新注册事件  此时 EPOLL MODE -> EPOLLIN|EPOLLONESHOT 没有修改, 使用重复值
//        ev->eventadd(  ev->events );

//        return 0;

//    }while(0);

//// 借助 do..while 在最后统一处理错误  避免使用goto语句
//    ev->eventdel();
//    close(ev->fd);
//    //**************
//    TcpKernel::GetInstance()->OnClientDisconnect(ev->fd);
//    //回收event结构
//    //pthis->m_mapSockfdToEvent.erase( ev->fd );
//    pthis->m_mapSockfdToEvent.erase( ev->fd );
//    delete ev;
//    return NULL;

//}
void* Block_Epoll_Net::recv_task(void* arg)
{
    myevent_s * ev = (myevent_s*)arg;
    Block_Epoll_Net * pthis = ev->pNet;

    int nRelReadNum = 0;
    int nPackSize = 0;
    char *pSzBuf = NULL;
    do
    {
        nRelReadNum = read(ev->fd,&nPackSize,sizeof(nPackSize) );
        if(nRelReadNum <= 0)
            break;

        pSzBuf = new char[nPackSize];
        int nOffSet = 0;
        nRelReadNum = 0;
        while(nPackSize)
        {
            nRelReadNum = recv(ev->fd,pSzBuf+nOffSet,nPackSize,0);
            if(nRelReadNum <= 0)
                break;

            nOffSet += nRelReadNum;
            nPackSize -= nRelReadNum;
        }
        DataBuffer * buffer = new DataBuffer(ev->pNet , ev->fd , pSzBuf , nOffSet );
        pthis->m_threadpool->Producer_add(  Buffer_Deal , (void*) buffer );

        ev->eventadd(  ev->events );

        return 0;

    }while(0);

    ev->eventdel();
    close(ev->fd);

    // ===== 【修正后代码开始】=====
    sock_fd clientfd = ev->fd;
    // ===== 新增：清理心跳上下文 =====
    printf("[recv_task] fd=%d 准备从 m_fdCtxMap 中删除\n", clientfd);
    pthread_mutex_lock(&pthis->m_beatMutex);
    pthis->m_fdCtxMap.erase(clientfd);
    pthread_mutex_unlock(&pthis->m_beatMutex);
    printf("[recv_task] fd=%d 已删除，当前容器size=%zu\n", clientfd, pthis->m_fdCtxMap.size());
    // ================================

    // 1. 直接使用全局变量 g_userRoomMap
    auto roomIter = g_userRoomMap.find(clientfd);
    if (roomIter != g_userRoomMap.end())
    {
        unsigned long long roomNo = roomIter->second;

        // 2. 把当前fd从房间列表移除
        pthis->RemoveClientFromRoom(roomNo, clientfd);

        // 3. 直接调用全局的 CLogic 实例来广播
        // 注意：如果你的 BroadcastOnlineUserList 是全局函数，可以直接调用
        //CLogic::GetInstance()->BroadcastOnlineUserList(roomNo);
    }
    // ===== 【修正后代码结束】=====

    TcpKernel::GetInstance()->OnClientDisconnect(ev->fd);
    pthis->m_mapSockfdToEvent.erase( ev->fd );
    delete ev;
    return NULL;
}


void * Block_Epoll_Net::Buffer_Deal( void * arg )
{
    DataBuffer * buffer = (DataBuffer *)arg;
    if( !buffer ) return NULL;

    buffer->pNet->m_recv_callback(buffer->sockfd,buffer->buf,buffer->nlen);

 //   printf("pszbuf = %p \n",buffer->buf);
    if(buffer->buf != NULL)
    {
        delete [] buffer->buf;
        buffer->buf = NULL;
    }
    delete buffer;
    return 0;
}

void Block_Epoll_Net::epollout_event( myevent_s *ev )
{
    // epoll LT模式 阻塞模式 发送阻塞 , 不用监听EPOLLOUT事件
}



int Block_Epoll_Net::SendData(int fd, char *szbuf, int nlen)
{
    // 发送不可分割 避免多线程并发
    // 先包大小, 再数据包 , 一次放入缓冲区

    /*
     +--------------+------------------+---------------------+
     |<-- 4bytes -->|<-- 4bytes协议头 ->|<--  协议其他内容    ->|
     +--------------+------------------+---------------------+
     |<- packsize ->|<------------ 数据包 struct ------------>|
     * */

    int nPackSize = nlen + 4;
    vector<char> vecbuf( nPackSize , 0);
    //vecbuf.resize( nPackSize );

    char* buf = &* vecbuf.begin();
    char* tmp = buf;
    *(int*)tmp = nlen;//按四个字节int写入
    tmp += sizeof(int );
    memcpy( tmp , szbuf , nlen );

    int res = send( fd,(const char *)buf, nPackSize ,0);

    return res;
}

void Block_Epoll_Net::setNonBlockFd(int fd)
{
    int flags = 0;
    flags = fcntl(fd, F_GETFL, 0);
    int ret = fcntl(fd, F_SETFL, flags|O_NONBLOCK);
    if( ret == -1)
        perror("setNonBlockFd fail:");
}

void Block_Epoll_Net::setRecvBufSize( int fd)
{
    //接收缓冲区
    int nRecvBuf = 256*1024;//设置为 256 K
    setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
}

void Block_Epoll_Net::setSendBufSize( int fd)
{
    //发送缓冲区
    int nSendBuf=128*1024;//设置为 128 K
    setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
}

#include<netinet/tcp.h>
void Block_Epoll_Net::setNoDelay( int fd)
{
    //nodelay
    int value = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY ,(char *)&value, sizeof(int));
}

// 1. 把观众加入房间
void Block_Epoll_Net::AddClientToRoom(unsigned long long roomNo, sock_fd fd)
{
    //pthread_mutex_lock(&g_globalMapMutex);
    m_RoomClientMap[roomNo].push_back(fd);
    //pthread_mutex_lock(&g_globalMapMutex);
}

// 2. 清空房间所有观众
//void Block_Epoll_Net::ClearRoomClient(unsigned long long roomNo)
//{
//    m_RoomClientMap.erase(roomNo); // 直接用数字，不要转string！
//}
// 2. 清空房间所有观众（主播下播使用）
void Block_Epoll_Net::ClearRoomClient(unsigned long long roomNo)
{
    auto it = m_RoomClientMap.find(roomNo);
    if (it == m_RoomClientMap.end())
        return;

    // 逐个关闭房间内客户端
    for (int fd : it->second)
    {
//        myevent_s* ev = nullptr;
//        // 使用 MyMap 的 find 接口查找元素
//        if (m_mapSockfdToEvent.find(fd, ev) && ev != nullptr)
//        {
//            ev->eventdel();
//            close(fd);
//            m_mapSockfdToEvent.erase(fd);
//            TcpKernel::GetInstance()->OnClientDisconnect(fd);
//            delete ev;
//        }
        printf("【移除房间成员】fd=%d 退出房间，房间号：%llu\n", fd, roomNo);

    }

    // 清空房间
    m_RoomClientMap.erase(roomNo);
}
// 3. 房间内广播消息（弹幕/状态）
void Block_Epoll_Net::BroadcastRoomData(unsigned long long roomNo, char* buf, int len)
{
    auto it = m_RoomClientMap.find(roomNo); // 直接用数字查找！
    if (it == m_RoomClientMap.end()) {
        return;
    }

    for (auto fd : it->second) {
        SendData(fd, buf, len);
    }
}
// 广播给所有在线客户端（用于大厅刷新通知）
void Block_Epoll_Net::BroadcastToAllClients(char* buf, int len)
{
    // 遍历所有连接的客户端
    for (auto& pair : m_fdCtxMap) {
        int fd = pair.first;
        SendData(fd, buf, len);
    }
}
// 单个客户端退出房间
void Block_Epoll_Net::RemoveClientFromRoom(unsigned long long roomNo, sock_fd fd)
{
    auto it = m_RoomClientMap.find(roomNo);
    if (it == m_RoomClientMap.end())
        return;

    // 遍历删除对应 fd
    auto& fdList = it->second;
    for (auto iter = fdList.begin(); iter != fdList.end(); )
    {
        if (*iter == fd)
        {
            iter = fdList.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}
// 心跳超时轮询检测
void Block_Epoll_Net::CheckHeartBeatTimeout()
{
    //printf("[轮询] 心跳检测执行中... m_fdCtxMap 数量: %zu\n", m_fdCtxMap.size());
    time_t now = time(NULL);
    // 遍历所有在线客户端fd
    pthread_mutex_lock(&m_beatMutex);
    auto it = m_fdCtxMap.begin();
    while (it != m_fdCtxMap.end())
    {
        int fd = it->first;
        ClientContext& ctx = it->second;
        time_t diff = now - ctx.lastBeatTime;
                // 新增：打印每个fd的间隔时间
                printf("[检测] fd=%d 距上次心跳: %lld 秒, 超时阈值: %d\n",
                       fd, (long long)diff, HEART_BEAT_TIMEOUT);

        // 超时判断
        if (now - ctx.lastBeatTime > HEART_BEAT_TIMEOUT)
        {
            printf("[心跳超时] fd=%d 无心跳，强制踢下线\n", fd);

            // 1. 先把该用户从所有房间移除
            for (auto& roomItem : m_RoomClientMap)
            {
                RemoveClientFromRoom(roomItem.first, fd);
            }

            // 2. 关闭fd、销毁epoll事件（标准踢人逻辑）
            myevent_s* ev = nullptr;
            if (m_mapSockfdToEvent.find(fd, ev) && ev != nullptr)
            {
                ev->eventdel();
                close(fd);
                m_mapSockfdToEvent.erase(fd);
                TcpKernel::GetInstance()->OnClientDisconnect(fd);
                delete ev;
            }

            // 3. 从心跳上下文容器删除
            it = m_fdCtxMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
    pthread_mutex_unlock(&m_beatMutex);
}
// 单次非阻塞事件处理
void Block_Epoll_Net::ProcessOnce()
{
    // epoll_wait 超时0 = 非阻塞
    int nfd = epoll_wait(m_epoll_fd, events, MAX_EVENTS+1, 0);
    if (nfd < 0)
        return;

    for (int i = 0; i < nfd; ++i)
    {
        myevent_s *ev = (myevent_s *)events[i].data.ptr;
        int fd = ev->fd;
        if (events[i].events & EPOLLIN)
        {
            if (fd == m_listenfd)
                accept_event();
            else
                recv_event(ev);
        }
        if (events[i].events & EPOLLOUT)
        {
            epollout_event(ev);
        }
    }
    // 每轮检测心跳超时
    CheckHeartBeatTimeout();
}
Block_Epoll_Net* Block_Epoll_Net::GetInstance()
{
    static Block_Epoll_Net instance;
    return &instance;
}
void Block_Epoll_Net::UpdateHeartBeatTime(int fd)
{
    pthread_mutex_lock(&m_beatMutex);
    auto iter = m_fdCtxMap.find(fd);
    if (iter != m_fdCtxMap.end())
    {
        iter->second.lastBeatTime = time(NULL);
        printf("[心跳刷新] fd=%d 更新心跳时间戳\n", fd);
    }
    pthread_mutex_unlock(&m_beatMutex);
}
