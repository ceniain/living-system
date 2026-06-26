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
    qDebug() << "[TcpClient::sendMsg] 原始数据长度 len = " << len;
    QByteArray sendBuf;
    sendBuf.append((char*)&len, 4); // 先发长度
    sendBuf.append(buf, len); // 再发数据

    qint64 writeRet = m_sock.write(sendBuf.data(), sendBuf.size());
    //m_sock.write(buf,len);
    qDebug() << "[TcpClient::sendMsg] write 返回字节数 = " << writeRet;
    if (m_sock.error() != QAbstractSocket::UnknownSocketError)
    {
        qDebug() << "[TcpClient ERROR] 错误码：" << m_sock.error()
                 << " 错误描述：" << m_sock.errorString();
    }

}
void TcpClient::slotReadData()
{
    qDebug()<<__func__;
    memset(m_recvBuf, 0, sizeof(m_recvBuf));
    m_recvLen = 0;
    int n = m_sock.read(m_recvBuf + m_recvLen, sizeof(m_recvBuf)-m_recvLen);
    if(n <=0) return;
    m_recvLen += n;

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

        //2、缓冲区数据不够一整包 → 等待下次recv
        if(m_recvLen < pkgTotal) break;

        //3、够整包：跳过包头4，把pkgTotal长度的body发给Kernel
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
        m_sock.close();
        m_sock.abort();
    }
}
