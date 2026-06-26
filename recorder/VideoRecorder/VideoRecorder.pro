QT       += core gui
QT       += multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    TcpClient.cpp \
    adminmanagedialog.cpp \
    audio_read.cpp \
    kernel.cpp \
    logindialog.cpp \
    main.cpp \
    picinpic_read.cpp \
    picturewidget.cpp \
    recorderdialog.cpp \
    roomcreatedialog.cpp \
    roomhallwidget.cpp \
    savevideofilethread.cpp

HEADERS += \
    TcpClient.h \
    adminmanagedialog.h \
    audio_read.h \
    common.h \
    kernel.h \
    logindialog.h \
    packdef.h \
    picinpic_read.h \
    picturewidget.h \
    recorderdialog.h \
    roomcreatedialog.h \
    roomhallwidget.h \
    savevideofilethread.h

FORMS += \
    RoomHallWidget.ui \
    adminmanagedialog.ui \
    logindialog.ui \
    picturewidget.ui \
    recorderdialog.ui \
    roomcreatedialog.ui \
    roomhallwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$PWD/ffmpeg-4.2.2/include\
                 D:/MediaPlayer/recorder/VideoRecorder/opencv-release/include/opencv2\
                 D:/MediaPlayer/recorder/VideoRecorder/opencv-release/include\

LIBS += $$PWD/ffmpeg-4.2.2/lib/avcodec.lib\
         $$PWD/ffmpeg-4.2.2/lib/avdevice.lib\
         $$PWD/ffmpeg-4.2.2/lib/avfilter.lib\
         $$PWD/ffmpeg-4.2.2/lib/avformat.lib\
         $$PWD/ffmpeg-4.2.2/lib/avutil.lib\
         $$PWD/ffmpeg-4.2.2/lib/postproc.lib\
         $$PWD/ffmpeg-4.2.2/lib/swresample.lib\
         $$PWD/ffmpeg-4.2.2/lib/swscale.lib
LIBS+= D:\MediaPlayer\recorder\VideoRecorder\opencv-release\lib\libopencv_*.dll.a
LIBS += -lws2_32

RESOURCES += \
    res.qrc
