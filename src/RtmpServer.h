#ifndef RTMP_SERVER_H
#define RTMP_SERVER_H

#include "StreamProcess.h"
#include "DataCache.h"
#include "libhttp/HttpDispatch.h"
#include <muduo/net/TcpServer.h>

class RtmpServer
{
public:
    RtmpServer(muduo::net::EventLoop *loop,
               const muduo::net::InetAddress &listenRtmp,
               const muduo::net::InetAddress &listenHttp);

    void Start();

private:
    void OnConnection(const muduo::net::TcpConnectionPtr &conn);

    void OnMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp time);

    void SendMessage(const muduo::net::TcpConnectionPtr &conn,
                     const char *data, size_t len);

    void RecvMessage(const muduo::net::TcpConnectionPtr &conn,
                     const PacketContext &packetContext, const void *info);

    void Play(StreamProcess *process, const AVMessage &message);

    void HandleStatus(const boost::shared_ptr<HttpRequester> &request,
                      boost::shared_ptr<HttpResponser> &response);

    static const int kTimeout = 10;

    DataCache m_dataCache;
    HttpDispatch m_httpDispatch;
    muduo::net::TcpServer m_server;
};

#endif // RTMP_SERVER_H
