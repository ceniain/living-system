#ifndef ADMINMANAGEDIALOG_H
#define ADMINMANAGEDIALOG_H

#include <QDialog>
#include "ui_adminmanagedialog.h"

class Kernel;

class AdminManageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdminManageDialog(QWidget *parent = nullptr);
    ~AdminManageDialog();

    // 刷新全量列表（身份+禁言状态）
    void slot_refresh_all_list();

private slots:
    // 房管管理
    void on_btn_add_admin_clicked();
    void on_btn_del_admin_clicked();

    // 禁言管理
    void on_btn_ban_clicked();
    void on_btn_unban_clicked();

private:
    Ui::AdminManageDialog *ui;
};

#endif // ADMINMANAGEDIALOG_H
