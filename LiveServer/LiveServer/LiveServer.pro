QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS

# 源文件路径：src下所有cpp
SOURCES += \
        main.cpp \
        src/Mysql.cpp \
        src/TCPKernel.cpp \
        src/Thread_pool.cpp \
        src/block_epoll_net.cpp \
        src/clogic.cpp \
        src/err_str.cpp

# 头文件在 Headers/include 目录
HEADERS += \
    include/Mysql.h \
    include/TCPKernel.h \
    include/Thread_pool.h \
    include/block_epoll_net.h \
    include/clogic.h \
    include/err_str.h \
    include/packdef.h

# 头文件检索路径
INCLUDEPATH += $$PWD/Headers/include

# 链接库
LIBS += -lpthread -lmysqlclient

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
