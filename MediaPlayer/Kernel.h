#ifndef KERNEL_H
#define KERNEL_H

#include <QObject>
#include <QString>
#include "TcpClient.h"
#include "packdef.h"
#include "logindialog.h"
#include "playerdialog.h"
#include <QTimer>
//class PlayerDialog;
class AdminManageDialog;
class Kernel : public QObject
{
    Q_OBJECT
public:
    static Kernel* getInstance();
    ~Kernel() override;
    // 统一初始化
    void init();

    // 获取TCP
    TcpClient* getTcpClient() { return m_pTcpClient; }

    // 获取主窗口
    PlayerDialog* getPlayerDialog() { return m_pPlayerDlg; }
    //设置当前房间号
    void setCurrentRoom(unsigned long long roomNo);
    // 发评论（观众/主播都用）
    void sendComment(const QString& content);
    // 发礼物
    void sendGift(const QString& giftMsg);
    int               m_loginUserId;       // 观众用户ID
    int                m_hostUserId = 0;
    QList<int>         m_adminList;
    QList<int>         m_onlineUserList;
    QList<int>         m_banUserList;
    AdminManageDialog* m_adminDlg = nullptr;

private:
    explicit Kernel(QObject *parent = nullptr);
    static Kernel* m_pInstance;

    // 协议处理函数数组
    typedef void (Kernel::*DealFun)(char*, int);
    DealFun m_dealFunArr[_DEF_PACK_COUNT];

    // 初始化协议映射
    void initProtocolMap();
    QTimer* m_heartBeatTimer;  // 心跳定时器
    void sendHeartBeat();      // 发送心跳包函数

public:
    int getLoginUserId() const { return m_loginUserId; }
    int getHostUserId() const { return m_hostUserId; }
    bool isAdmin(int uid) const { return m_adminList.contains(uid); }
    QList<int> getAdminList() const { return m_adminList; }
    QList<int> getOnlineUserList() const { return m_onlineUserList; }
    QList<int> getBanUserList() const { return m_banUserList; }
    void sendAddAdminReq(int targetUid);    // 添加房管
    void sendDelAdminReq(int targetUid);    // 移除房管
    void sendBanReq(int targetUid);         // 禁言
    void sendUnBanReq(int targetUid);        // 解禁
    // 判断当前登录用户是否被禁言
    bool isSelfBanned() const;
    void quitRoom();//主动退出房间
    bool isInRoom() const { return m_bInRoom; }
    // 发送退房请求包
    void sendLeaveRoomReq();

public slots:
    // 统一接收数据
    void slotRecvData(char* buf, int len);

    // 登录请求
    void slotLoginReq(QString user, QString pwd);

    // 注册请求
    void slotRegisterReq(QString user, QString tel, QString pwd1, QString pwd2);

    // 加入房间请求（观众核心）
    void slotJoinRoomReq(unsigned long long roomNo);

    void dealDanmuBroadcast(char* buf, int len);
    void slot_open_admin_dialog(); // 打开管理窗口


    void dealBanBroadcast(char* buf, int len);
    void dealAdminBroadcast(char* buf, int len);
    void dealUserBroadcast(char* buf, int len);
    // 房管设置回执解析
    void dealSetAdminRs(char* buf, int len);
    void dealSysNoticeBroad(char* buf, int len);
    // 退房回执解析
    void dealLeaveRoomRs(char* buf, int len);
    void onHeartBeatTimeout(); // 定时器超时槽函数
    // TCP连接断开回调
    void slotOnDisconnected();

signals:
    void sigLoginSuccess();
    // 给界面发：显示弹幕
    void sigRecvComment(const QString& msg,int sendUid);
    void signalRoomClosed();
    void sigRecvGift(const QString& giftMsg);
    // 统一刷新信号：列表、身份、禁言状态全部刷新
    void sigDataSync();
    void sigAdminStatusChanged(bool isAdmin); //本人房管状态变化

private:
    // ===================== 协议处理 =====================
    void dealRegisterRs(char* buf, int len);
    void dealLoginRs(char* buf, int len);
    void dealJoinRoomRs(char* buf, int len);
    void dealStopLiveRs(char* buf, int len); // 下播应答

private:
    TcpClient*        m_pTcpClient;
    LoginDialog*      m_pLoginDlg;
    PlayerDialog*     m_pPlayerDlg=nullptr;
    bool              m_bInRoom = false;//是否已进入房间


     unsigned long long m_currentRoomNo = 0; // 当前房间号


};

#endif // KERNEL_H
