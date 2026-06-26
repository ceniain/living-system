#include "picturewidget.h"
#include "ui_picturewidget.h"
#include"QDebug"

PictureWidget::PictureWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PictureWidget)
{
    ui->setupUi(this);

    //FramelessWindowHint无边框WindowStaysOnTopHint一直在最上面
    this->setWindowFlags(Qt::FramelessWindowHint|
                         Qt::WindowStaysOnTopHint);

}

PictureWidget::~PictureWidget()
{
    delete ui;
}
//外部通过信号发一张照片过来在这里接受现实
void PictureWidget::slot_setImage(QImage img)
{
    qDebug() << "▶ slot_setImage...";
    QPixmap pixmap;
    if(!img.isNull())//判断照片是否为空
    {
        pixmap = QPixmap::fromImage(img.scaled(ui->lb_showImage->size()
                                                   ,Qt::KeepAspectRatio));
    }else{//图片为空直接转成pixmap
        pixmap=QPixmap::fromImage(img);
    }
    ui->lb_showImage->setPixmap(pixmap);
}
