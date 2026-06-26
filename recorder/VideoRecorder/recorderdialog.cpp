#include "recorderdialog.h"
#include "ui_recorderdialog.h"
#include <QMessageBox>
#include <QLabel>
#include <QPropertyAnimation>
#include"kernel.h"
#include <QWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

//rtmp://192.168.179.129/videotest/user=100
RecorderDialog::RecorderDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RecorderDialog)
{
    ui->setupUi(this);
     qDebug() << "=== 主界面已加载 ===";  // 加这行
     // ===================== 加调试输出 =====================
     static int s_id = 0;
     m_myId = s_id++;
     qDebug() << "=== 新建 RecorderDialog 窗口，编号：" << m_myId;
     // ======================================================

    m_pictureWidget=new PictureWidget;
    m_pictureWidget->hide();
    m_pictureWidget->move(0,0);

    this->setWindowFlags(Qt::WindowMinMaxButtonsHint|
                         Qt::WindowCloseButtonHint );
    //=====初始化弹幕画布（QGraphicsView透明悬浮在视频上方）=====
    m_scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setStyleSheet("background-color:transparent;");
    //隐藏滚动条
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //=====评论列表平滑滚动设置=====
    // 1. 视频最底层
    ui->lb_showimage->lower();

    // 2. 弹幕层在中间
    ui->graphicsView->stackUnder(ui->wdg_effect_layer); // 弹幕在礼物下面

    // 3. 礼物特效层最顶层
    ui->wdg_effect_layer->raise();
    //ui->wdg_effect_layer->raise();
    ui->list_comment->addItem("测试弹幕");
    ui->list_comment->setVerticalScrollMode(QListWidget::ScrollPerPixel);
    // 特效层透明
    ui->wdg_effect_layer->setAttribute(Qt::WA_TranslucentBackground);
    ui->wdg_effect_layer->setAttribute(Qt::WA_NoSystemBackground);

    // 绑定接收礼物信号
    connect(Kernel::getInstance(), &Kernel::sigRecvGift, this, &RecorderDialog::onRecvGift);
    m_saveFileThread=new SaveVideoFileThread;
    m_saveFileThread->start();

    connect(m_saveFileThread, SIGNAL(SIG_sendPicInPic(QImage)),
            m_pictureWidget, SLOT(slot_setImage(QImage)));
    connect(m_saveFileThread, SIGNAL(SIG_sendVideoFrame(QImage)),
            this, SLOT(slot_setImage(QImage)));
    // 绑定弹幕/评论更新（主播和观众共用）
    connect(Kernel::getInstance(), &Kernel::sigRecvComment,
            this, &RecorderDialog::slotAddSelfComment);
    // 连接下播关闭信号
    connect(Kernel::getInstance(), &Kernel::sigRoomClosed,
            this, &RecorderDialog::onRoomClosed);
    connect(ui->btn_admin_manage, &QPushButton::clicked,
            Kernel::getInstance(), &Kernel::slot_open_admin_dialog);




}

RecorderDialog::~RecorderDialog()
{
    delete ui;


}

void RecorderDialog::setRoomNumber(QString no)
{
    m_roomNo = no;
    qDebug()<<no;
    ui->lb_roomno->setText("直播间号：" + m_roomNo);
    //disconnect(Kernel::getInstance(), nullptr, this, nullptr); // 先断开旧连接
    connect(Kernel::getInstance(), &Kernel::sigRoomCountUpdate,
            this, [this](int count) {
                qDebug() << "当前窗口更新在线人数：" << count;
                ui->label_online->setText(QString("在线人数：%1").arg(count));
            });

    //自动生成推流地址
    QString rtmp = QString("rtmp://192.168.179.129/videotest/user=%1").arg(m_roomNo);
    ui->le_url->setText(rtmp);

}

void RecorderDialog::sendDanmu(const QString &text, bool isHost)
{
    if (text.isEmpty()) return;
    QGraphicsTextItem *item = new QGraphicsTextItem(text);
    item->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));

    // 🔥 主播 → 黄色
    if (isHost)
        item->setDefaultTextColor(Qt::yellow);
    else
        item->setDefaultTextColor(Qt::white);

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


void RecorderDialog::on_pb_start_clicked()
{
    this->showMinimized();
    m_pictureWidget->show();

    STRU_AV_FORMAT format;
    //format.clear();
    format.fileName = m_saveUrl;
    format.frame_rate = FRAME_RATE;
    format.hasAudio = true;
    format.hasCamera = true;
    format.hasDesk = true;
    format.videoBitRate = 4000000;
    //采样频率 44100
    //码率 64000
    //声道 2
    //精度位数16
    //... fltp aac....

    QScreen *src = QApplication::primaryScreen();
    QRect rect = src->geometry();
    format.width = rect.width();
    format.height = rect.height();

    m_saveFileThread->slot_setInfo( format );
    m_saveFileThread->slot_openVideo();
}


void RecorderDialog::on_pb_stop_clicked()
{
//    m_pictureWidget->hide();
//    m_saveFileThread->slot_closeVideo();
    QMessageBox::StandardButton btn = QMessageBox::question(
        this,
        "确认下播",       // 标题
        "确定要结束直播吗？", // 提示文字
        QMessageBox::Yes | QMessageBox::No
        );

    // 如果用户点 否 → 直接返回，不下播
    if (btn == QMessageBox::No) {
        return;
    }

    // 用户点 是 → 执行原来的下播逻辑
    m_pictureWidget->hide();
    m_saveFileThread->slot_closeVideo();

    // 发送下播请求给服务端
    Kernel::getInstance()->sendStopLiveRequest();

}
void RecorderDialog::onRoomClosed()
{
    qDebug() << "RecorderDialog::onRoomClosed 执行下播逻辑";
    // ===================== 加调试输出 =====================
    qDebug() << "========= onRoomClosed 来自窗口编号：" << m_myId;
    // ======================================================


    // 清空弹幕
    m_scene->clear();
    m_danmuItems.clear();

    // 提示
    QMessageBox::information(this, "提示", "已成功下播！");

    // 关闭主播端窗口
    this->close();
}

void RecorderDialog::on_pb_setUrl_clicked()
{
    //ui->pb_start->setEnabled(true);
    m_saveUrl=ui->le_url->text();
   // m_saveUrl=m_saveUrl.replace("/","\\");
    qDebug()<<"直播地址设置成功";
}
//外部通过信号发一张照片过来在这里接受现实
void RecorderDialog::slot_setImage(QImage img)
{

//    QPixmap pixmap;
//    if(!img.isNull())//判断照片是否为空
//    {
//        pixmap = QPixmap::fromImage(img.scaled(ui->lb_showimage->size()
//                                                   ,Qt::KeepAspectRatio));
//    }else{//图片为空直接转成pixmap
//        pixmap=QPixmap::fromImage(img);
//    }
//    ui->lb_showimage->setPixmap(pixmap);*****************
    QPixmap pixmap;
    if(!img.isNull())
    {
        // 使用Label实时大小 + 等比例填充，补充平滑缩放
        pixmap = QPixmap::fromImage(img.scaled(ui->lb_showimage->size(),
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else
    {
        pixmap = QPixmap::fromImage(img);
    }
    ui->lb_showimage->setPixmap(pixmap);


}


void RecorderDialog::on_pb_comment_clicked()
{
    qDebug() << "RecorderDialog::on_pb_comment_clicked";
    QString msg = ui->le_msg->text().trimmed();
    if (msg.isEmpty()) return;

    //QString showMsg = "主播：" + msg;

//    slotAddSelfComment(showMsg);

    // 发给服务器
    Kernel::getInstance()->sendComment(msg);

    ui->le_msg->clear();
}

void RecorderDialog::slotAddSelfComment(const QString &msg,int userid)
{
    qDebug() << "【UI】收到评论：" << msg << " userid:" << userid;

    // 列表显示
    ui->list_comment->addItem(msg);
    ui->list_comment->scrollToBottom();
    // 弹幕飘屏（和观众端完全一样）
    bool isHost = (userid == Kernel::getInstance()->m_loginUserId);

    sendDanmu(msg, isHost); // 传给弹幕颜色判断


}
QImage RecorderDialog::whiteToTransparent(const QImage &src)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); y++) {
        QRgb *row = (QRgb*)img.scanLine(y);
        for (int x = 0; x < img.width(); x++) {
            QRgb p = row[x];
            if (qRed(p) > 220 && qGreen(p) > 220 && qBlue(p) > 220) {
                row[x] = qRgba(0, 0, 0, 0);
            }
        }
    }
    return img;
}

QImage RecorderDialog::cropTransparent(const QImage &src)
{
    int l = src.width(), r = 0, t = src.height(), b = 0;
    for (int y = 0; y < src.height(); y++) {
        QRgb *row = (QRgb*)src.scanLine(y);
        for (int x = 0; x < src.width(); x++) {
            if (qAlpha(row[x]) > 0) {
                l = qMin(l, x);
                r = qMax(r, x);
                t = qMin(t, y);
                b = qMax(b, y);
            }
        }
    }
    if (l > r || t > b) return src;
    return src.copy(l, t, r-l+1, b-t+1);
}

void RecorderDialog::playGiftAnimation(const QString &imgPath)
{
    QWidget *layer = ui->wdg_effect_layer;
    QLabel *lab = new QLabel(layer);

    lab->setAttribute(Qt::WA_DeleteOnClose);
    lab->setAttribute(Qt::WA_TranslucentBackground);
    lab->setStyleSheet("background:transparent; border:none; padding:0; margin:0;");

    QPixmap pix(imgPath);
    if (!pix.isNull()) {
        QImage img = pix.toImage();
        img = whiteToTransparent(img);
        img = cropTransparent(img);
        pix = QPixmap::fromImage(img);
        pix = pix.scaled(120,120,Qt::KeepAspectRatio,Qt::FastTransformation);
        lab->setPixmap(pix);
        lab->setFixedSize(pix.size());
    }

    int x = (layer->width() - lab->width()) / 2;
    int y = layer->height() / 2;
    lab->move(x, y);
    lab->show();

    QParallelAnimationGroup *g = new QParallelAnimationGroup(lab);

    QPropertyAnimation *a1 = new QPropertyAnimation(lab, "geometry");
    a1->setDuration(3000);
    a1->setStartValue(lab->geometry());
    a1->setKeyValueAt(0.3, QRect(x-30, y-30, lab->width()*1.5, lab->height()*1.5));
    a1->setEndValue(QRect(x, -200, lab->width(), lab->height()));

    QPropertyAnimation *a2 = new QPropertyAnimation(lab, "windowOpacity");
    a2->setDuration(3000);
    a2->setStartValue(1);
    a2->setEndValue(0);

    g->addAnimation(a1);
    g->addAnimation(a2);
    connect(g, &QParallelAnimationGroup::finished, lab, &QLabel::close);
    g->start();
}
void RecorderDialog::onRecvGift(const QString &giftMsg)
{
    // ----------------------
    // 第一步：把礼物消息加到聊天列表
    // ----------------------
    qDebug() << "【主播】收到礼物啦：" << giftMsg;
    ui->list_comment->addItem(giftMsg);
    ui->list_comment->scrollToBottom(); // 自动滚动到底部

    // ----------------------
    // 第二步：识别礼物，播放对应动画
    // ----------------------
    QString path = ":/gift/rocket.png";

    if (giftMsg.contains("玫瑰")) path = ":/gift/flower.png";
    else if (giftMsg.contains("跑车")) path = ":/gift/car.png";
    else if (giftMsg.contains("棒棒糖")) path = ":/gift/sugar.png";
    else if (giftMsg.contains("啤酒")) path = ":/gift/beer.png";

    playGiftAnimation(path);
}


void RecorderDialog::on_btn_admin_manage_clicked()
{
    emit sigOpenAdminDialog();


}

