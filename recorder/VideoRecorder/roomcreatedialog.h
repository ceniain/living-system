#ifndef ROOMCREATEDIALOG_H
#define ROOMCREATEDIALOG_H

#include <QDialog>

namespace Ui {
class RoomCreateDialog;
}

class RoomCreateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RoomCreateDialog(QWidget *parent = nullptr);
    ~RoomCreateDialog();

signals:
    // 点击创建，把房间名发给 Kernel
    void sigCreateRoom(QString roomName);


private slots:
    void on_btn_create_clicked();   // 创建按钮
    void on_btn_cancel_clicked();   // 取消按钮

private:
    Ui::RoomCreateDialog *ui;
};

#endif // ROOMCREATEDIALOG_H
