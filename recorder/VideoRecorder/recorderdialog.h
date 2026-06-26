#ifndef RECORDERDIALOG_H
#define RECORDERDIALOG_H

#include <QDialog>
#include "picturewidget.h"
#include "savevideofilethread.h"
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPropertyAnimation>
#include <QList>
#include <QListWidgetItem>
#include <QRandomGenerator>



QT_BEGIN_NAMESPACE
namespace Ui { class RecorderDialog; }
QT_END_NAMESPACE

class RecorderDialog : public QDialog
{
    Q_OBJECT

public:
    RecorderDialog(QWidget *parent = nullptr);
    ~RecorderDialog();
    void setRoomNumber(QString no);
    void sendDanmu(const QString &text,bool isHost);   // 飘屏函数
    int m_myId; // 加这一行
    QImage whiteToTransparent(const QImage &src);
    QImage cropTransparent(const QImage &src);

private slots:
    void on_pb_start_clicked();

    void on_pb_stop_clicked();

    void on_pb_setUrl_clicked();

    void slot_setImage(QImage img);
    //发送评论按钮
    void on_pb_comment_clicked();
     void slotAddSelfComment(const QString& msg,int userid);
     void onRoomClosed(); // 下播关闭窗口
    //接收礼物
     void onRecvGift(const QString &giftMsg);
    //播放动画
     void playGiftAnimation(const QString &imgPath);

     void on_btn_admin_manage_clicked();

 signals:
     void sigOpenAdminDialog();


private:
    Ui::RecorderDialog *ui;

    PictureWidget * m_pictureWidget;
    SaveVideoFileThread * m_saveFileThread;

    QString m_saveUrl;
    QString m_roomNo;
    QGraphicsScene* m_scene = nullptr;//弹幕画布
    QList<QGraphicsTextItem*> m_danmuItems;//弹幕项管理

};
#endif // RECORDERDIALOG_H
