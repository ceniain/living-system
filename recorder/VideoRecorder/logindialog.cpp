#include "ui_LoginDialog.h"
#include <QMessageBox>
#include "LoginDialog.h"
LoginDialog::LoginDialog(QWidget *parent, TcpClient *pTcp) :
    QDialog(parent),
    ui(new Ui::LoginDialog),
    m_pTcpClient(pTcp)
{
    ui->setupUi(this);
    setWindowTitle("主播登录/注册");
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

// 登录
void LoginDialog::on_btn_login_clicked()
{
    QString user = ui->le_user->text().trimmed();
    QString pwd = ui->le_pwd->text().trimmed();

    if (user.isEmpty() || pwd.isEmpty()) {
        QMessageBox::warning(this,"提示","账号或密码不能为空！");
        return;
    }

    // 保存用户名供后续使用
    m_lastLoginUser = user;
    // 发给 Kernel，不再自己发包
    emit sigLoginRequest(user, pwd);
}

// 注册
void LoginDialog::on_btn_reg_clicked()
{
    qDebug()<<"LoginDialog::on_btn_reg_clicked";
    QString user = ui->le_reg_user->text().trimmed();
    QString tel = ui->le_tel->text().trimmed();
    QString pwd1 = ui->le_reg_pwd->text().trimmed();
    QString pwd2 = ui->le_reg_pwd2->text().trimmed();

    if (user.isEmpty() || pwd1.isEmpty()) {
        QMessageBox::warning(this,"提示","账号密码不可为空");
        return;
    }
    if (pwd1 != pwd2) {
        QMessageBox::warning(this,"提示","两次输入密码不一致");
        return;
    }
    if(tel.isEmpty())
    {
        QMessageBox::warning(this,"提示","电话号码不能为空");
        return;
    }
    qDebug()<<"【Login】准备发射注册信号:"<<user<<pwd1<<tel;
    // 发给 Kernel
    emit sigRegisterRequest(user, tel,pwd1, pwd2);
    qDebug()<<"【Login】注册信号发射完毕";

}
// 注意：信号现在是 void sigRecvData(char* buf,int len)，槽参数必须对应
void LoginDialog::slotRecvServerData(char* buf, int len)
{

    PackType type = *(PackType*)buf;
    if(type == _DEF_PACK_REGISTER_RS)
    {
        STRU_REGISTER_RS* rsp = (STRU_REGISTER_RS*)buf;
        if(rsp->result == register_success)
        {
            QMessageBox::information(this,"提示","注册成功");
            ui->stackedWidget->setCurrentIndex(0);
        }
        else if(rsp->result == user_is_exist)
        {
            QMessageBox::warning(this,"提示","账号已存在");
        }
    }
    else if(type == _DEF_PACK_LOGIN_RS)
    {
        STRU_LOGIN_RS* rsp = (STRU_LOGIN_RS*)buf;
        if(rsp->result == login_success)
        {
            emit sigLoginSuccess(QString::number(rsp->userid));
            this->close();
        }
        else
        {
            QMessageBox::warning(this,"提示","账号密码错误");
        }
    }
}
// 切换页面
void LoginDialog::on_btn_to_reg_clicked() { ui->stackedWidget->setCurrentIndex(1); }
void LoginDialog::on_btn_to_login_clicked() { ui->stackedWidget->setCurrentIndex(0); }
