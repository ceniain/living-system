#ifndef PLAYERDIALOG_H
#define PLAYERDIALOG_H

#include <QDialog>
#include "videoplayer.h"
#include<QTimer>
#include<QDebug>
#include<QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QList>
#include "RoomHallWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class PlayerDialog; }
QT_END_NAMESPACE

class PlayerDialog : public QDialog
{
    Q_OBJECT

public:
    PlayerDialog(QWidget *parent = nullptr);
    ~PlayerDialog();
    // 填入拉流RTMP地址
    void setUrl(const QString&);
     int m_hostId = 0; //保存当前房间主播ID
     void setHostId(int id){m_hostId = id;qDebug() << "【成功保存主播ID】" << id; // 加打印确认
}
    // 显示弹幕 + 评论列表（统一接口）
void addCommentAndDanmu(const QString& msg);
void setPlayUrl(const QString &rtmpUrl);
void sendGift(const QString &giftMsg);
signals:
    // 点击查询，发送房间号到Kernel
    void sigJoinRoom(unsigned long long roomNo);
private slots:
    void on_pb_start_clicked();

    void slots_setImage(QImage img);

    void on_pb_resume_clicked();

    void on_pb_pause_clicked();

    void on_pb_stop_clicked();

    void slot_PlayerStateChanged(int state);

    void slot_getTotalTime(qint64 uSec);

    void slot_TimerTimeOut();

    bool eventFilter(QObject * obj,QEvent * event);

    void on_pb_setUrl_clicked();
    //查询房间
    void on_pb_query_clicked();
    //发送评论
    void on_pb_send_clicked();
    //送礼
    void on_pb_gift_clicked();


    // 弹幕滚动
    void sendDanmu(const QString& text,int sendUid);
    // 评论列表追加
    void addCommentToList(const QString& msg,int sendUid);
    // 礼物动画
    void playGiftAnimation(const QString &imgPath);
    void updateInputState();

private slots:
    void onRecvGift(const QString& giftMsg); // 接收礼物、播放动画
    void onAdminStatusChanged(bool isAdmin);
    void on_pb_quit_room_clicked();

public slots:
    void onHostStopLive(); // 主播下播
    void on_btn_RoomHall_clicked(); // 打开直播大厅
    void closeRoomHall(); // 关闭直播大厅




private:
    Ui::PlayerDialog *ui;

    VideoPlayer *m_player;
    QTimer m_timer;
    //停止的状态
    bool isStop;
    QString m_playUrl;
    QGraphicsScene* m_scene = nullptr;
    QList<QGraphicsTextItem*> m_danmuItems;
    RoomHallWidget* m_roomHallWidget = nullptr;




};
#endif // PLAYERDIALOG_H
