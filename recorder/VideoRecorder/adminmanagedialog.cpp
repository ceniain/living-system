#include "adminmanagedialog.h"
#include "ui_adminmanagedialog.h"
#include <QMessageBox>
#include <QString>
#include <QtEndian>

AdminManageDialog::AdminManageDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdminManageDialog)
{
    ui->setupUi(this);

    // 窗口基础设置
    this->setWindowTitle("房管管理");
    this->resize(450, 500);

    // 初始化容器
    m_adminUidList.clear();
    m_nickToUidMap.clear();

    // 输入框占位提示（UI里输入框objectName: le_target_name）
    ui->le_target_name->setPlaceholderText("请输入用户昵称");

    // ========== 信号槽绑定 ==========
    // 双击列表项，自动把昵称回填到输入框
    connect(ui->list_admin_user, &QListWidget::itemClicked, this, [=](QListWidgetItem* item)
            {
                if (!item) return;
                QString text = item->text();
                // 截取「昵称 (UID)」前面的昵称部分
                QString nick = text.left(text.indexOf(" ("));
                ui->le_target_name->setText(nick);
            });

    // 绑定Kernel房管同步信号，自动刷新列表
    Kernel* pKernel = Kernel::getInstance();
    connect(pKernel, &Kernel::sigAdminSync, this, &AdminManageDialog::slot_refresh_admin_list);
    connect(pKernel, &Kernel::sigOnlineUserSync, this, &AdminManageDialog::slot_refresh_admin_list);


    // 打开窗口立即刷新一次列表
    slot_refresh_admin_list();
}

AdminManageDialog::~AdminManageDialog()
{
    delete ui;
}

// 发送房管操作协议包
void AdminManageDialog::sendAdminOperate(int opType, int targetUid)
{
    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    STRU_SET_ADMIN_RQ req;

    req.opType    = opType;
    req.operUid   = pKernel->m_loginUserId;
    req.targetUid = targetUid;
    req.room_no   = pKernel->m_currentRoomNo;

    // 加调试日志，打印原始值和转换后的值
    qDebug() << "[Admin] 原始房间号：" << req.room_no;
    qDebug() << "[Admin] 转换前 hex：" << QString::number(req.room_no, 16);

    // 按你的协议要求，转成小端
    req.room_no = qToLittleEndian(req.room_no);

    qDebug() << "[Admin] 转换后房间号：" << req.room_no;
    qDebug() << "[Admin] 转换后 hex：" << QString::number(req.room_no, 16);

    // 补充操作人昵称
    strncpy(req.operName, pKernel->m_loginName.toStdString().c_str(), sizeof(req.operName) - 1);
    req.operName[sizeof(req.operName) - 1] = '\0';

    // 发送前再打印一次完整请求内容
    qDebug() << "[Admin] 发送设为房管请求，opType:" << opType
             << " targetUid:" << targetUid
             << " roomNo(LE):" << req.room_no;

    pKernel->m_pTcpClient->sendMsg((char*)&req, sizeof(req));
}


// 根据昵称查找在线用户UID
int AdminManageDialog::getUidByNickName(const QString& nickName)
{
    qDebug() << "[getUidByNickName] 要查找的昵称：" << nickName;
    if (m_nickToUidMap.contains(nickName))
    {
        qDebug() << "[getUidByNickName] 找到UID：" << m_nickToUidMap[nickName];
        return m_nickToUidMap[nickName];
    }
    qDebug() << "[getUidByNickName] 未找到用户，返回-1";
    return -1; // 未找到
}

// 【设为房管】按钮
void AdminManageDialog::on_btn_add_admin_clicked()
{
    QString nick = ui->le_target_name->text().trimmed();
    if (nick.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户昵称！");
        return;
    }

    int targetUid = getUidByNickName(nick);
    qDebug() << "[Admin] 输入昵称：" << nick << " 对应的UID：" << targetUid;
    if (targetUid == -1)
    {
        QMessageBox::warning(this, "提示", "未找到该在线用户，请检查昵称！");
        ui->le_target_name->clear();
        return;
    }

    // OP_ADD_ADMIN = 1 添加房管
    sendAdminOperate(OP_ADD_ADMIN, targetUid);
    ui->le_target_name->clear();
    QMessageBox::about(this, "提示", "设置房管成功！");
}

// 【移除房管】按钮
void AdminManageDialog::on_btn_del_admin_clicked()
{
    // 只从输入框获取昵称
    QString nick = ui->le_target_name->text().trimmed();
    if (nick.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入用户昵称！");
        return;
    }

    int targetUid = getUidByNickName(nick);
    qDebug() << "[Admin] 输入昵称：" << nick << " 对应的UID：" << targetUid;
    if (targetUid == -1)
    {
        QMessageBox::warning(this, "提示", "未找到该在线用户，请检查昵称！");
        ui->le_target_name->clear();
        return;
    }

    Kernel* pKernel = Kernel::getInstance();
    if (pKernel && !pKernel->getAdminList().contains(targetUid))
    {
        QMessageBox::warning(this, "提示", "该用户不是房管，无需移除！");
        return;
    }

    // OP_DEL_ADMIN = 2 移除房管
    sendAdminOperate(OP_DEL_ADMIN, targetUid);
    ui->le_target_name->clear();
    QMessageBox::about(this, "提示", "移除房管请求已发送！");
}


// 【返回】按钮：关闭当前窗口
void AdminManageDialog::on_btn_back_clicked()
{
    this->close();
}

// 刷新房管列表 + 构建昵称-UID映射
void AdminManageDialog::slot_refresh_admin_list()
{
    Kernel* pKernel = Kernel::getInstance();
    if (!pKernel) return;

    // 清空旧数据
    m_adminUidList.clear();
    m_nickToUidMap.clear();
    ui->list_admin_user->clear();

    int hostUid = pKernel->m_hostUserId;          // 主播UID
    QList<int> adminList = pKernel->m_adminList;   // 房管列表
    QList<int> userList = pKernel->m_onlineUserMap.keys(); // 全体在线用户UID列表

    // 1. 先展示 主播
    QString hostNick = pKernel->m_onlineUserMap.value(hostUid, "主播");
    m_nickToUidMap[hostNick] = hostUid;
    QString hostText = QString("【主播】%1 (%2)").arg(hostNick).arg(hostUid);
    new QListWidgetItem(hostText, ui->list_admin_user);

    // 2. 展示 房管
    m_adminUidList = adminList;
    for (int uid : adminList)
    {
        QString nick = pKernel->m_onlineUserMap.value(uid, "未知用户");
        m_nickToUidMap[nick] = uid;
        QString showText = QString("【房管】%1 (%2)").arg(nick).arg(uid);
        new QListWidgetItem(showText, ui->list_admin_user);
    }

    // 3. 展示 普通观众（排除主播、房管）
    for (int uid : userList)
    {
        if (uid == hostUid || adminList.contains(uid))
            continue;

        QString nick = pKernel->m_onlineUserMap.value(uid, "未知用户");
        m_nickToUidMap[nick] = uid;
        QString showText = QString("【观众】%1 (%2)").arg(nick).arg(uid);
        new QListWidgetItem(showText, ui->list_admin_user);
    }
}
