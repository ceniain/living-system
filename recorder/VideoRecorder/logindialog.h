#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTcpSocket>
// 引入协议头，你的packdef.h放到项目include目录
#include "packdef.h"

namespace Ui {
class LoginDialog;
}

// 前置声明TCP客户端类
class TcpClient;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    // 新增第二个参数：tcp对象，用来收发网络，兼容你原有无参构造
    explicit LoginDialog(QWidget *parent = nullptr, TcpClient* pTcp = nullptr);
    ~LoginDialog();

    // 获取登录用户名
    QString GetUserName() const { return m_lastLoginUser; }

    // 登录成功信号：传给主界面，关闭登录弹窗、打开创建直播间界面
signals:
    void sigLoginSuccess(QString strUserId);
    void sigLoginRequest(QString user, QString pwd);
    void sigRegisterRequest(QString user, QString tel,QString pwd1, QString pwd2);

private slots:
    void on_btn_to_reg_clicked();

    void on_btn_login_clicked();

    void on_btn_reg_clicked();

    void on_btn_to_login_clicked();

    // 新增：接收服务端返回数据槽函数
    void slotRecvServerData(char* buf, int len);

private:
    Ui::LoginDialog *ui;
    TcpClient* m_pTcpClient = nullptr; // 保存tcp管理对象
    QString m_lastLoginUser; // 保存最后一次登录的用户名
};

#endif // LOGINDIALOG_H
