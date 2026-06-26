#include <QCoreApplication>
#include <iostream>
#include "./include/TCPKernel.h"
using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    printf("1.main begin\n");

    TcpKernel* pKernel = TcpKernel::GetInstance();
    printf("2.获取单例成功\n");

    int ret = pKernel->Open(_DEF_PORT);
    printf("【MAIN】Open返回值 = %d\n",ret);

    if(ret != TRUE)
    {
        printf("服务启动失败，退出main\n");
        return -1;
    }
    printf("✅服务启动成功，准备调用EventLoop\n");
    pKernel->EventLoop();
    printf("⚠️EventLoop执行完毕退出\n");

    return a.exec();
}
