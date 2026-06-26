#ifndef ADMINMANAGEDIALOG_H
#define ADMINMANAGEDIALOG_H

#include <QDialog>
#include <QMap>
#include <QListWidgetItem>
#include "packdef.h"
#include "kernel.h"

namespace Ui {
class AdminManageDialog;
}

class AdminManageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminManageDialog(QWidget *parent = nullptr);
    ~AdminManageDialog();

private slots:
    void on_btn_add_admin_clicked();
    void on_btn_del_admin_clicked();
    void on_btn_back_clicked();
    // 刷新房管列表
    void slot_refresh_admin_list();

private:
    // 发送 增设/移除房管 协议包
    void sendAdminOperate(int opType, int targetUid);
    // 根据昵称匹配在线用户UID
    int getUidByNickName(const QString& nickName);

    Ui::AdminManageDialog *ui;
    QList<int>         m_adminUidList;    // 缓存房管UID
    QMap<QString, int> m_nickToUidMap;     // 昵称 -> UID 映射
};

#endif // ADMINMANAGEDIALOG_H
