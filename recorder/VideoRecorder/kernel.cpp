#include "kernel.h"
#include <QDebug>
#include <QMessageBox>
#include <QtEndian>
#include"recorderdialog.h"
#include"roomhallwidget.h"
#include "adminmanagedialog.h"

Kernel* Kernel::m_pInstance = nullptr;

Kernel* Kernel::getInstance()
{
    qDebug() << "【Kernel】getInstance() 调用";
    if (!m_pInstance)
        m_pInstance = new Kernel;
    return m_pInstance;
}

Kernel::Kernel(QObject *parent)
    : QObject(parent),
    m_pRoomCreateDlg(nullptr),  // 初始化空指针
    m_pRoomHall(nullptr)    // 新增：初始化直播大厅指针

{
    qDebug() << "【Kernel】构造函数执行";

    m_pTcpClient = new TcpClient;
    m_pLoginDlg = new LoginDialog(nullptr, m_pTcpClient);

    qDebug() << "【Kernel】TCP客户端、登录窗口创建完成";

    initProtocolMap();
    qDebug() << "【Kernel】协议映射初始化完成";
    m_heartBeatTimer = new QTimer(this);
    m_heartBeatTimer->setInterval(3000); // 3秒一次
    connect(m_heartBeatTimer, &QTimer::timeout,
            this, &Kernel::onHeartBeatTimeout, Qt::UniqueConnection);
    connect(m_pTcpClient, &TcpClient::sigDisconnected,
            this, &Kernel::slotOnDisconnected, Qt::UniqueConnection);

}
Kernel::~Kernel()
{
    qDebug() << "【Kernel 析构】开始回收所有资源";

    // 1. 停止并释放心跳定时器
    if (m_heartBeatTimer)
    {
        m_heartBeatTimer->stop();
        m_heartBeatTimer->deleteLater();
        m_heartBeatTimer = nullptr;
        qDebug() << "【析构】心跳定时器已释放";
    }

    // 2. 释放 TCP 客户端（先断连接，再销毁）
    if (m_pTcpClient)
    {
        // 调用你 TcpClient 内部的断开接口
        m_pTcpClient->disconnectFromServer();
        m_pTcpClient->deleteLater();
        m_pTcpClient = nullptr;
        qDebug() << "【析构】TcpClient 已断开并释放";
    }

    // 3. 释放所有窗口（先关闭，再销毁）
    // 登录窗口
    if (m_pLoginDlg)
    {
        m_pLoginDlg->close();
        m_pLoginDlg->deleteLater();
        m_pLoginDlg = nullptr;
    }
    // 创建房间窗口
    if (m_pRoomCreateDlg)
    {
        m_pRoomCreateDlg->close();
        m_pRoomCreateDlg->deleteLater();
        m_pRoomCreateDlg = nullptr;
    }
    // 房管管理窗口
    if (m_adminDlg)
    {
        m_adminDlg->close();
        m_adminDlg->deleteLater();
        m_adminDlg = nullptr;
    }
    if (m_pRoomHall)
    {
        m_pRoomHall->close();
        m_pRoomHall->deleteLater();
        m_pRoomHall = nullptr;
    }


    // 4. 清空业务容器
    m_adminList.clear();
    m_muteUidList.clear();
    m_onlineUserMap.clear();

    qDebug() << "【Kernel 析构】所有资源回收完成";
}
void Kernel::initProtocolMap()
{
    qDebug() << "【Kernel】initProtocolMap 绑定协议处理函数";

    m_dealFunArr[_DEF_PACK_REGISTER_RS - _DEF_PACK_BASE] = &Kernel::dealRegisterRs;
    m_dealFunArr[_DEF_PACK_LOGIN_RS - _DEF_PACK_BASE] = &Kernel::dealLoginRs;
    m_dealFunArr[_DEF_PACK_CREATE_ROOM_RS - _DEF_PACK_BASE] = &Kernel::dealCreateRoomRs;
    // 以后加进房、弹幕、关播回复在这里绑定
    m_dealFunArr[_DEF_PACK_DANMU_BROADCAST - _DEF_PACK_BASE] = &Kernel::dealChatMsg;
    m_dealFunArr[_DEF_PACK_STOP_LIVE_RS - _DEF_PACK_BASE] = &Kernel::dealStopLiveRs;
    m_dealFunArr[_DEF_ROOM_COUNT - _DEF_PACK_BASE] = &Kernel::dealRoomCountBroadcast;
    // 禁言状态同步广播
    m_dealFunArr[_DEF_PACK_MUTE_SYNC_BROAD - _DEF_PACK_BASE] = &Kernel::dealMuteSyncBroad;
    // 房管列表同步
    m_dealFunArr[_DEF_PACK_ADMIN_SYNC_BROAD - _DEF_PACK_BASE] = &Kernel::dealAdminSyncBroad;
    // 在线用户同步
    m_dealFunArr[_DEF_PACK_ONLINE_USER_SYNC - _DEF_PACK_BASE] = &Kernel::dealOnlineUserSync;
    // 系统公告协议绑定
    m_dealFunArr[_DEF_PACK_SYS_NOTICE_BROAD-_DEF_PACK_BASE] = &Kernel::dealSysNoticeBroad;
    //房间列表响应
    m_dealFunArr[_DEF_PACK_GET_ROOM_LIST_RS - _DEF_PACK_BASE] = &Kernel::dealGetRoomListRs;

}

void Kernel::init()
{
    qDebug() << "【Kernel】init() 开始建立信号槽连接";

    connect(m_pTcpClient, &TcpClient::sigRecvData,
            this, &Kernel::slotRecvData);

    connect(m_pLoginDlg, &LoginDialog::sigLoginRequest,
            this, &Kernel::slotLoginReq);

    connect(m_pLoginDlg, &LoginDialog::sigRegisterRequest,
            this, &Kernel::slotRegisterReq);

    bool ret = m_pTcpClient->connectServer("192.168.179.129",8000);
    if(ret)
    {
        qDebug()<<"【Kernel】TCP连接服务器成功";
        if (!m_heartBeatTimer->isActive())
        {
            m_heartBeatTimer->start();
            qDebug() << "【DEBUG】连接成功，启动心跳定时器";
        }
    }
    else
    {
        qDebug()<<"【Kernel】TCP连接服务器失败！检查虚拟机IP/服务端是否启动";
    }

    qDebug() << "【Kernel】所有信号槽连接成功！显示登录窗口";
    m_pLoginDlg->show();
}

void Kernel::sendComment(const QString &content)
{
    STRU_SEND_DANMU_RQ req;
    req.userid = m_loginUserId;
    req.room_no = m_currentRoomNo;
    // 填充发送者昵称 m_loginName 自己定义的登录用户名成员变量
    strncpy(req.username, m_loginName.toUtf8().data(), sizeof(req.username)-1);
    strncpy(req.msg, content.toUtf8(), sizeof(req.msg)-1);

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
    //emit sigRecvComment(content);
}

void Kernel::sendGift(const QString &giftMsg)
{
    STRU_SEND_DANMU_RQ dan;
    dan.userid = m_loginUserId;
    dan.room_no = m_currentRoomNo; //补上房间号
    //礼物消息同样带上用户名
    strncpy(dan.username, m_loginName.toUtf8().data(), sizeof(dan.username)-1);
    strncpy(dan.msg, giftMsg.toUtf8().data(), sizeof(dan.msg)-1);
    m_pTcpClient->sendMsg((char*)&dan, sizeof(dan));
}
void Kernel::setCurrentRoom(unsigned long long roomNo)
{
    m_currentRoomNo = roomNo;
}


void Kernel::slotRecvData(char* buf, int len)
{
    qDebug() << "【原始收包】buf = " << QByteArray(buf, len).toHex(' ');
    qDebug() << "【原始收包】len = " << len;

    if (len <= 0) return;

    PackType type = qFromLittleEndian<quint32>((const uchar*)buf);
    qDebug() << "[Kernel] 收到包，协议号：" << type << "，长度：" << len;

    if(type >= _DEF_PACK_BASE)
    {
        int idx = type - _DEF_PACK_BASE;
        if(idx >=0 && idx < 100 && m_dealFunArr[idx])
        {
            (this->*m_dealFunArr[idx])(buf, len);
        }
    }
}

// ===================== 登录发包 =====================
void Kernel::slotLoginReq(QString user, QString pwd)
{
    STRU_LOGIN_RQ req;
    strcpy(req.name, user.toUtf8().data());
    strcpy(req.password, pwd.toUtf8().data());
    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 注册发包 =====================
void Kernel::slotRegisterReq(QString user,QString tel, QString pwd1, QString pwd2)
{
    if (pwd1 != pwd2) return;

    STRU_REGISTER_RQ req;
    strcpy(req.name,user.toUtf8().data());
    strcpy(req.tel, tel.toUtf8().data());
    strcpy(req.password, pwd1.toUtf8().data());
    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 创建房间（正常不动） =====================
void Kernel::slotCreateRoom(QString roomName)
{
    STRU_CREATE_ROOM_RQ req;
    req.userid = m_loginUserId;
    strcpy(req.room_name, roomName.toUtf8().data());
    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 创建房间回复 =====================
//void Kernel::dealCreateRoomRs(char *buf, int len)
//{
//    STRU_CREATE_ROOM_RS* rsp = (STRU_CREATE_ROOM_RS*)buf;

//    if (rsp->result == create_room_success) {
//        m_currentRoomNo = rsp->room_no;
//        m_hostUserId = m_loginUserId; // 保存自己是主播
//        QString roomNo = QString::number(rsp->room_no);
//        QMessageBox::information(m_pRoomCreateDlg, "成功",
//                                 QString("房间创建成功！\n直播间编号：%1").arg(roomNo));

//        m_pRoomCreateDlg->close();
//        RecorderDialog *w = new RecorderDialog;
//        w->setRoomNumber(roomNo);
//        w->show();
//    } else {
//        QMessageBox::warning(m_pRoomCreateDlg, "失败", "创建失败！");
//    }
//}
void Kernel::dealCreateRoomRs(char *buf, int len)
{
    STRU_CREATE_ROOM_RS* rsp = (STRU_CREATE_ROOM_RS*)buf;

    if (rsp->result == create_room_success) {
        m_currentRoomNo = rsp->room_no;
        m_hostUserId = m_loginUserId;
        QString roomNo = QString::number(rsp->room_no);
        QMessageBox::information(m_pRoomCreateDlg, "成功",
                                 QString("房间创建成功！\n直播间编号：%1").arg(roomNo));

        m_pRoomCreateDlg->close();

        // ✅【修复】只创建一次窗口，永远只有一个实例！
        static RecorderDialog *w = nullptr;
        if (!w) {
            w = new RecorderDialog;
        }
        w->setRoomNumber(roomNo);
        w->show();
    }
}

// ===================== 注册回复 =====================
void Kernel::dealRegisterRs(char* buf, int len)
{
    STRU_REGISTER_RS* rsp = (STRU_REGISTER_RS*)buf;
    if (rsp->result == register_success)
        QMessageBox::information(m_pLoginDlg, "提示", "注册成功！");
    else
        QMessageBox::warning(m_pLoginDlg, "提示", "账号已存在！");
}

// ===================== 登录回复 =====================
void Kernel::dealLoginRs(char* buf, int len)
{
    STRU_LOGIN_RS* rsp = (STRU_LOGIN_RS*)buf;

    if (rsp->result == login_success) {
        m_loginUserId = rsp->userid;
        QMessageBox::information(m_pLoginDlg, "成功", "登录成功");
        m_pLoginDlg->close();

//        // ========== 修复：在这里创建对话框，避免空指针 ==========
        if(!m_pRoomCreateDlg)
        {
            m_pRoomCreateDlg = new RoomCreateDialog;
            // 在这里连接！绝对安全
            connect(m_pRoomCreateDlg, &RoomCreateDialog::sigCreateRoom,
                    this, &Kernel::slotCreateRoom);
        }
        m_pRoomCreateDlg->show();
//        if (!m_pRoomHall)
//        {
//            m_pRoomHall = new RoomHallWidget(nullptr);
//        }
//        m_pRoomHall->show();
//        m_pRoomHall->raise();

        emit sigLoginSuccess();

    } else {
        QMessageBox::warning(m_pLoginDlg, "失败", "登录失败");
    }
}

// 处理聊天/弹幕/礼物
void Kernel::dealChatMsg(char* buf, int len)
{
    if(len < sizeof(STRU_DANMU_BROADCAST)) return;
    STRU_DANMU_BROADCAST* pDan = (STRU_DANMU_BROADCAST*)buf;

    QString showText = QString::fromUtf8(pDan->msg).trimmed();
    int sendUserId = pDan->userid;

    qDebug() << "[Kernel] 收到弹幕/礼物：" << showText;

    // ✅ 直接发！
    emit sigRecvComment(showText, sendUserId);
    qDebug()<<"Kernel::dealChatMsg sigRecvComment";

    // 礼物逻辑
    if (showText.contains("火箭") ||
        showText.contains("玫瑰") ||
        showText.contains("跑车") ||
        showText.contains("棒棒糖") ||
        showText.contains("啤酒") ||
        showText.contains("赠送"))
    {
        emit sigRecvGift(showText);
        qDebug()<<"Kernel::dealChatMsg sigRecvGift";
    }
}

void Kernel::dealStopLiveRs(char *buf, int len)
{
    qDebug() << "Kernel::dealStopLiveRs收到下播应答";    

    static bool is_closed = false;
    if (is_closed) return;

    STRU_STOP_LIVE_RS* rsp = (STRU_STOP_LIVE_RS*)buf;

    if (rsp->result == 1) {
        // ===================== 加调试输出 =====================
        qDebug() << "Kernel: 准备发射 sigRoomClosed 信号";
        // ======================================================
        if (m_heartBeatTimer->isActive())
        {
            m_heartBeatTimer->stop();
            qDebug() << "【主播】下播成功，兜底停止心跳";
        }
        is_closed = true;
//        emit sigRoomClosed();
//        m_currentRoomNo = 0;
//        qDebug() << "Kernel::dealStopLiveRs 下播成功，房间已关闭";
        QTimer::singleShot(1000, this, [=]() {

            // 弹窗前保证观众已经处理完
            //QMessageBox::information(nullptr, "下播成功", "✅ 直播已结束！");

            // 清空房间号
            m_currentRoomNo = 0;
        });

        // 立刻发射信号给观众（不等待）
        emit sigRoomClosed();
    }
}
// 主播点击下播按钮 → 发送关播请求
void Kernel::sendStopLiveRequest()
{
    if (m_currentRoomNo == 0 || m_loginUserId == 0)
        return;

    if (m_heartBeatTimer->isActive())
    {
        m_heartBeatTimer->stop();
        qDebug() << "【主播】主动下播，停止心跳";
    }

    STRU_STOP_LIVE_RQ req;
    req.type = _DEF_PACK_STOP_LIVE_RQ;
    req.userid = m_loginUserId;
    req.room_no = m_currentRoomNo;

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
    qDebug() << "主播已发送下播请求";

}
// ===================== 接收房间在线人数 =====================
void Kernel::dealRoomCountBroadcast(char* buf, int len)
{
    qDebug() << __func__;

    if (len < sizeof(STRU_ROOM_COUNT_BROADCAST)) {
        return;
    }

    STRU_ROOM_COUNT_BROADCAST* pkg = (STRU_ROOM_COUNT_BROADCAST*)buf;
    int count = pkg->count;

    qDebug() << " 收到房间在线人数：" << count;

    // 发给主播界面
    emit sigRoomCountUpdate(count);
}

void Kernel::dealAdminSyncBroad(char *buf, int len)
{
    if (len < sizeof(STRU_ADMIN_SYNC_BROAD))
    {
        qDebug() << "[Kernel] 房管广播包长度异常";
        return;
    }

    STRU_ADMIN_SYNC_BROAD* pBroad = (STRU_ADMIN_SYNC_BROAD*)buf;
    m_adminList.clear();

    for (int i = 0; i < pBroad->adminCnt; ++i)
    {
        m_adminList.append(pBroad->adminUid[i]);
    }

    qDebug() << "[Kernel] 同步房管列表，当前房管数：" << m_adminList.size();
    emit sigAdminSync();
}

void Kernel::dealOnlineUserSync(char *buf, int len)
{
    if (len < sizeof(STRU_ONLINE_USER_SYNC))
    {
        qDebug() << "[Kernel] 在线用户包长度异常";
        return;
    }

    STRU_ONLINE_USER_SYNC* pSync = (STRU_ONLINE_USER_SYNC*)buf;
    m_onlineUserMap.clear();

    for (int i = 0; i < pSync->userCnt; ++i)
    {
        int uid = pSync->userUid[i];
        QString name = QString::fromUtf8(pSync->userName[i]);
        m_onlineUserMap.insert(uid, name);
    }

    qDebug() << "[Kernel] 同步在线用户列表，在线人数：" << m_onlineUserMap.size();
    emit sigOnlineUserSync();
}
// 禁言状态同步广播解析
void Kernel::dealMuteSyncBroad(char *buf, int len)
{
    if (len < sizeof(STRU_MUTE_SYNC_BROAD))
    {
        qDebug() << "[Kernel] 禁言同步包长度异常";
        return;
    }

    STRU_MUTE_SYNC_BROAD* pMute = (STRU_MUTE_SYNC_BROAD*)buf;
    // 使用结构体定义里的正确字段名
    m_isAllMute = pMute->isAllMute;
    m_muteUidList.clear();

    // 循环条件使用正确的 muteCount
    for (int i = 0; i < pMute->muteCount; ++i)
    {
        // 使用正确的数组名 muteUid
        m_muteUidList.append(pMute->muteUid[i]);
    }

    qDebug() << "[Kernel] 同步禁言状态：全体禁言=" << m_isAllMute
             << " 单人禁言数=" << m_muteUidList.size();

    // 通知UI刷新禁言按钮/状态
    emit sigMuteStateUpdate();
}
void Kernel::slot_open_admin_dialog()
{
    // 1. 权限校验：仅主播 / 房管 可打开
    int uid = m_loginUserId;
    bool isHost = (uid == m_hostUserId);
    bool isAdmin = m_adminList.contains(uid);

    if (!isHost && !isAdmin)
    {
        qDebug() << "无权打开房管管理窗口";
        return;
    }

    // 2. 单例窗口，防止重复创建
    if (!m_adminDlg)
    {
        m_adminDlg = new AdminManageDialog(nullptr);
    }

    // 3. 显示并置顶
    m_adminDlg->show();
    m_adminDlg->raise();
}
// 获取房管列表
QList<int> Kernel::getAdminList() const
{
    return m_adminList;
}

// 判断指定UID是否为房管
bool Kernel::isAdmin(int uid) const
{
    return m_adminList.contains(uid);
}

// 获取禁言用户列表
QList<int> Kernel::getMuteUidList() const
{
    return m_muteUidList;
}

// 判断当前登录用户是否被禁言
bool Kernel::isSelfBanned() const
{
    return m_muteUidList.contains(m_loginUserId);
}
void Kernel::dealSysNoticeBroad(char* buf, int len)
{
    qDebug() << __func__;
    STRU_SYS_NOTICE_BROAD* pPkg = (STRU_SYS_NOTICE_BROAD*)buf;
    if (!pPkg) return;

    unsigned long long roomNo = qFromLittleEndian(pPkg->room_no);
    int type = pPkg->notice_type;
    int uid = pPkg->target_uid;
    QString name = QString::fromUtf8(pPkg->target_name);

    QString tip;
    switch (type)
    {
    case SYS_USER_ENTER:
        tip = QString("【系统】%1 进入直播间").arg(name);
        break;
    case SYS_USER_LEAVE:
        tip = QString("【系统】%1 离开直播间").arg(name);
        break;
    case SYS_SET_ADMIN:
        tip = QString("【系统】%1 被设置为房管").arg(name);
        break;
    case SYS_DEL_ADMIN:
        tip = QString("【系统】%1 被移除房管").arg(name);
        break;
    default:
        return;
    }
    // 复用原有弹幕信号，推送到UI
    emit sigRecvComment(tip, 0);
}
// 定时器超时回调：触发发送心跳
void Kernel::onHeartBeatTimeout()
{
    qDebug() << "【主播】onHeartBeatTimeout 触发，准备发心跳";
    sendHeartBeat();
}

// 组装并发送心跳包
void Kernel::sendHeartBeat()
{
    qDebug()<<__func__;
    qDebug() << "【DEBUG】m_pTcpClient 地址：" << m_pTcpClient;
    if (!m_pTcpClient) {
        qDebug() << "【DEBUG】m_pTcpClient为空" ;
        return;
    }
    //    // 连接失效直接放弃
    //    if (!m_pTcpClient || !m_pTcpClient->isConnected())
    //    {
    //        qDebug() << "【DEBUG】心跳包被拦截：连接无效！";
    //        return;
    //    }

    STRU_HEART_BEAT beat;
    beat.type = _DEF_PACK_HEART_BEAT;
    qDebug() << "【DEBUG】准备发送心跳包，协议号：" << beat.type;
    // 发送心跳包
    m_pTcpClient->sendMsg((char*)&beat, sizeof(STRU_HEART_BEAT));
    qDebug() << "发送心跳包"; // 测试可打开，上线注释减少日志
}
void Kernel::slotOnDisconnected()
{
    qDebug() << "【主播】TCP连接已断开";
    // 停止心跳定时器
    if (m_heartBeatTimer->isActive())
    {
        m_heartBeatTimer->stop();
        qDebug() << "【主播】心跳定时器已停止";
    }
    // 可选：清空房间、登录状态，弹窗提示
    m_currentRoomNo = 0;
    m_loginUserId = 0;
    QMessageBox::warning(nullptr, "连接断开", "与服务器连接中断，请重新登录！");
    if(m_pLoginDlg)
    {
        m_pLoginDlg->show();
    }
}
//发送获取房间列表请求
void Kernel::SendRoomListReq()
{
    qDebug()<<__func__;
    // 结构体自带构造函数，自动初始化协议号、清零成员
    STRU_GET_ROOM_LIST_RQ req;
    // 默认参数（可由UI层传参，这里Kernel仅做转发）
    req.page_index  = 1;
    req.page_size   = 10;
    req.sort_type   = 0;
    memset(req.search_key, 0, sizeof(req.search_key));

    if(m_pTcpClient)
    {
        m_pTcpClient->sendMsg(reinterpret_cast<char*>(&req), sizeof(req));
        qDebug() << "[Kernel] 已发送 获取房间列表 请求包";
    }
}

//解析 房间列表响应包
void Kernel::dealGetRoomListRs(char *buf, int len)
{
    qDebug()<<__func__;
    if(nullptr == buf || len < sizeof(STRU_GET_ROOM_LIST_RS))
    {
        qDebug() << "[Kernel] 房间列表包长度异常";
        return;
    }
    STRU_GET_ROOM_LIST_RS* rsp = (STRU_GET_ROOM_LIST_RS*)buf;

    // 使用服务端返回的真实数据
    qDebug() << "[Kernel] 收到房间列表: cur_page=" << rsp->cur_page
             << "total_page=" << rsp->total_page << "item_cnt=" << rsp->item_cnt;

    for (int i = 0; i < rsp->item_cnt; ++i) {
        qDebug() << "  房间" << i << ": room_no=" << rsp->items[i].room_no
                 << "name=" << rsp->items[i].room_name
                 << "online=" << rsp->items[i].online_count;
    }

    emit sig_RoomListResp(*rsp);
    qDebug() << "[Kernel] 解析房间列表完成，转发信号至UI";
}
// 大厅点击创建房间按钮 → 打开创建房间弹窗
void Kernel::slot_OpenCreateRoomDialog()
{
    // 单例弹窗，防止重复创建
    if (!m_pRoomCreateDlg)
    {
        m_pRoomCreateDlg = new RoomCreateDialog(nullptr);
        // 绑定弹窗提交信号 → 转发创建房间请求
        connect(m_pRoomCreateDlg, &RoomCreateDialog::sigCreateRoom,
                this, &Kernel::slotCreateRoom, Qt::UniqueConnection);
        // 关闭窗口不销毁对象，仅隐藏
        m_pRoomCreateDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    }

    m_pRoomCreateDlg->show();
    m_pRoomCreateDlg->raise();
    m_pRoomCreateDlg->activateWindow();
}
