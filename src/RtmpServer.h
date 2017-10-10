#ifndef RTMP_SERVER_H
#define RTMP_SERVER_H

#include <muduo/net/TcpServer.h>

class RtmpServer
{
public:
    RtmpServer(muduo::net::EventLoop *loop,
               const muduo::net::InetAddress &listenAddr);

    void Start();

private:
    void OnConnection(const muduo::net::TcpConnectionPtr &conn);

    void OnMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp time);
    void SendMessage(const muduo::net::TcpConnectionPtr &conn,
                     const char *data, size_t len);

    muduo::net::TcpServer m_server;
};

#endif // RTMP_SERVER_H
