#include "RoomHallWidget.h"
#include "ui_RoomHallWidget.h"
#include "Kernel.h"
#include <cstring>
#include <QString>
#include <QHeaderView>

RoomHallWidget::RoomHallWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::RoomHallWidget)
{
    ui->setupUi(this);

    // 1. 初始化分页、排序、搜索状态
    m_curPage = 1;
    m_totalPage = 0;
    m_sortType = 0;
    m_searchKey.clear();

    // 2. 初始化排序下拉框
    ui->cbox_Sort->addItem("默认排序", 0);
    ui->cbox_Sort->addItem("在线人数升序", 1);
    ui->cbox_Sort->addItem("在线人数降序", 2);

    // 3. 设置表格列宽
    ui->table_RoomList->setColumnWidth(0, 120); // 房间号
    ui->table_RoomList->setColumnWidth(1, 150); // 房间名
    ui->table_RoomList->setColumnWidth(2, 80);  // 主播ID
    ui->table_RoomList->setColumnWidth(3, 80);  // 在线人数

    // 4. 设置表格不可编辑
    ui->table_RoomList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 设置选择行为为整行选择
    ui->table_RoomList->setSelectionBehavior(QAbstractItemView::SelectRows);

    // ========= 核心：绑定 Kernel 的房间列表信号 =========
    bool conn1 = connect(Kernel::getInstance(), &Kernel::sig_RoomListResp,
            this, &RoomHallWidget::slot_OnRoomListResp, Qt::UniqueConnection);
    qDebug() << "[RoomHall] sig_RoomListResp 连接结果:" << conn1;

    // 绑定房间列表变更通知（新开播/下播自动刷新）
    bool conn2 = connect(Kernel::getInstance(), &Kernel::sigRoomListUpdateNotify,
            this, &RoomHallWidget::slot_OnRoomListUpdateNotify, Qt::UniqueConnection);
    qDebug() << "[RoomHall] sigRoomListUpdateNotify 连接结果:" << conn2;

    // 页面加载自动请求第一页数据
    qDebug() << "[RoomHall] 构造函数调用 RefreshRoomList";
    RefreshRoomList();
}

RoomHallWidget::~RoomHallWidget()
{
    delete ui;
}

// 对外刷新接口
void RoomHallWidget::RefreshRoomList()
{
    qDebug() << "===== 【UI】RefreshRoomList 被调用 =====";
    m_curPage = 1;
    m_searchKey = ui->edit_Search->text().trimmed();
    qDebug() << "【UI】搜索框当前文本:" << ui->edit_Search->text();
    qDebug() << "【UI】m_searchKey:" << m_searchKey;
    SendGetRoomListReq();
}

// 组装参数，调用 Kernel 发包
void RoomHallWidget::SendGetRoomListReq()
{
    qDebug() << "===== 【UI】发送房间列表请求 =====";
    qDebug() << "【UI】page:" << m_curPage << "sort:" << m_sortType << "key:" << m_searchKey;
    // 传递UI状态到Kernel
    Kernel::getInstance()->SendRoomListReq(m_curPage, 10, m_sortType, m_searchKey);
}

// ===================== UI 交互槽函数 =====================
void RoomHallWidget::on_btn_Search_clicked()
{
    qDebug()<<__func__;
    RefreshRoomList();
}

void RoomHallWidget::on_cbox_Sort_currentIndexChanged(int index)
{
    qDebug()<<__func__;
    m_sortType = index;
    RefreshRoomList();
}

void RoomHallWidget::on_btn_Prev_clicked()
{
    qDebug()<<__func__;
    if (m_curPage > 1)
    {
        m_curPage--;
        SendGetRoomListReq();
    }
}

void RoomHallWidget::on_btn_Next_clicked()
{
    qDebug()<<__func__;
    if (m_curPage < m_totalPage)
    {
        m_curPage++;
        SendGetRoomListReq();
    }
}

// 双击列表行 -> 进房间
void RoomHallWidget::on_table_RoomList_cellDoubleClicked(int row, int col)
{
    qDebug()<<__func__ << "row:" << row << "col:" << col;
    QString roomNoStr = ui->table_RoomList->item(row, 0)->text();
    unsigned long long roomNo = roomNoStr.toULongLong();

    // 发送进入房间信号
    emit sigJoinRoom(roomNo);
}

// ===================== 接收Kernel信号，刷新UI表格 =====================
void RoomHallWidget::slot_OnRoomListResp(const STRU_GET_ROOM_LIST_RS &rsp)
{
    qDebug() << "===== 【UI】收到房间列表信号 =====";
    qDebug() << "【UI】cur_page:" << rsp.cur_page;
    qDebug() << "【UI】total_page:" << rsp.total_page;
    qDebug() << "【UI】item_cnt:" << rsp.item_cnt;
    if(rsp.item_cnt > 0)
    {
        qDebug() << "第0条 room_no:" << rsp.items[0].room_no;
        qDebug() << "第0条 room_name原始char:" << rsp.items[0].room_name;
        QString testName = QString::fromLocal8Bit(rsp.items[0].room_name, sizeof(rsp.items[0].room_name));
        qDebug() << "转换后room_name:" << testName;
        qDebug() << "online_count:" << rsp.items[0].online_count;
    }
    qDebug() << "============================";

    m_totalPage = rsp.total_page;
    m_curPage   = rsp.cur_page;

    ui->lab_PageInfo->setText(QString("当前 %1 / 总 %2 页")
                                  .arg(m_curPage)
                                  .arg(m_totalPage));

    ui->table_RoomList->setRowCount(0);

    if (rsp.item_cnt <= 0)
    {
        ui->btn_Prev->setEnabled(false);
        ui->btn_Next->setEnabled(false);
        return;
    }

    for (int i = 0; i < rsp.item_cnt; ++i)
    {
        const RoomItem& item = rsp.items[i];
        int newRow = ui->table_RoomList->rowCount();
        ui->table_RoomList->insertRow(newRow);
        qDebug() << "插入行号：" << newRow;

        ui->table_RoomList->setItem(newRow, 0,
                                    new QTableWidgetItem(QString::number(item.room_no)));
        ui->table_RoomList->setItem(newRow, 1,
                                    new QTableWidgetItem(QString::fromUtf8(item.room_name, sizeof(item.room_name))));
        ui->table_RoomList->setItem(newRow, 2,
                                    new QTableWidgetItem(QString::number(item.host_userid)));
        ui->table_RoomList->setItem(newRow, 3,
                                    new QTableWidgetItem(QString::number(item.online_count)));
    }

    ui->btn_Prev->setEnabled(m_curPage > 1);
    ui->btn_Next->setEnabled(m_curPage < m_totalPage);
}

// 接收房间列表变更通知（新开播/下播自动刷新）
void RoomHallWidget::slot_OnRoomListUpdateNotify(int update_type)
{
    qDebug() << "[RoomHall] 收到大厅刷新通知 update_type=" << update_type
             << (update_type == 1 ? "(新开播)" : "(下播)");
    // 自动刷新房间列表
    RefreshRoomList();
}
