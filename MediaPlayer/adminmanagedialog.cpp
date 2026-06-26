#include "adminmanagedialog.h"
#include "kernel.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QRegExp>

AdminManageDialog::AdminManageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AdminManageDialog)
{
    ui->setupUi(this);

    // 初始全部禁用
    ui->btn_add_admin->setEnabled(false);
    ui->btn_del_admin->setEnabled(false);
    ui->btn_ban->setEnabled(false);
    ui->btn_unban->setEnabled(false);

    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    // 绑定列表刷新信号
    connect(pKernel, &Kernel::sigDataSync,
            this, &AdminManageDialog::slot_refresh_all_list);

    int selfUid = pKernel->getLoginUserId();
    int hostUid = pKernel->getHostUserId();
    bool isHost = (selfUid == hostUid);
    bool isAdmin = pKernel->isAdmin(selfUid);

    // 权限分配
    if (isHost)
    {
        // 主播：全开
        ui->btn_add_admin->setEnabled(true);
        ui->btn_del_admin->setEnabled(true);
        ui->btn_ban->setEnabled(true);
        ui->btn_unban->setEnabled(true);
    }
    else if (isAdmin)
    {
        // 房管：全开
        ui->btn_ban->setEnabled(true);
        ui->btn_unban->setEnabled(true);
        ui->btn_add_admin->setEnabled(true);
        ui->btn_del_admin->setEnabled(true);
    }

    // 打开窗口立即刷新列表
    slot_refresh_all_list();
}

AdminManageDialog::~AdminManageDialog()
{
    delete ui;
}
void AdminManageDialog::slot_refresh_all_list()
{
    qDebug()<<__func__;
    ui->list_all->clear();
    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    int hostUid = pKernel->getHostUserId();
    QList<int> adminList = pKernel->getAdminList();
    QList<int> onlineList = pKernel->getOnlineUserList();
    QList<int> banList = pKernel->getBanUserList();

    // 1. 主播
    QString hostText = QString("【主播】ID：%1  状态：正常").arg(hostUid);
    ui->list_all->addItem(hostText);

    // 2. 房管
    for (int uid : adminList)
    {
        QString state = banList.contains(uid) ? "已禁言" : "正常";
        QString item = QString("【房管】ID：%1  状态：%2").arg(uid).arg(state);
        ui->list_all->addItem(item);
    }

    // 3. 普通观众（排除主播、房管）
    for (int uid : onlineList)
    {
        if (uid == hostUid || adminList.contains(uid))
            continue;

        QString state = banList.contains(uid) ? "已禁言" : "正常";
        QString item = QString("【观众】ID：%1  状态：%2").arg(uid).arg(state);
        ui->list_all->addItem(item);
    }
}
void AdminManageDialog::on_btn_add_admin_clicked()
{
    qDebug()<<__func__;
    auto item = ui->list_all->currentItem();
    if (!item)
    {
        QMessageBox::warning(this, "提示", "请先选中一名用户");
        return;
    }

    // 正则解析ID
    QString text = item->text();
    QRegExp rx("ID：(\\d+)");
    int targetUid = 0;
    if (rx.indexIn(text) != -1)
    {
        targetUid = rx.cap(1).toInt();
    }

    qDebug() << "选中用户UID:" << targetUid;
    if (targetUid <= 0) return;

    Kernel* pKernel = Kernel::getInstance();
    int selfUid = pKernel->m_loginUserId;
    // 禁止操作自己
    if (targetUid == selfUid)
    {
        QMessageBox::warning(this, "提示", "不能将自己设为房管");
        return;
    }
    if (targetUid == pKernel->getHostUserId())
    {
        QMessageBox::warning(this, "提示", "无法对主播操作");
        return;
    }

    // 发送添加房管协议
    pKernel->sendAddAdminReq(targetUid);
}

void AdminManageDialog::on_btn_del_admin_clicked()
{
    qDebug()<<__func__;
    auto item = ui->list_all->currentItem();
    if (!item)
    {
        QMessageBox::warning(this, "提示", "请先选中一名房管");
        return;
    }

    // 正则解析ID
    QString text = item->text();
    QRegExp rx("ID：(\\d+)");
    int targetUid = 0;
    if (rx.indexIn(text) != -1)
    {
        targetUid = rx.cap(1).toInt();
    }

    qDebug() << "选中用户UID:" << targetUid;
    if (targetUid <= 0) return;

    Kernel* pKernel = Kernel::getInstance();
    int selfUid = pKernel->m_loginUserId;

    // 禁止移除自己
    if (targetUid == selfUid)
    {
        QMessageBox::warning(this, "提示", "不能移除自己");
        return;
    }

    if (targetUid == pKernel->getHostUserId())
    {
        QMessageBox::warning(this, "提示", "无法移除主播");
        return;
    }

    // 发送移除房管协议
    pKernel->sendDelAdminReq(targetUid);
}
void AdminManageDialog::on_btn_ban_clicked()
{
    qDebug() << "【点击禁言按钮】";
    auto item = ui->list_all->currentItem();
    if (!item)
    {
        QMessageBox::warning(this, "提示", "请先选中一名用户");
        return;
    }

    QString text = item->text();
    QRegExp rx("ID：(\\d+)"); // 匹配 ID：后面的数字
    int targetUid = 0;
    if (rx.indexIn(text) != -1)
    {
        targetUid = rx.cap(1).toInt();
    }

    qDebug() << "选中禁言用户UID:" << targetUid;
    if (targetUid <= 0) return;

    // ========== 新增权限校验 ==========
    Kernel* pKernel = Kernel::getInstance();
    int selfUid  = pKernel->m_loginUserId;
    int hostUid  = pKernel->m_hostUserId;
    bool isHost  = (selfUid == hostUid);
    bool targetIsAdmin = pKernel->isAdmin(targetUid);
    // 禁止禁言自己
    if (targetUid == selfUid)
    {
        QMessageBox::warning(this, "提示", "不能禁言自己");
        return;
    }

    // 禁止禁言主播
    if (targetUid == hostUid)
    {
        QMessageBox::warning(this, "提示", "无法对主播执行禁言");
        return;
    }
    // 非主播(房管) 禁止禁言其他房管
    if (!isHost && targetIsAdmin)
    {
        QMessageBox::warning(this, "提示", "房管不能禁言其他房管");
        return;
    }
    // =================================

    Kernel::getInstance()->sendBanReq(targetUid);
}
void AdminManageDialog::on_btn_unban_clicked()
{
    qDebug()<<__func__;
    auto item = ui->list_all->currentItem();
    if (!item)
    {
        QMessageBox::warning(this, "提示", "请先选中一名用户");
        return;
    }

    QString text = item->text();
    QRegExp rx("ID：(\\d+)");
    int targetUid = 0;
    if (rx.indexIn(text) != -1)
    {
        targetUid = rx.cap(1).toInt();
    }

    qDebug() << "选中解禁用户UID:" << targetUid;
    if (targetUid <= 0) return;

    // ========== 新增权限校验 ==========
    Kernel* pKernel = Kernel::getInstance();
    int selfUid  = pKernel->m_loginUserId;
    int hostUid  = pKernel->m_hostUserId;
    bool isHost  = (selfUid == hostUid);
    bool targetIsAdmin = pKernel->isAdmin(targetUid);

    // 禁止解禁自己
    if (targetUid == selfUid)
    {
        QMessageBox::warning(this, "提示", "不能解禁自己");
        return;
    }

    // 禁止解禁主播
    if (targetUid == hostUid)
    {
        QMessageBox::warning(this, "提示", "无法对主播执行解禁");
        return;
    }
    // 非主播(房管) 禁止解禁其他房管
    if (!isHost && targetIsAdmin)
    {
        QMessageBox::warning(this, "提示", "房管不能解禁其他房管");
        return;
    }
    // =================================

    Kernel::getInstance()->sendUnBanReq(targetUid);
}
