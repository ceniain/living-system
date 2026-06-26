#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include "packdef.h"

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    //连接服务端 IP=Ubuntu虚拟机IP 端口8000
    bool connectServer(const QString& ip, quint16 port);
    //发送协议包
    void sendMsg(char* buf, int len);
    //获取连接状态
    bool isConnected() const
    {
        return m_sock.state() == QAbstractSocket::ConnectedState;
    }
    void disconnectFromServer();


signals:
    //收到服务端数据，抛信号给登录界面
     void sigRecvData(char* buf, int len);
    //void sigRecvData(const QByteArray &data);
     void sigDisconnected(char* buf, int len);
private slots:
    void slotReadData();

private:
    QTcpSocket m_sock;
    char m_recvBuf[4096]={0};
    int m_recvLen = 0;
public:
    char* getRecvBuf(){ return m_recvBuf; }

};

#endif // TCPCLIENT_H
