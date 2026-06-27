#ifndef KERNEL_H
#define KERNEL_H

#include <QObject>
#include <QDebug>
#include <QMessageBox>
#include "TcpClient.h"
#include "packdef.h"
#include "LoginDialog.h"
#include "RoomCreateDialog.h"
#include "PictureWidget.h"
#include"TcpClient.h"
#include <QMap>
#include <QList>
#include <QTimer>
class AdminManageDialog; // 前置声明
class RoomHallWidget;   // 直播大厅窗口前置声明

// 先前置声明Kernel类
class Kernel;
// 再定义函数指针
typedef void (Kernel::*DealFun)(char* buf, int len);

class Kernel : public QObject
{
    Q_OBJECT
public:
    static Kernel* getInstance();
    ~Kernel() override;
    void init();

    void sendComment(const QString& content);   // 发评论
    void sendGift(const QString& giftMsg);     // 发礼物
    void setCurrentRoom(unsigned long long roomNo);
    //下播函数
    void sendStopLiveRequest();
    QList<int> getAdminList() const;
    bool isAdmin(int uid) const;
    QList<int> getMuteUidList() const;
    bool isSelfBanned() const;


private:
    explicit Kernel(QObject *parent = nullptr);
    static Kernel* m_pInstance;

    // 函数指针数组
    DealFun m_dealFunArr[100];
    // 初始化协议映射
    void initProtocolMap();
signals:
    void sigLoginSuccess(); // 新增这行登录成功信号
    void sigRecvComment(const QString& msg,int userid);    // 所有界面接收消息
    void sigRecvGift(const QString& msg);      // 礼物动画
    void sigRoomClosed();  // 房间关闭信号（主播下播）
    void sigRoomCountUpdate(int count);
    void sigAdminSync();        // 房管列表更新，通知UI刷新
    void sigOnlineUserSync();   // 在线用户列表更新
    void sigMuteStateUpdate();         // 禁言状态更新信号
    void sigOpenAdminDialog();
    void sigDataSync();//在线用户/全体数据同步
    void sig_RoomListResp(const STRU_GET_ROOM_LIST_RS& rsp);//房间列表响应信号
    void sigRoomListUpdateNotify(int update_type);//房间列表变更通知（新开播/下播）


private slots:
    // 接收TCP数据，参数匹配信号：char* buf,int len
    void slotRecvData(char* buf,int len);
    //void slotRecvData(const QByteArray &data);
    // 登录注册界面信号槽
    void slotLoginReq(QString user, QString pwd);
    void slotRegisterReq(QString user,QString tel, QString pwd1, QString pwd2);
    //创建房间
    void slotCreateRoom(QString roomName);
    void dealCreateRoomRs(char* buf, int len);

    // 各个协议处理函数
    void dealRegisterRs(char* buf, int len);
    void dealLoginRs(char* buf, int len);

    // 处理服务器的聊天消息
    void dealChatMsg(char* buf, int len);
    //下播
    void dealStopLiveRs(char* buf, int len);

    void dealRoomCountBroadcast(char *buf, int len);

    void dealAdminSyncBroad(char* buf, int len);    // 房管列表广播包
    void dealOnlineUserSync(char* buf, int len);    // 在线用户列表广播包
    void dealMuteSyncBroad(char* buf, int len);  // 禁言同步广播解析
    void dealSysNoticeBroad(char* buf, int len);
    void onHeartBeatTimeout();   // 定时回调
    void sendHeartBeat();        // 发送心跳包

public slots:
    void slot_open_admin_dialog();
    void slotOnDisconnected();   // TCP断开连接回调
    void dealGetRoomListRs(char* buf, int len);//房间列表协议处理
    void dealRoomListUpdateNotify(char* buf, int len);//房间列表变更通知处理
    void SendRoomListReq(int page_index, int page_size, int sort_type, const QString& search_key);
    // 大厅按钮触发：打开创建房间弹窗
    void slot_OpenCreateRoomDialog();


public:
    TcpClient*  m_pTcpClient = nullptr;
    LoginDialog* m_pLoginDlg = nullptr;
    RoomCreateDialog* m_pRoomCreateDlg = nullptr;
    AdminManageDialog* m_adminDlg = nullptr;
    RoomHallWidget* m_pRoomHall;    // 直播大厅窗口

    int m_loginUserId = 0; // 保存登录成功的用户ID
    unsigned long long m_currentRoomNo = 0;
    QString m_loginName; //登录保存的用户名，登录成功时赋值
     int m_hostUserId;//主播userid
     QMap<int, QString>  m_onlineUserMap;   // UID -> 昵称
     QList<int>          m_adminList;      // 房间房管UID列表
     // 禁言状态
     bool m_isAllMute = false;          // 全体禁言开关
     QList<int> m_muteUidList;          // 单人禁言UID列表
     // 心跳定时器
     QTimer* m_heartBeatTimer;
};

#endif // KERNEL_H
