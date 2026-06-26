#include "recorderdialog.h"
#include "LoginDialog.h"
#include "kernel.h"   // 必须加
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ===================== 1. 启动 Kernel（核心！自动创建TCP+登录窗口） =====================
    Kernel* pKernel = Kernel::getInstance();
    pKernel->init(); // 建立信号槽 + 显示登录界面

    // ===================== 2. 监听登录成功信号 =====================
//    RecorderDialog w;
//    QObject::connect(pKernel, &Kernel::sigLoginSuccess, [&]() {
//        w.show();       // 登录成功 → 显示主界面
//    });
    int ret = a.exec();
    if (pKernel!= nullptr)
    {
        delete pKernel;
        pKernel = nullptr;
        qDebug() << "【main】已释放Kernel单例，析构函数已执行";
    }

    return ret;
}
//*************************************
//#include "recorderdialog.h"

//#include <QApplication>

//int main(int argc, char *argv[])
//{
//    QApplication a(argc, argv);
//    RecorderDialog w;
//    w.show();
//    return a.exec();
//}
