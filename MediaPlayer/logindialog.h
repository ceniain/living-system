#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "TcpClient.h"
#include "packdef.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
    void setTcpClient(TcpClient* pTcp){m_tcp = pTcp;}
signals:
    //登录信号：账号+密码
    void sigLoginRequest(QString userName, QString pwd);
    //注册信号：昵称+手机号+密码
    void sigRegisterRequest(QString userName, QString tel, QString pwd1, QString pwd2);
private slots:
    // 页面切换
    void slotToRegister();
    void slotToLogin();

    // 按钮
    void slotLogin();
    void slotRegister();

    // 接收服务端数据
    //void slotRecvData(char* buf, int len);

private:
    Ui::LoginDialog *ui;
    TcpClient* m_tcp; // 你的TCP类
};

#endif // LOGINDIALOG_H
