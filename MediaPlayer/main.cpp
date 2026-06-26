#include "playerdialog.h"
#include"Kernel.h"
#include <QApplication>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include"libswresample/swresample.h"
#include"libavutil/time.h"
#include "SDL.h"
}

#include <iostream>
using namespace std;
#undef main
//int main(int argc, char *argv[])
//{
//    QApplication a(argc, argv);
//    PlayerDialog w;
//    w.show();

//    //这里简单的输出一个版本号
//    cout << "Hello FFmpeg!" << endl;
//    av_register_all();//注册
//    unsigned version = avcodec_version();//编解码器
//    cout << "version is:" << version << endl;
//    return a.exec();
//}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PlayerDialog* w = nullptr; //先空指针，不实例化
    Kernel* pKernel = Kernel::getInstance();

    QObject::connect(pKernel,&Kernel::sigLoginSuccess,[&](){
        w = new PlayerDialog; //登录成功再堆上创建
        w->show();
    });

    pKernel->init();

    cout << "Hello FFmpeg!" << endl;
    av_register_all();
    unsigned version = avcodec_version();
    cout << "version is:" << version << endl;

    int ret = a.exec();
    if (w != nullptr)
    {
        delete w;
        w = nullptr;
    }
    if (pKernel)
    {
        delete pKernel;          // 触发 ~Kernel()
        pKernel = nullptr;
    }
    //delete w; //释放
    return ret;
}
