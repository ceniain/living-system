#include "kernel.h"
#include <QDebug>
#include <QMessageBox>
#include <QtEndian>
#include"logindialog.h"
#include "adminmanagedialog.h"


Kernel* Kernel::m_pInstance = nullptr;

Kernel* Kernel::getInstance()
{
    if (!m_pInstance)
        m_pInstance = new Kernel;
    return m_pInstance;
}

Kernel::Kernel(QObject *parent)
    : QObject(parent)
{
    qDebug() << "【观众Kernel】构造";

    m_pTcpClient = new TcpClient(this);
    m_pLoginDlg = new LoginDialog(nullptr);
    m_pLoginDlg->setTcpClient(m_pTcpClient);

    //监听TCP断开信号
    connect(m_pTcpClient, &TcpClient::sigDisconnected,
            this, &Kernel::slotOnDisconnected, Qt::UniqueConnection);

    m_loginUserId = 0;
    m_hostUserId  = 0;
    m_currentRoomNo = 0;
    // 清空列表
    m_adminList.clear();
    m_onlineUserList.clear();
    m_banUserList.clear();


    initProtocolMap();
    // 初始化心跳定时器：3000ms = 3秒
    m_heartBeatTimer = new QTimer(this);
    m_heartBeatTimer->setInterval(3000);
    bool ok=connect(m_heartBeatTimer, &QTimer::timeout, this, &Kernel::onHeartBeatTimeout);
    qDebug() << "【DEBUG】定时器信号槽绑定结果 ok = " << ok;


}
Kernel::~Kernel()
{
    // 停止心跳定时器
    if (m_heartBeatTimer)
    {
        m_heartBeatTimer->stop();
        m_heartBeatTimer->deleteLater();
        m_heartBeatTimer = nullptr;
    }

    // 释放TCP客户端
    if (m_pTcpClient)
    {
        m_pTcpClient->disconnectFromServer();
        m_pTcpClient->deleteLater();
        m_pTcpClient = nullptr;
    }

    // 关闭并释放所有窗口
    if (m_pLoginDlg)
    {
        m_pLoginDlg->close();
        m_pLoginDlg->deleteLater();
        m_pLoginDlg = nullptr;
    }
    if (m_pPlayerDlg)
    {
        m_pPlayerDlg->close();
        m_pPlayerDlg->deleteLater();
        m_pPlayerDlg = nullptr;
    }
    if (m_adminDlg)
    {
        m_adminDlg->close();
        m_adminDlg->deleteLater();
        m_adminDlg = nullptr;
    }

    // 清空容器
    m_adminList.clear();
    m_onlineUserList.clear();
    m_banUserList.clear();

    qDebug() << "【观众Kernel】析构完成，资源全部释放";
}
void Kernel::initProtocolMap()
{
    qDebug()<<__func__;
    m_dealFunArr[_DEF_PACK_REGISTER_RS - _DEF_PACK_BASE] = &Kernel::dealRegisterRs;
    m_dealFunArr[_DEF_PACK_LOGIN_RS - _DEF_PACK_BASE] = &Kernel::dealLoginRs;
    m_dealFunArr[_DEF_PACK_JOIN_ROOM_RS - _DEF_PACK_BASE] = &Kernel::dealJoinRoomRs;
    m_dealFunArr[_DEF_PACK_DANMU_BROADCAST - _DEF_PACK_BASE] = &Kernel::dealDanmuBroadcast;
    m_dealFunArr[_DEF_PACK_STOP_LIVE_RS - _DEF_PACK_BASE] = &Kernel::dealStopLiveRs;
    // 禁言状态广播
    m_dealFunArr[_DEF_PACK_MUTE_SYNC_BROAD-_DEF_PACK_BASE]   = &Kernel::dealBanBroadcast;
    // 房管列表广播
    m_dealFunArr[_DEF_PACK_ADMIN_SYNC_BROAD-_DEF_PACK_BASE]  = &Kernel::dealAdminBroadcast;
    // 在线用户列表广播
    m_dealFunArr[_DEF_PACK_ONLINE_USER_SYNC-_DEF_PACK_BASE]  = &Kernel::dealUserBroadcast;
    //房管设置回执包解析
    m_dealFunArr[_DEF_PACK_SET_ADMIN_RS-_DEF_PACK_BASE] = &Kernel::dealSetAdminRs;
    // 系统公告协议绑定
    m_dealFunArr[_DEF_PACK_SYS_NOTICE_BROAD-_DEF_PACK_BASE] = &Kernel::dealSysNoticeBroad;
    m_dealFunArr[_DEF_PACK_LEAVE_ROOM_RS - _DEF_PACK_BASE] = &Kernel::dealLeaveRoomRs;
    // 房间列表响应
    m_dealFunArr[_DEF_PACK_GET_ROOM_LIST_RS - _DEF_PACK_BASE] = &Kernel::dealRoomListRs;
    // 房间列表变更通知
    m_dealFunArr[_DEF_PACK_ROOM_LIST_UPDATE_NOTIFY - _DEF_PACK_BASE] = &Kernel::dealRoomListUpdateNotify;

}

void Kernel::init()
{
    qDebug()<<__func__;

    connect(m_pTcpClient, &TcpClient::sigRecvData,
            this, &Kernel::slotRecvData, Qt::UniqueConnection);

    connect(m_pLoginDlg, &LoginDialog::sigLoginRequest, this, &Kernel::slotLoginReq);
    connect(m_pLoginDlg, &LoginDialog::sigRegisterRequest, this, &Kernel::slotRegisterReq);

    bool ret = m_pTcpClient->connectServer("192.168.179.129", 8000);
    if (ret) {
        qDebug() << "【观众Kernel】TCP连接成功";
        qDebug() << "【DEBUG】连接成功后，isConnected() = " << m_pTcpClient->isConnected();
        if (!m_heartBeatTimer->isActive())
        {
            m_heartBeatTimer->start();
            qDebug() << "【DEBUG】连接成功，启动心跳定时器";
        }
    } else {
        qDebug() << "【观众Kernel】TCP连接失败";
    }

    m_pLoginDlg->show();
}

// ===================== 收包【已修复】=====================
void Kernel::slotRecvData(char* buf, int len)
{
    qDebug()<<__func__;
    qDebug() << "========= 观众收到数据了！len =" << len;
    if (len <= 0) return;

    // ✅【修复】正确取协议号，不转小端
    PackType type = *(PackType*)buf;

    qDebug() << "观众[Kernel] 收到包，协议号：" << type << "，长度：" << len;
    if (type >= _DEF_PACK_BASE) {
        int idx = type - _DEF_PACK_BASE;
        if (idx >= 0 && idx < _DEF_PACK_COUNT) {
            if (m_dealFunArr[idx]) {
                (this->*m_dealFunArr[idx])(buf, len);
            }
        }
    }
}

// ===================== 登录 =====================
void Kernel::slotLoginReq(QString user, QString pwd)
{
    qDebug()<<__func__;
    STRU_LOGIN_RQ req;
    strcpy(req.name, user.toUtf8().data());
    strcpy(req.password, pwd.toUtf8().data());
    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 注册 =====================
void Kernel::slotRegisterReq(QString user, QString tel, QString pwd1, QString pwd2)
{
    qDebug()<<__func__;
    if (pwd1 != pwd2) return;

    STRU_REGISTER_RQ req;
    strcpy(req.name, user.toUtf8().data());
    strcpy(req.tel, tel.toUtf8().data());
    strcpy(req.password, pwd1.toUtf8().data());

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 加入房间【已修复】=====================
void Kernel::slotJoinRoomReq(unsigned long long roomNo)
{
    qDebug()<<__func__;
    qDebug() << "当前 m_bInRoom = " << m_bInRoom;
    qDebug() << "当前房间号 = " << m_currentRoomNo;
    qDebug() << "目标进入房间号 = " << roomNo;
    // 已在房间，禁止重复进房
    if (m_bInRoom&&(roomNo == m_currentRoomNo))
    {
        qDebug() << "当前已在房间内，禁止重复进入";
        return;
    }
    qDebug() << "【观众Kernel】请求加入房间号：" << roomNo;

    STRU_JOIN_ROOM_RQ req;
    req.userid = m_loginUserId;
    req.room_no = qToLittleEndian(roomNo);

    if(m_pTcpClient)
    {
        m_pTcpClient->sendMsg((char*)&req, sizeof(req));
    }
    else
    {
        qDebug() << "TCP连接已断开，无法发送进房请求";
    }
}

// ===================== 注册结果 =====================
void Kernel::dealRegisterRs(char* buf, int len)
{
    qDebug()<<__func__;
    STRU_REGISTER_RS* rsp = (STRU_REGISTER_RS*)buf;
    if (rsp->result == register_success) {
        QMessageBox::information(m_pLoginDlg, "成功", "注册成功！");
    } else {
        QMessageBox::warning(m_pLoginDlg, "失败", "账号已存在！");
    }
}

// ===================== 登录结果 =====================
void Kernel::dealLoginRs(char* buf, int len)
{
    qDebug()<<__func__;
    STRU_LOGIN_RS* rsp = (STRU_LOGIN_RS*)buf;

    if (rsp->result == login_success) {
        m_loginUserId = rsp->userid;
        qDebug() << "观众登录成功 userid：" << m_loginUserId;

        QMessageBox::information(m_pLoginDlg, "成功", "登录成功");
        m_pLoginDlg->close();
        if (m_pPlayerDlg == nullptr)
        {
            m_pPlayerDlg = new PlayerDialog;
            connect(m_pPlayerDlg, &PlayerDialog::sigJoinRoom, this, &Kernel::slotJoinRoomReq);
        }

        m_pPlayerDlg->show();

    } else {
        QMessageBox::warning(m_pLoginDlg, "失败", "账号或密码错误");
    }
}

// ===================== 进房结果=====================
void Kernel::dealJoinRoomRs(char* buf, int len)
{
    qDebug()<<"Kernel::dealJoinRoomRs";
    STRU_JOIN_ROOM_RS* rsp = (STRU_JOIN_ROOM_RS*)buf;
    qDebug() << "STRU_JOIN_ROOM_RS size = " << sizeof(STRU_JOIN_ROOM_RS);
    qDebug() << "result:" << rsp->result;
    qDebug() << "room_no:" << rsp->room_no;
    qDebug() << "rtmp_url:" << rsp->rtmp_url;
    qDebug() << "host_uid:" << rsp->host_uid;
    if (rsp->result == join_room_success) {
        QString url = rsp->rtmp_url;
        m_bInRoom = true;       // 标记已在房间
        m_currentRoomNo = qFromLittleEndian(rsp->room_no);
        m_hostUserId    = rsp->host_uid;
        qDebug() << "获取拉流地址：" << url;
        setCurrentRoom(m_currentRoomNo);
        int hostId = qFromLittleEndian(rsp->host_uid);
        qDebug()<<"下发主播ID:"<<hostId;
        m_pPlayerDlg->setHostId(hostId);
        bool bIsAdmin = m_adminList.contains(m_loginUserId);
        emit sigAdminStatusChanged(bIsAdmin);

        //m_pPlayerDlg->setUrl(url);
        //QMessageBox::information(m_pPlayerDlg, "进房成功", "已获取直播地址！");
    } else {
        m_bInRoom = false;
        m_currentRoomNo = 0;
        QMessageBox::warning(m_pPlayerDlg, "失败", "房间不存在或未开播");
    }
}


void Kernel::setCurrentRoom(unsigned long long roomNo)
{
    m_currentRoomNo = roomNo;
}

// ===================== 发送弹=====================
void Kernel::sendComment(const QString &content)
{
    qDebug() << __func__ << "发送到房间：" << m_currentRoomNo;
    qDebug() << "发送内容：" << content;

    if(isSelfBanned())
    {
        QMessageBox::information(nullptr, "提示", "您当前已被禁言，无法发言！");
        return;
    }

    STRU_SEND_DANMU_RQ req;
    req.userid = m_loginUserId;
    req.room_no = qToLittleEndian(m_currentRoomNo);
    strncpy(req.msg, content.toUtf8().data(), sizeof(req.msg)-1);

    qDebug() << "结构体内容：" << req.msg;

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
    qDebug() << "=== 弹幕已发送 ===";

}

void Kernel::sendGift(const QString &giftMsg)
{
    qDebug()<<__func__;
    STRU_SEND_DANMU_RQ req;
    req.type = _DEF_PACK_SEND_DANMU_RQ;
    req.userid = m_loginUserId;
    req.room_no = qToLittleEndian(m_currentRoomNo);
    strncpy(req.msg, giftMsg.toUtf8().data(), sizeof(req.msg)-1);

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 接收弹幕=====================
//void Kernel::dealDanmuBroadcast(char* buf, int len)
//{
//    qDebug()<<__func__;
//    qDebug() << "=== 收到弹幕包，长度：" << len;

//    if (len < (int)sizeof(STRU_DANMU_BROADCAST)) {
//        qDebug() << "!!! 包长度不足！";
//        return;
//    }

//    STRU_DANMU_BROADCAST* pkg = (STRU_DANMU_BROADCAST*)buf;
//    QString msg = QString::fromUtf8(pkg->msg);
//    int sendUid = pkg->userid;

//    // 这里打印主播ID
//    qDebug()<<"本条发送uid="<<sendUid<<" 本地保存主播id="<< m_pPlayerDlg->m_hostId;

//    emit sigRecvComment(msg,sendUid);
//}
void Kernel::dealDanmuBroadcast(char* buf, int len)
{
    qDebug()<<__func__;
    qDebug() << "=== 收到弹幕包，长度：" << len;

    if (len < (int)sizeof(STRU_DANMU_BROADCAST)) {
        qDebug() << "!!! 包长度不足！";
        return;
    }

    STRU_DANMU_BROADCAST* pkg = (STRU_DANMU_BROADCAST*)buf;
    QString msg = QString::fromUtf8(pkg->msg);
    int sendUid = pkg->userid;

    qDebug()<<"本条发送uid="<<sendUid<<" 本地保存主播id="<< m_pPlayerDlg->m_hostId;

    emit sigRecvComment(msg,sendUid);

    // ===========================
    // 🔥 增加礼物判断（必须加！）
    // ===========================
    if (msg.contains("火箭") || msg.contains("玫瑰") ||
        msg.contains("跑车") || msg.contains("棒棒糖") ||
        msg.contains("啤酒") || msg.contains("赠送"))
    {
        qDebug() << "🔥 这是礼物！触发特效！";
        emit sigRecvGift(msg);
    }
}
void Kernel::dealStopLiveRs(char *buf, int len)
{
    // ====================== 超强调试输出 ======================
    qDebug() << "\n=====================================================";
    qDebug() << "🔥 【观众Kernel】dealStopLiveRs 被调用！！！收到下播协议！";
    qDebug() << "数据包长度 len = " << len;
    qDebug() << "结构体大小 sizeof(STRU_STOP_LIVE_RS) = " << sizeof(STRU_STOP_LIVE_RS);
    qDebug() << "进入前 m_bInRoom = " << m_bInRoom;
    qDebug() << "进入前 m_currentRoomNo = " << m_currentRoomNo;
    qDebug() << "=====================================================\n";

    qDebug() << "收到主播下播通知 | 数据包长度 len = " << len;

    // 至少要大于等于结构体长度
    if (len < (int)sizeof(STRU_STOP_LIVE_RS)) {
        qDebug() << "❌ 数据包长度不足！无法解析！";
        return;
    }

    STRU_STOP_LIVE_RS* rsp = (STRU_STOP_LIVE_RS*)buf;
    qDebug() << "解析结果 rsp->result = " << rsp->result;

    if (rsp->result == stop_success)
    {
        qDebug() << "✅ 下播成功，准备发射 signalRoomClosed 信号！";

        // 发射信号
        qDebug() << "准备发射 signalRoomClosed...";
        emit signalRoomClosed();
        qDebug() << "signalRoomClosed 发射完毕！";

        qDebug() << "✅ signalRoomClosed 信号发射完成！";
        m_bInRoom = false;
        // 清空房间数据
        m_currentRoomNo = 0;
        m_hostUserId    = 0;
        m_adminList.clear();
        m_onlineUserList.clear();
        m_banUserList.clear();
        qDebug() << "✅ 清空后 m_bInRoom = " << m_bInRoom;

        qDebug() << "✅ 当前房间号已清空：m_currentRoomNo = " << m_currentRoomNo;
        //emit signalRoomClosed();
        //qDebug() << "✅ signalRoomClosed 信号发送完成";

    }
    else
    {
        qDebug() << "❌ 下播失败，result 不是 1！不触发关闭！";
    }
}
void Kernel::dealUserBroadcast(char* buf, int len)
{
    qDebug()<<__func__;
    qDebug() << "观众[Kernel::dealUserBroadcast] 被调用！userCnt=" << ((STRU_ONLINE_USER_SYNC*)buf)->userCnt;
    STRU_ONLINE_USER_SYNC* pData = (STRU_ONLINE_USER_SYNC*)buf;
    if(!pData) return;

    m_currentRoomNo = pData->room_no;
    m_onlineUserList.clear();

    // 遍历在线用户ID
    for(int i = 0; i < pData->userCnt; ++i)
    {
        qDebug() << "在线用户["<<i<<"] uid = " << pData->userUid[i];
        m_onlineUserList.append(pData->userUid[i]);
    }
    qDebug() << "[Kernel::dealUserBroadcast] 准备发射 sigDataSync";
    // 通知界面刷新列表
    emit sigDataSync();
    qDebug() << "[Kernel::dealUserBroadcast] sigDataSync 已发射";

}
void Kernel::dealAdminBroadcast(char* buf, int len)
{
    qDebug()<<__func__;
    STRU_ADMIN_SYNC_BROAD* pData = (STRU_ADMIN_SYNC_BROAD*)buf;
    if(!pData) return;

    m_currentRoomNo = pData->room_no;
    m_adminList.clear();

    // 遍历房管ID
    for(int i = 0; i < pData->adminCnt; ++i)
    {
        m_adminList.append(pData->adminUid[i]);
        qDebug() << "【房管列表更新】UID:" << pData->adminUid[i];
    }
    bool bIsAdmin = m_adminList.contains(m_loginUserId);
    qDebug() << "【房管状态检测】本人UID:" << m_loginUserId
             << " 是否房管:" << bIsAdmin;

    // 发射状态变更信号，通知界面刷新按钮
    emit sigAdminStatusChanged(bIsAdmin);

    // 刷新界面
    emit sigDataSync();
}
void Kernel::dealBanBroadcast(char* buf, int len)
{
    qDebug()<<__func__;
    STRU_MUTE_SYNC_BROAD* pData = (STRU_MUTE_SYNC_BROAD*)buf;
    if(!pData) return;

    m_currentRoomNo = pData->room_no;
    m_banUserList.clear();
    bool oldBanState = isSelfBanned(); // 记录旧状态
    // 遍历被禁言用户ID
    for(int i = 0; i < pData->muteCount; ++i)
    {
        m_banUserList.append(pData->muteUid[i]);
    }
    bool newBanState = isSelfBanned();

    // 仅在【刚被禁言】时弹窗，重复广播不重复弹
    if (!oldBanState && newBanState)
    {
        QMessageBox::information(nullptr, "提示", "您已被管理员禁言，暂时无法发言！");
    }

    emit sigDataSync();
}
// 单人禁言
void Kernel::sendBanReq(int targetUid)
{
    qDebug()<<__func__ << " targetUid:" << targetUid;
    if(!m_pTcpClient || m_currentRoomNo == 0) return;

    STRU_MUTE_CTRL_RQ req;
    req.opType    = OP_MUTE_ONE;
    req.adminUid  = m_loginUserId;
    req.targetUid = targetUid;
    req.room_no   = qToLittleEndian(m_currentRoomNo); // 修复：转小端
    memset(req.adminName, 0, sizeof(req.adminName));

    m_pTcpClient->sendMsg((char*)&req, sizeof(STRU_MUTE_CTRL_RQ));
}

// 解除单人禁言
void Kernel::sendUnBanReq(int targetUid)
{
    qDebug()<<__func__ << " targetUid:" << targetUid;
    if(!m_pTcpClient || m_currentRoomNo == 0) return;

    STRU_MUTE_CTRL_RQ req;
    req.opType    = OP_UNMUTE_ONE;
    req.adminUid  = m_loginUserId;
    req.targetUid = targetUid;
    req.room_no   = qToLittleEndian(m_currentRoomNo); // 修复：转小端
    memset(req.adminName, 0, sizeof(req.adminName));

    m_pTcpClient->sendMsg((char*)&req, sizeof(STRU_MUTE_CTRL_RQ));
}
// 添加房管
void Kernel::sendAddAdminReq(int targetUid)
{
    qDebug()<<__func__;
    if(!m_pTcpClient || m_currentRoomNo == 0) return;

    STRU_SET_ADMIN_RQ req;
    req.opType    = OP_ADD_ADMIN;    // 枚举：添加房管
    req.operUid   = m_loginUserId;
    req.targetUid = targetUid;
    req.room_no   = m_currentRoomNo;
    memset(req.operName, 0, sizeof(req.operName)); // 清空昵称

    m_pTcpClient->sendMsg((char*)&req, sizeof(STRU_SET_ADMIN_RQ));
}

// 移除房管
void Kernel::sendDelAdminReq(int targetUid)
{
    qDebug()<<__func__;
    if(!m_pTcpClient || m_currentRoomNo == 0) return;

    STRU_SET_ADMIN_RQ req;
    req.opType    = OP_DEL_ADMIN;    // 枚举：移除房管
    req.operUid   = m_loginUserId;
    req.targetUid = targetUid;
    req.room_no   = m_currentRoomNo;
    memset(req.operName, 0, sizeof(req.operName));

    m_pTcpClient->sendMsg((char*)&req, sizeof(STRU_SET_ADMIN_RQ));
}


void Kernel::slot_open_admin_dialog()
{
    qDebug()<<__func__;
    int selfUid = m_loginUserId;
    bool isHost  = (selfUid == m_hostUserId);
    bool isAdmin = m_adminList.contains(selfUid);

    // 普通观众直接拦截
    if(!isHost && !isAdmin)
    {
        QMessageBox::warning(nullptr, "提示", "您暂无管理权限！");
        return;
    }

    // 单例窗口，只创建一次
    if(nullptr == m_adminDlg)
    {
        m_adminDlg = new AdminManageDialog(nullptr);
    }

    m_adminDlg->show();
    m_adminDlg->raise();
}
void Kernel::dealSetAdminRs(char* buf, int len)
{
    qDebug()<<__func__;
    STRU_SET_ADMIN_RS* pData = (STRU_SET_ADMIN_RS*)buf;
    if(!pData) return;

    if (pData->result == 1)
    {
        QMessageBox::information(nullptr, "提示", "房管操作成功！");
    }
    else
    {
        QMessageBox::warning(nullptr, "提示", "房管操作失败！");
    }
}
bool Kernel::isSelfBanned() const
{
    return m_banUserList.contains(m_loginUserId);
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
void Kernel::quitRoom()
{
    qDebug() << __func__;
    // 不在房间直接返回
    if (!m_bInRoom)
    {
        qDebug() << "当前未进入房间，无需退出";
        return;
    }

    qDebug() << "主动退出房间，房间号：" << m_currentRoomNo;

    //先向服务端发送退房请求包
    sendLeaveRoomReq();

    // 2. 再清空本地房间状态
    m_bInRoom = false;
    m_currentRoomNo = 0;

    // 3. 触发全局数据刷新信号（同步UI列表）
    emit sigDataSync();
    qDebug() << "【Kernel】本地房间状态已清空";
}
void Kernel::sendLeaveRoomReq()
{
    qDebug() << __func__;
    if (!m_bInRoom || m_currentRoomNo == 0)
    {
        qDebug() << "当前未在房间，无需发送退房请求";
        return;
    }

    STRU_LEAVE_ROOM_RQ req;
    memset(&req, 0, sizeof(req));
    req.type = _DEF_PACK_LEAVE_ROOM_RQ;
    req.room_no = qToLittleEndian(m_currentRoomNo); // 房间号转小端发包

    qDebug() << "【Kernel】发送退房请求，房间号：" << m_currentRoomNo;
    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}
void Kernel::dealLeaveRoomRs(char* buf, int len)
{
    STRU_LEAVE_ROOM_RS* rs = (STRU_LEAVE_ROOM_RS*)buf;
    if (!rs) return;

    if (rs->result == 1)
    {
        qDebug() << "【Kernel】退房请求服务端处理成功";
    }
    else
    {
        qDebug() << "【Kernel】退房请求服务端处理失败";
    }
}
// 定时触发：每3秒执行一次
void Kernel::onHeartBeatTimeout()
{
    qDebug() << "【DEBUG】onHeartBeatTimeout 被调用！！！";
    sendHeartBeat();
}

// 组装并发送心跳包
void Kernel::sendHeartBeat()
{
    qDebug()<<__func__;
    qDebug() << "【DEBUG】m_pTcpClient 地址：" << m_pTcpClient;
    if ((!m_pTcpClient) || !m_pTcpClient->isConnected())
    {
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
    qDebug() << "【观众端】TCP连接已断开";

    // 1. 停止心跳定时器
    if(m_heartBeatTimer->isActive())
    {
        m_heartBeatTimer->stop();
    }

    // 2. 清空所有房间/登录状态
    m_bInRoom = false;
    m_loginUserId = 0;
    m_hostUserId = 0;
    m_currentRoomNo = 0;
    m_adminList.clear();
    m_onlineUserList.clear();
    m_banUserList.clear();

    // 3. 关闭直播界面、管理界面
    if(m_pPlayerDlg)
    {
        m_pPlayerDlg->close();
    }
    if(m_adminDlg)
    {
        m_adminDlg->close();
    }

    // 4. 弹窗提示 + 回到登录页
    QMessageBox::warning(nullptr, "连接断开", "与服务器连接中断，请重新登录！");
    if(m_pLoginDlg)
    {
        m_pLoginDlg->show();
    }
}

// ===================== 发送获取房间列表请求 =====================
void Kernel::SendRoomListReq(int pageIndex, int pageSize, int sortType, const QString& searchKey)
{
    qDebug() << __func__;
    qDebug() << "【Kernel】请求房间列表 page:" << pageIndex << "size:" << pageSize
             << "sort:" << sortType << "key:" << searchKey;

    STRU_GET_ROOM_LIST_RQ req;
    req.page_index = pageIndex;
    req.page_size = pageSize;
    req.sort_type = sortType;
    strncpy(req.search_key, searchKey.toUtf8().data(), sizeof(req.search_key)-1);

    m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}

// ===================== 房间列表响应解析 =====================
void Kernel::dealRoomListRs(char* buf, int len)
{
    qDebug() << __func__;
    qDebug() << "【Kernel】收到房间列表响应，长度:" << len;

    if (len < (int)sizeof(STRU_GET_ROOM_LIST_RS)) {
        qDebug() << "!!! 包长度不足！";
        return;
    }

    STRU_GET_ROOM_LIST_RS* rsp = (STRU_GET_ROOM_LIST_RS*)buf;
    qDebug() << "【Kernel】cur_page:" << rsp->cur_page << "total_page:" << rsp->total_page
             << "item_cnt:" << rsp->item_cnt;

    // 发射信号给UI
    emit sig_RoomListResp(*rsp);
}

// ===================== 房间列表变更通知解析 =====================
void Kernel::dealRoomListUpdateNotify(char* buf, int len)
{
    qDebug() << __func__;
    qDebug() << "【Kernel】收到房间列表变更通知，长度:" << len;

    if (len < (int)sizeof(STRU_ROOM_LIST_UPDATE_NOTIFY)) {
        qDebug() << "!!! 包长度不足！";
        return;
    }

    STRU_ROOM_LIST_UPDATE_NOTIFY* notify = (STRU_ROOM_LIST_UPDATE_NOTIFY*)buf;
    qDebug() << "【Kernel】update_type:" << notify->update_type
             << (notify->update_type == 1 ? "(新开播)" : "(下播)");

    // 发射信号通知UI刷新
    emit sigRoomListUpdateNotify(notify->update_type);
}
