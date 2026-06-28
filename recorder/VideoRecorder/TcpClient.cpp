#include "TcpClient.h"
#include <cstring>
#include <QMessageBox>
#include <QDebug>

TcpClient::TcpClient(QObject *parent) : QObject(parent)
{
    memset(m_recvBuf, 0, sizeof(m_recvBuf)); // 构造时清空缓冲区
    m_recvLen = 0;
    connect(&m_sock,&QTcpSocket::readyRead,this,&TcpClient::slotReadData);
}

bool TcpClient::connectServer(const QString &ip, quint16 port)
{
    m_sock.connectToHost(ip,port);
    bool ret = m_sock.waitForConnected(3000);
    if(!ret)
    {
        return false;
    }
    return true;
}

// 发送：你原来的代码完全正确，不动
void TcpClient::sendMsg(char* buf, int len)
{
    QByteArray sendBuf;
    sendBuf.append((char*)&len, 4); // 先发长度
    sendBuf.append(buf, len); // 再发数据

    m_sock.write(sendBuf.data(), sendBuf.size());
    //m_sock.write(buf,len);
}
void TcpClient::slotReadData()
{
    qDebug()<<__func__;
    // 修复：不能每次清空缓冲区！TCP 是流式协议，可能分包到达
    // 只有第一次接收时才需要清空（构造函数已清空）
    int n = m_sock.read(m_recvBuf + m_recvLen, sizeof(m_recvBuf)-m_recvLen);
    if(n <=0) return;
    m_recvLen += n;
    qDebug()<<"【TCP】本次接收"<<n<<"字节，缓冲区总长度"<<m_recvLen;

    while (m_recvLen >= 4)
    {
        qDebug()<<"全量缓冲区前8字节:"
                 <<hex<<(int)m_recvBuf[0]
                 <<(int)m_recvBuf[1]
                 <<(int)m_recvBuf[2]
                 <<(int)m_recvBuf[3];
        // 取出 4 字节长度
        //1、从buf头部取bodyLen
        int bodyLen = *(int*)m_recvBuf;
        int pkgTotal = 4 + bodyLen;
        qDebug()<<"【TCP】解析包 bodyLen="<<bodyLen<<"pkgTotal="<<pkgTotal;

        //2、缓冲区数据不够一整包 → 等待下次recv
        if(m_recvLen < pkgTotal) {
            qDebug()<<"【TCP】数据不足，等待下次接收";
            break;
        }

        //3、够整包：跳过包头4，把pkgTotal长度的body发给Kernel
        qDebug()<<"【TCP】发送完整包给Kernel，长度"<<bodyLen;
        emit sigRecvData(m_recvBuf+4, bodyLen);

        //4、剩下的数据前移到缓冲区开头，**必须memmove，剩余后面清零**
        m_recvLen -= pkgTotal;
        if(m_recvLen > 0)
        {
            memmove(m_recvBuf, m_recvBuf+pkgTotal, m_recvLen);
            memset(m_recvBuf+m_recvLen,0,_DEF_BUFFER - m_recvLen);
        }
        else
        {
            memset(m_recvBuf,0,_DEF_BUFFER);
        }
    }
}
void TcpClient::disconnectFromServer()
{
    if (m_sock.state() == QAbstractSocket::ConnectedState)
    {
        m_sock.close(); // 用QTcpSocket标准方法关闭
        m_sock.waitForDisconnected(3000); // 等待断开完成
        if (m_sock.state() != QAbstractSocket::UnconnectedState)
        {
            m_sock.abort(); // 超时强制关闭
        }
    }
}
