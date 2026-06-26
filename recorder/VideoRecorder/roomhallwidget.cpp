#include "RoomHallWidget.h"
#include "ui_RoomHallWidget.h"
#include "kernel.h"
#include <cstring>
#include <QString>

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

    // ========= 核心：绑定 Kernel 的房间列表信号 =========
    connect(Kernel::getInstance(), &Kernel::sig_RoomListResp,
            this, &RoomHallWidget::slot_OnRoomListResp, Qt::UniqueConnection);

    // 页面加载自动请求第一页数据
    RefreshRoomList();
}

RoomHallWidget::~RoomHallWidget()
{
    delete ui;
}

// 对外刷新接口
void RoomHallWidget::RefreshRoomList()
{
    m_curPage = 1;
    m_searchKey = ui->edit_Search->text().trimmed();
    SendGetRoomListReq();
}

// 组装参数，调用 Kernel 发包
void RoomHallWidget::SendGetRoomListReq()
{
    qDebug()<<__func__;
    // 如需携带 页码、排序、关键词，可扩展 Kernel 接口
    // 这里统一交给 Kernel 发起网络请求
    Kernel::getInstance()->SendRoomListReq();
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

// 点击列表行 -> 进房间
void RoomHallWidget::on_table_RoomList_cellClicked(int row, int col)
{
    qDebug()<<__func__;
    QString roomNoStr = ui->table_RoomList->item(row, 0)->text();
    unsigned long long roomNo = roomNoStr.toULongLong();

    // 在这里调用 Kernel 发送【加入房间】请求 + 页面跳转
    // 示例：Kernel::getInstance()->sendJoinRoomReq(roomNo);
}

// 创建房间按钮
void RoomHallWidget::on_btn_CreateRoom_clicked()
{
    qDebug()<<__func__;
    // 调用 Kernel 原有创建房间逻辑
    Kernel::getInstance()->slot_OpenCreateRoomDialog();
}

// ===================== 接收Kernel信号，刷新UI表格 =====================
void RoomHallWidget::slot_OnRoomListResp(const STRU_GET_ROOM_LIST_RS &rsp)
{
    qDebug() << "===== 收到房间列表信号 =====";
    qDebug() << "cur_page:" << rsp.cur_page;
    qDebug() << "total_page:" << rsp.total_page;
    qDebug() << "item_cnt:" << rsp.item_cnt;
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
                                    new QTableWidgetItem(QString::fromLocal8Bit(item.room_name, sizeof(item.room_name))));
        ui->table_RoomList->setItem(newRow, 2,
                                    new QTableWidgetItem(QString::number(item.host_userid)));
        ui->table_RoomList->setItem(newRow, 3,
                                    new QTableWidgetItem(QString::number(item.online_count)));
    }

    ui->btn_Prev->setEnabled(m_curPage > 1);
    ui->btn_Next->setEnabled(m_curPage < m_totalPage);
}
