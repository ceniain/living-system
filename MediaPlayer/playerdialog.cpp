#include "playerdialog.h"
#include "ui_playerdialog.h"
//#define _DEF_PATH "D:/MediaPlayer/video/riku.mp4"
//#define _DEF_PATH "rtmp://192.168.179.129:1935/vod//101.mp4"
//rtmp://192.168.179.129:1935/vod/101.mp4
#define _DEF_PATH "http://111.40.196.9/PLTV/88888888/224/3221225606/index.m3u8"
#define _DEF_LIVE_PATH "rtmp://192.168.179.128:1935/videotest/user=100"
#include"videoplayer.h"
#include <QParallelAnimationGroup>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMediaContent>
#include <QUrl>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QList>
#include <QRandomGenerator>
#include "Kernel.h"


PlayerDialog::PlayerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PlayerDialog)
{
    ui->setupUi(this);
    // ========== 初始化弹幕场景 ==========
    m_scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setStyleSheet("background:transparent;");
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->wdg_effect_layer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->wdg_effect_layer->raise(); // 确保它在最上层
    ui->wdg_effect_layer->show();
    ui->wdg_effect_layer->setAttribute(Qt::WA_TranslucentBackground, true);
    ui->wdg_effect_layer->setStyleSheet("background-color: transparent;");
//    QMenu *menu = new QMenu(this);
//    menu->addAction("火箭", this, [=](){ playGiftAnimation(":/gift/rocket.png"); });
//    menu->addAction("棒棒糖", this, [=](){ playGiftAnimation(":/gift/sugar.png"); });
//    menu->addAction("玫瑰花", this, [=](){ playGiftAnimation(":/gift/flower.png"); });
//    menu->addAction("跑车", this, [=](){ playGiftAnimation(":/gift/car.png"); });

//    // 绑定到你原来的送礼按钮
//    ui->pb_gift->setMenu(menu);


    m_player=new VideoPlayer;
    connect(m_player, SIGNAL(SIG_getOneImage(QImage)),
            this, SLOT(slots_setImage(QImage)));
    //slot_PlayerStateChanged(PlayerState::Stop);
    //测试
    //m_player->setFileName(_DEF_PATH);
    //connect(&m_timer,SIGNAL(timeout()),
            //this,SLOT());
    connect(m_player,SIGNAL(SIG_PlayerStateChanged(int)),
            this,SLOT(slot_PlayerStateChanged(int)));
    connect( m_player, SIGNAL( SIG_TotalTime(qint64)) ,
            this ,SLOT( slot_getTotalTime(qint64)) );
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(slot_TimerTimeOut()));
    // 接收 Kernel 发来的弹幕（所有人的消息）
//    connect(Kernel::getInstance(),&Kernel::sigRecvComment,
//            this,&PlayerDialog::addCommentToList,
//            Qt::UniqueConnection);
    static bool bindFlag = false;
    if(!bindFlag)
    {
        connect(Kernel::getInstance(),&Kernel::sigRecvComment,
                this,&PlayerDialog::addCommentToList,
                Qt::UniqueConnection);
        connect(Kernel::getInstance(), &Kernel::signalRoomClosed,
                this, &PlayerDialog::onHostStopLive, Qt::UniqueConnection);
        connect(Kernel::getInstance(), &Kernel::sigRecvGift,
                this, &PlayerDialog::onRecvGift, Qt::UniqueConnection);
        connect(Kernel::getInstance(), &Kernel::sigAdminStatusChanged,
                this, &PlayerDialog::onAdminStatusChanged, Qt::UniqueConnection);
        connect(Kernel::getInstance(), &Kernel::sigDataSync,
                this, &PlayerDialog::updateInputState, Qt::UniqueConnection);

        bindFlag = true;
    }

    m_timer.setInterval(500);//超时时间500ms
    //安装事件过滤器，让该对象成为被观察 this去执行函数
    ui->slider_process->installEventFilter(this);
    // 1. 权限判断：普通观众直接禁用按钮
    Kernel* pKernel = Kernel::getInstance();
    if (pKernel)
    {
        int loginUid = pKernel->m_loginUserId;
        bool isHost = (loginUid == pKernel->m_hostUserId);
        bool isAdmin = pKernel->m_adminList.contains(loginUid);

        if (!isHost && !isAdmin)
        {
            ui->btn_admin_manage->setEnabled(false);
            ui->btn_admin_manage->setToolTip("仅主播和房管可操作");
        }
    }

    // 2. 按钮点击直接绑定到 Kernel 的槽函数
    connect(ui->btn_admin_manage, &QPushButton::clicked,
            Kernel::getInstance(), &Kernel::slot_open_admin_dialog);
    // =====================================================================
    updateInputState();


}


PlayerDialog::~PlayerDialog()
{
    // 清理直播大厅窗口
    if (m_roomHallWidget)
    {
        m_roomHallWidget->close();
        m_roomHallWidget->deleteLater();
        m_roomHallWidget = nullptr;
    }
    delete ui;
    delete m_player;
}

//QT线程
//QThread 定义子类 start()->run() ;
//打开文件播放
void PlayerDialog::on_pb_start_clicked()
{
    //开始播放-》一段时间内获取图片
    //m_player->start();
    //    //打开浏览选择文件
    QString path = QFileDialog::getOpenFileName(this,"选择要播放的文件" , "F:/",
                                                "视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv);; 所有文件(*.*);;");    //判断
    if(path.isEmpty()) return;
    //首先要先关闭 判断当前状态 stop
    if(m_player->playerState()!=PlayerState::Stop)
    {
        m_player->stop(true);
    }

//    //设置m_play fileName
    m_player->setFileName(path);
    m_player->setFileName(_DEF_PATH);
//    m_player->setFileName(_DEF_LIVE_PATH);

    m_player->start();
    //play
    slot_PlayerStateChanged(PlayerState::Playing);
}


void PlayerDialog::slots_setImage(QImage img)
{
    //pixmap imag的区别 显示与存储
//    qDebug() << "收到视频帧，img is null:" << img.isNull();
//    //缩放
//    QPixmap pixmap;
//    if( !img.isNull() )
//        pixmap=QPixmap::fromImage(img.scaled(ui->lb_show->size(),
//               Qt::KeepAspectRatio));
//    else
//        pixmap = QPixmap::fromImage(img);

    //要实现视频加速渲染OpenGL
    //qDebug() << "slots_setImage 收到帧，isNull:" << img.isNull();
    ui->wdg_show->slot_setImage(img);

//    ui->lb_show->setPixmap(pixmap);

}
void PlayerDialog::on_pb_resume_clicked()
{
    if(m_player->playerState()!= PlayerState::Pause)
    {
        return;
    }
    m_player->play();
    //切换
    ui->pb_resume->hide();
    ui->pb_pause->show();
}


void PlayerDialog::on_pb_pause_clicked()
{
    if(m_player->playerState()!= PlayerState::Playing)
    {
        return;
    }
    m_player->pause();
    //切换
    ui->pb_resume->show();
    ui->pb_pause->hide();
}


void PlayerDialog::on_pb_stop_clicked()
{
    m_player->stop(true);
}
void PlayerDialog::slot_PlayerStateChanged(int state)
{
    switch( state )
    {
    case PlayerState::Stop:
    qDebug()<< "PlayerDialog::Stop";
    m_timer.stop();
    ui->slider_process->setValue(0);
    ui->lb_totalTime->setText("00:00:00");
    ui->lb_curTime->setText("00:00:00");
    ui->pb_pause->hide();
    ui->pb_resume->show();
//    {
//        QImage img;
//        img.fill( Qt::black);
//        slots_setImage( img );
//    }
    this->update();
    isStop = true;
    break;
    case PlayerState::Playing:
    qDebug()<< "VideoPlayer::Playing";
    ui->pb_resume->hide();
    ui->pb_pause->show();
    m_timer.start();
    this->update();
    isStop = false;
    break;
    }
}

void PlayerDialog::slot_getTotalTime(qint64 uSec)
{
    qint64 Sec = uSec/1000000;
    ui->slider_process->setRange(0,Sec);//精确到秒
    QString hStr = QString("00%1").arg(Sec/3600);
    QString mStr = QString("00%1").arg(Sec/60);
    QString sStr = QString("00%1").arg(Sec%60);
    QString str =
        QString("%1:%2:%3").arg(hStr.right(2)).arg(mStr.right(2)).arg(sStr.right(2));
    ui->lb_totalTime->setText(str);
}
//获取当前视频时间定时器
void PlayerDialog::slot_TimerTimeOut()
{
    if (QObject::sender() == &m_timer)
    {
        qint64 Sec = m_player->getCurrentTime()/1000000;
        ui->slider_process->setValue(Sec);
        QString hStr = QString("00%1").arg(Sec/3600);
        QString mStr = QString("00%1").arg(Sec/60%60);
        QString sStr = QString("00%1").arg(Sec%60);
        QString str =
            QString("%1:%2:%3").arg(hStr.right(2)).arg(mStr.right(2)).arg(sStr.right(2));
        ui->lb_curTime->setText(str);
        if(ui->slider_process->value() == ui->slider_process->maximum()
            && m_player->playerState() == PlayerState::Stop)
        {
            slot_PlayerStateChanged( PlayerState::Stop );
        }else if(ui->slider_process->value() + 1 ==
                       ui->slider_process->maximum()
                   && m_player->playerState() == PlayerState::Stop)
        {
            slot_PlayerStateChanged( PlayerState::Stop );
        }
    }
}

#include<QStyle>
#include<QMouseEvent>
bool PlayerDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->slider_process)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            int min=ui->slider_process->minimum();
            int max=ui->slider_process->maximum();
            int value=QStyle::sliderValueFromPosition(
                min,max,mouseEvent->pos().x(),ui->slider_process->width());

            m_timer.stop();
            ui->slider_process->setValue(value);
            m_player->seek((qint64)value*1000000);
            m_timer.start();
            return true;
        }
        else
        {
            return false;
        }
    }
    //空格 暂停/恢复 左右 退回/快进 上下 音量调整
    // pass the event on to the parent class
    return QDialog::eventFilter(obj, event);
}

#include <QMessageBox> // 必须加这个头文件

//void PlayerDialog::on_pb_url_clicked()
//{
//    bool ok;
//    QString url = QInputDialog::getText(
//        this,
//        "播放网络视频",
//        "请输入视频地址（m3u8 / mp4 等）:",
//        QLineEdit::Normal,
//        "http://localhost:8000/output.m3u8",
//        &ok
//        );

//    if (ok && !url.trimmed().isEmpty()) {
//        // 1. 先停止当前播放
//        if (m_player->playerState() != PlayerState::Stop) {
//            m_player->stop(true);
//        }

//        // 2. 设置新的 URL
//        m_player->setFileName(url);

//        // 3. 启动播放
//        m_player->start();

//        QMessageBox::information(this, "提示", "开始播放：\n" + url);
//    }
//}


//void PlayerDialog::on_pb_setUrl_clicked()
//{
//    QString url=QInputDialog::getText(this,"设置","输入链接地址:",QLineEdit::Normal,
//                                        "rtmp://192.168.179.129:1935/vod/101.mp4");///_DEF_LIVE_PATH
//    if(url.isEmpty()) return;

//    m_player->setFileName(url);
//    m_player->start();
//    slot_PlayerStateChanged(PlayerState::Playing);
//}
void PlayerDialog::on_pb_setUrl_clicked()
{
     qDebug()<<__func__;
    QString url = QInputDialog::getText(
        this,
        "设置",
        "输入链接地址:",
        QLineEdit::Normal,
        "rtmp://192.168.179.129:1935/vod/101.mp4");

    if(url.isEmpty()) return;

    // 关键：先停止当前播放，重置播放器状态
    if(m_player->playerState() != PlayerState::Stop)
    {
        m_player->stop(true);
    }

    // 等待一下，让播放器完全停止
    QThread::msleep(100);

    // 设置新地址并启动
    m_player->setFileName(url);
    m_player->start();
    slot_PlayerStateChanged(PlayerState::Playing);
}
// 自动给URL输入框赋值
void PlayerDialog::setUrl(const QString& rtmpUrl)
{
     qDebug()<<__func__;
    // 直接弹出url弹窗，并且预填RTMP直播地址
    QString url = QInputDialog::getText(this,"设置","输入链接地址：",QLineEdit::Normal,rtmpUrl);
    if(!url.isEmpty())
    {
        // 赋值给播放器变量，点打开即可拉流
        m_playUrl = url;
    }
}
void PlayerDialog::on_pb_send_clicked()
{
    qDebug()<<__func__;
    QString msg = ui->le_msg->text().trimmed();
    if (msg.isEmpty()) return;

    //QString showMsg = "观众：" + msg;

    // 本地显示
    //    sendDanmu(showMsg);
    //    addCommentToList(showMsg);
    Kernel::getInstance()->sendComment(msg);
    qDebug()<<"PlayerDialog::on_pb_send_clicked"<<msg;
    ui->le_msg->clear();


}
void PlayerDialog::setPlayUrl(const QString& rtmpUrl)
{
    m_playUrl = rtmpUrl;
}

void PlayerDialog::on_pb_query_clicked()
{
    qDebug() << __func__;

    QString strNo = ui->le_roomNo->text().trimmed();
    bool ok;
    unsigned long long roomNo = strNo.toULongLong(&ok);

    // 1. 校验房间号
    if (!ok || strNo.size() != 10)
    {
        QMessageBox::warning(this, "提示", "请输入10位直播间编号");
        return;
    }

    // 2. 你自己的拼接地址（正确，保留！）
    QString rtmpUrl = QString("rtmp://192.168.179.129/videotest/user=%1").arg(strNo);

    // ========================
    // 【修复二次进房黑屏】
    // 彻底重置播放器状态
    // ========================
    if (m_player->playerState() != PlayerState::Stop)
    {
        m_player->stop(true);
    }

    // 关键：加一句黑屏重置，防止画面卡住
    QImage blackImg(ui->wdg_show->size(), QImage::Format_RGB32);
    blackImg.fill(Qt::black);
    ui->wdg_show->slot_setImage(blackImg);

    QThread::msleep(50); // 短延时，更稳定

    // 播放新地址
    m_player->setFileName(rtmpUrl);
    m_player->start();
    slot_PlayerStateChanged(PlayerState::Playing);

    // 只弹一个成功提示
    QMessageBox::information(this, "成功", "正在播放房间：" + strNo);

    // 发送进房请求（只发请求，不做播放）
    emit sigJoinRoom(roomNo);
}


void PlayerDialog::addCommentToList(const QString &msg, int sendUid)
{
    qDebug()<<__func__;
    QListWidgetItem* item = new QListWidgetItem(msg);
    // 橙粉色文字 + 加粗
    if (msg.startsWith("【系统】"))
    {
        // RGB: 255, 105, 145 橙粉色
        item->setForeground(QBrush(QColor(255, 105, 145)));
        item->setFont(QFont("微软雅黑", 9, QFont::Bold));
    }

    ui->list_comment->addItem(item);
    ui->list_comment->scrollToBottom();
    sendDanmu(msg, sendUid);
}
#include <QImage>
#include <QBitmap>
#include <QImage>
#include <QImage>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>

// 把白色变成透明
QImage whiteToTransparent(const QImage &src)
{
    // 转成带透明通道
    QImage img = src.convertToFormat(QImage::Format_ARGB32);

    // ========================
    // 阈值调低一点！220 能去掉 99% 图片内部白色/灰白/浅白
    // ========================
    const int WHITE_THRESHOLD = 220;

    for (int y = 0; y < img.height(); ++y)
    {
        QRgb *row = (QRgb *)img.scanLine(y);
        for (int x = 0; x < img.width(); ++x)
        {
            QRgb p = row[x];
            int r = qRed(p);
            int g = qGreen(p);
            int b = qBlue(p);

            // ========================
            // 只要三个通道都大于阈值 → 全部变透明！
            // 不管是纯白、浅白、灰白、白边、内部白点，全部干掉
            // ========================
            if (r > WHITE_THRESHOLD && g > WHITE_THRESHOLD && b > WHITE_THRESHOLD)
            {
                row[x] = qRgba(0, 0, 0, 0);
            }
        }
    }

    return img;
}

// 裁掉四周全透明区域，只留火箭
QImage cropTransparent(const QImage &src)
{
    int l = src.width(), r = 0;
    int t = src.height(), b = 0;

    for (int y = 0; y < src.height(); ++y)
    {
        const QRgb *row = (const QRgb *)src.scanLine(y);
        bool hasContent = false;
        for (int x = 0; x < src.width(); ++x)
        {
            if (qAlpha(row[x]) > 0) // 不透明区域
            {
                hasContent = true;
                l = qMin(l, x);
                r = qMax(r, x);
            }
        }
        if (hasContent)
        {
            t = qMin(t, y);
            b = y;
        }
    }

    if (l > r || t > b) return src;
    return src.copy(QRect(l, t, r - l + 1, b - t + 1));
}
void PlayerDialog::playGiftAnimation(const QString &imgPath)
{
    qDebug()<<__func__ << imgPath;

    QWidget *effectLayer = ui->wdg_effect_layer;
    QLabel *gift = new QLabel(effectLayer);
    gift->setAttribute(Qt::WA_DeleteOnClose);

    // Label 完全透明
    gift->setAttribute(Qt::WA_TranslucentBackground, true);
    gift->setStyleSheet("background: transparent; border: none;");

    // 1. 加载对应礼物图片（从参数来）
    QPixmap pix(imgPath);
    if (pix.isNull())
    {
        gift->setText("🎁 礼物 🎁");
        gift->setStyleSheet("font-size:30px; color:yellow; background:transparent;");
        gift->adjustSize();
    }
    else
    {
        // 2. 去白色背景 → 透明
        QImage img = pix.toImage();
        img = whiteToTransparent(img);

        // 3. 裁掉多余透明（白边）
        img = cropTransparent(img);

        // 4. 转回 Pixmap 并缩放
        pix = QPixmap::fromImage(img);
        pix = pix.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        gift->setPixmap(pix);
        gift->setFixedSize(pix.size());
    }

    gift->show();

    // 居中
    int startX = (effectLayer->width() - gift->width()) / 2;
    int startY = effectLayer->height() / 2;
    gift->move(startX, startY);

    // 动画：放大 + 上升 + 渐隐
    QParallelAnimationGroup *group = new QParallelAnimationGroup(gift);

    QPropertyAnimation *scaleAnim = new QPropertyAnimation(gift, "geometry");
    scaleAnim->setDuration(3000);
    scaleAnim->setStartValue(QRect(startX, startY, gift->width(), gift->height()));
    scaleAnim->setKeyValueAt(0.3, QRect(startX-30, startY-30, gift->width()*1.5, gift->height()*1.5));
    scaleAnim->setEndValue(QRect(startX, -200, gift->width(), gift->height()));

    QPropertyAnimation *opacityAnim = new QPropertyAnimation(gift, "windowOpacity");
    opacityAnim->setDuration(3000);
    opacityAnim->setStartValue(1.0);
    opacityAnim->setEndValue(0.0);

    group->addAnimation(scaleAnim);
    group->addAnimation(opacityAnim);

    connect(group, &QParallelAnimationGroup::finished, gift, &QLabel::close);
    group->start();
}
void PlayerDialog::onHostStopLive()
{
    qDebug() << "PlayerDialog::onHostStopLive 主播已下播 → 恢复黑屏待机状态";

    // ========================
    // 0. 关闭直播大厅窗口
    // ========================
    closeRoomHall();

    // ========================
    // 1. 安全停止视频（非阻塞，绝不卡死）
    // ========================
    if (m_player->playerState() != PlayerState::Stop)
    {
        m_player->stop(false);   // 👈 必须 false！不等待线程，防止卡死
    }

    // ========================
    // 2. 清空弹幕、清空聊天
    // ========================
    m_scene->clear();
    m_danmuItems.clear();
    ui->list_comment->clear();

    // ========================
    // 3. 让视频区域 → 黑屏（回到初始状态）
    // ========================
    QImage blackImg(ui->wdg_show->size(), QImage::Format_RGB32);
    blackImg.fill(Qt::black);
    ui->wdg_show->slot_setImage(blackImg);

    // ========================
    // 4. 重置播放器UI状态（初始样子：停止、进度条清空）
    // ========================
    slot_PlayerStateChanged(PlayerState::Stop);

    // ========================
    // 5. 提示弹窗（不关闭窗口）
    // ========================
    QMessageBox::information(this, "提示", "主播已下播！\n已恢复待机状态");
}
// 礼物按钮
void PlayerDialog::on_pb_gift_clicked()
{
    qDebug()<<__func__;

    // 弹出礼物菜单
    QMenu giftMenu(this);

    giftMenu.addAction("送火箭🚀", [this]() {
        sendGift("【观众赠送火箭🚀】");
    });
    giftMenu.addAction("送玫瑰🌹", [this]() {
        sendGift("【观众赠送玫瑰🌹】");
    });
    giftMenu.addAction("送跑车🏎️", [this]() {
        sendGift("【观众赠送跑车🏎️】");
    });
    giftMenu.addAction("送棒棒糖🍭", [this]() {
        sendGift("【观众赠送棒棒糖🍭】");
    });
    giftMenu.addAction("送啤酒🍺", [this]() {
        sendGift("【观众赠送啤酒🍺】");
    });

    giftMenu.exec(ui->pb_gift->mapToGlobal(QPoint(0, ui->pb_gift->height())));
}

// 只发送给服务器，本地不播动画！
void PlayerDialog::sendGift(const QString& giftMsg)
{
    sendDanmu(giftMsg, Kernel::getInstance()->getLoginUserId());
    // 只发服务器，动画靠服务器返回播放！
    Kernel::getInstance()->sendGift(giftMsg);
}


void PlayerDialog::sendDanmu(const QString &text, int sendUid)
{
    qDebug()<<__func__;
    if (text.isEmpty()) return;
    QGraphicsTextItem *item = new QGraphicsTextItem(text);
    item->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));

    // ===========================
    // ✅ 自己发的 → 黄色
    // ✅ 别人发的 → 白色
    // ===========================
    if (sendUid == Kernel::getInstance()->getLoginUserId())
    {
        // 自己的消息：黄色
        item->setDefaultTextColor(Qt::yellow);
    }
    else
    {
        // 别人的消息：白色
        item->setDefaultTextColor(Qt::white);
    }

    item->setPos(ui->graphicsView->width(), qrand() % ui->graphicsView->height());
    m_scene->addItem(item);
    m_danmuItems << item;

    QPropertyAnimation *ani = new QPropertyAnimation(item, "pos");
    ani->setDuration(8000);
    ani->setStartValue(item->pos());
    ani->setEndValue(QPoint(-item->boundingRect().width(), item->pos().y()));
    ani->start(QAbstractAnimation::DeleteWhenStopped);

    connect(ani, &QPropertyAnimation::finished, this, [=]() {
        m_scene->removeItem(item);
        m_danmuItems.removeOne(item);
        delete item;
    });
}
void PlayerDialog::onRecvGift(const QString &giftMsg)
{
    // 显示弹幕
    addCommentToList(giftMsg, 0);

    // 播放动画
    QString imgPath;
    if (giftMsg.contains("火箭")) imgPath = ":/gift/rocket.png";
    else if (giftMsg.contains("玫瑰")) imgPath = ":/gift/flower.png";
    else if (giftMsg.contains("跑车")) imgPath = ":/gift/car.png";
    else if (giftMsg.contains("棒棒糖")) imgPath = ":/gift/lollipop.png";
    else if (giftMsg.contains("啤酒")) imgPath = ":/gift/beer.png";

    playGiftAnimation(imgPath);
}
void PlayerDialog::onAdminStatusChanged(bool isAdmin)
{
    Kernel* pKernel = Kernel::getInstance();
    bool isHost = (pKernel->m_loginUserId == pKernel->m_hostUserId);
    // 主播 或 房管 都启用按钮
    ui->btn_admin_manage->setEnabled(isHost || isAdmin);

    if(isHost || isAdmin)
        ui->btn_admin_manage->setToolTip("");
    else
        ui->btn_admin_manage->setToolTip("仅主播和房管可操作");

}
void PlayerDialog::updateInputState()
{
    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    bool isBanned = pKernel->isSelfBanned();
    // 禁言则禁用输入框+发送按钮，否则恢复
    ui->le_msg->setEnabled(!isBanned);
    ui->pb_send->setEnabled(!isBanned);

    if(isBanned)
    {
        ui->le_msg->setPlaceholderText("您已被禁言，无法输入...");
    }
    else
    {
        ui->le_msg->setPlaceholderText("请输入弹幕内容...");
    }
}

void PlayerDialog::on_pb_quit_room_clicked()
{
    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    // 1. 调用内核退出房间逻辑
    pKernel->quitRoom();

    // 2. 停止播放器
    if(m_player)
    {
        m_player->stop(true);
    }

    // 3. 清空聊天、在线列表UI
    ui->list_comment->clear();
    // 4. 提示
    QMessageBox::information(this, "提示", "已退出当前直播间");

}

// 打开直播大厅
void PlayerDialog::on_btn_RoomHall_clicked()
{
    qDebug() << __func__;

    // 单例模式，只创建一次（独立窗口，父窗口设为nullptr）
    if (m_roomHallWidget == nullptr)
    {
        m_roomHallWidget = new RoomHallWidget(nullptr);
        m_roomHallWidget->setWindowTitle("直播大厅");
        // 连接进入房间信号
        connect(m_roomHallWidget, &RoomHallWidget::sigJoinRoom,
                this, [this](unsigned long long roomNo) {
            qDebug() << "【PlayerDialog】从大厅进入房间:" << roomNo;
            // 关闭大厅窗口
            closeRoomHall();
            // 发送进房请求
            emit sigJoinRoom(roomNo);
        });
    }

    // 显示大厅窗口（独立弹出）
    m_roomHallWidget->show();
    m_roomHallWidget->raise();
    m_roomHallWidget->activateWindow();
}

// 关闭直播大厅
void PlayerDialog::closeRoomHall()
{
    if (m_roomHallWidget)
    {
        qDebug() << "【PlayerDialog】关闭直播大厅窗口";
        m_roomHallWidget->close();
        m_roomHallWidget->deleteLater();
        m_roomHallWidget = nullptr;
    }
}

