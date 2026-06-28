#ifndef ROOMHALLWIDGET_H
#define ROOMHALLWIDGET_H

#include <QWidget>
#include "packdef.h"

namespace Ui {
class RoomHallWidget;
}

class RoomHallWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RoomHallWidget(QWidget *parent = nullptr);
    ~RoomHallWidget();

    // 主动刷新房间列表（外部调用/页面切换调用）
    void RefreshRoomList();

signals:
    // 用户选择进入房间
    void sigJoinRoom(unsigned long long roomNo);

private slots:
    // 搜索按钮点击
    void on_btn_Search_clicked();
    // 上一页
    void on_btn_Prev_clicked();
    // 下一页
    void on_btn_Next_clicked();
    // 列表行双击 → 进房间
    void on_table_RoomList_cellDoubleClicked(int row, int col);
    // 排序下拉框切换
    void on_cbox_Sort_currentIndexChanged(int index);
    // 接收Kernel房间列表信号
    void slot_OnRoomListResp(const STRU_GET_ROOM_LIST_RS& rsp);
    // 接收房间列表变更通知（新开播/下播自动刷新）
    void slot_OnRoomListUpdateNotify(int update_type);

private:
    Ui::RoomHallWidget *ui;

    // 分页参数
    int m_curPage;     // 当前页码
    int m_totalPage;   // 总页数
    const int m_pageSize = 10; // 每页10条

    // 排序类型 0默认 1升序 2降序
    int m_sortType;
    // 搜索关键词
    QString m_searchKey;

    // 组装并发送【获取房间列表】请求包
    void SendGetRoomListReq();
};

#endif // ROOMHALLWIDGET_H
