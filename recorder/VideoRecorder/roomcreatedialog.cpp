#include "RoomCreateDialog.h"
#include "ui_RoomCreateDialog.h"
#include <QMessageBox>

RoomCreateDialog::RoomCreateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RoomCreateDialog)
{
    ui->setupUi(this);
    setWindowTitle("创建直播间");
}

RoomCreateDialog::~RoomCreateDialog()
{
    delete ui;
}

// 创建房间按钮
void RoomCreateDialog::on_btn_create_clicked()
{
    QString roomName = ui->le_rooName->text().trimmed();

    if (roomName.isEmpty()) {
        QMessageBox::warning(this, "提示", "房间名称不能为空！");
        return;
    }

    // 发送信号给 Kernel
    emit sigCreateRoom(roomName);
}

// 取消
void RoomCreateDialog::on_btn_cancel_clicked()
{
    this->close();
}
