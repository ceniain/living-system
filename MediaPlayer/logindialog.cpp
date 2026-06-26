#include "LoginDialog.h"
#include "ui_LoginDialog.h"
#include <QMessageBox>
#include <QDebug>

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle("观众登录");

    // 创建TCP客户端
    //m_tcp = new TcpClient(this);

    // 连接服务器（改成你服务端的IP和端口）
//    if (!m_tcp->connectServer("192.168.179.129", 8000)) {
//        QMessageBox::warning(this, "错误", "连接服务器失败！");
//    }

    // 绑定接收信号
    //connect(m_tcp, &TcpClient::sigRecvData, this, &LoginDialog::slotRecvData);

    // 密码隐藏
    ui->le_pwd->setEchoMode(QLineEdit::Password);
    ui->le_reg_pwd->setEchoMode(QLineEdit::Password);
    ui->le_reg_pwd2->setEchoMode(QLineEdit::Password);

    // 页面切换
    connect(ui->btn_to_reg, &QPushButton::clicked, this, &LoginDialog::slotToRegister);
    connect(ui->btn_to_login, &QPushButton::clicked, this, &LoginDialog::slotToLogin);

    // 按钮
    connect(ui->btn_login, &QPushButton::clicked, this, &LoginDialog::slotLogin);
    connect(ui->btn_reg, &QPushButton::clicked, this, &LoginDialog::slotRegister);

    // 默认显示登录页
    ui->stackedWidget->setCurrentIndex(0);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

// 去注册
void LoginDialog::slotToRegister()
{
    ui->stackedWidget->setCurrentIndex(1);
}

// 去登录
void LoginDialog::slotToLogin()
{
    ui->stackedWidget->setCurrentIndex(0);
}

// 登录按钮
void LoginDialog::slotLogin()
{
    QString name = ui->le_user->text().trimmed();
    QString pwd = ui->le_pwd->text().trimmed();

    if (name.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this, "提示", "账号或密码不能为空！");
        return;
    }

    // 封装协议
    STRU_LOGIN_RQ req;
    strcpy(req.name, name.toUtf8().data());
    strcpy(req.password, pwd.toUtf8().data());

    // 发送
    emit sigLoginRequest(name,pwd);
}

// 注册按钮
void LoginDialog::slotRegister()
{
    QString tel = ui->le_reg_tel->text().trimmed();
    QString name = ui->le_reg_user->text().trimmed();
    QString pwd1 = ui->le_reg_pwd->text().trimmed();
    QString pwd2 = ui->le_reg_pwd2->text().trimmed();

    if (tel.isEmpty() || name.isEmpty() || pwd1.isEmpty()) {
        QMessageBox::warning(this, "提示", "信息不能为空！");
        return;
    }
    if (pwd1 != pwd2) {
        QMessageBox::warning(this, "提示", "两次密码不一致！");
        return;
    }

    STRU_REGISTER_RQ req;
    strcpy(req.tel, tel.toUtf8().data());
    strcpy(req.name, name.toUtf8().data());
    strcpy(req.password, pwd1.toUtf8().data());

    //m_tcp->sendMsg((char*)&req, sizeof(req));
    emit sigRegisterRequest(name,tel,pwd1,pwd2);

}

//// 接收服务端消息
//void LoginDialog::slotRecvData(char *buf, int len)
//{
//    PackType type = *(PackType*)buf;

//    // 注册回复
//    if (type == _DEF_PACK_REGISTER_RS) {
//        STRU_REGISTER_RS* rsp = (STRU_REGISTER_RS*)buf;
//        if (rsp->result == register_success) {
//            QMessageBox::information(this, "成功", "注册成功，请登录！");
//            slotToLogin();
//        } else {
//            QMessageBox::warning(this, "失败", "注册失败，账号已存在！");
//        }
//    }

//    // 登录回复
//    else if (type == _DEF_PACK_LOGIN_RS) {
//        STRU_LOGIN_RS* rsp = (STRU_LOGIN_RS*)buf;
//        if (rsp->result == login_success) {
//            QMessageBox::information(this, "成功", "登录成功！");
//            this->accept(); // 关闭对话框，返回成功
//        } else if (rsp->result == user_not_exist) {
//            QMessageBox::warning(this, "失败", "用户不存在！");
//        } else if (rsp->result == password_error) {
//            QMessageBox::warning(this, "失败", "密码错误！");
//        }
//    }
//}
