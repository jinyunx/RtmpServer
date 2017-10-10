#include "RtmpServer.h"
#include "HandShake.h"
#include "StreamProcess.h"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

struct Context
{
    HandShake handShake;
    StreamProcess streamProcess;

    Context(const OnS0S1S2 & onS0S1S2, const OnChunkSend &onChunkSend)
        : handShake(onS0S1S2), streamProcess(onChunkSend)
    { }
};

typedef boost::shared_ptr<Context> ContextPtr;

RtmpServer::RtmpServer(muduo::net::EventLoop *loop,
                       const muduo::net::InetAddress &listenAddr)
    : m_server(loop, listenAddr, "RtmpServer")
{
    m_server.setConnectionCallback(
        boost::bind(&RtmpServer::OnConnection, this, _1));
    m_server.setMessageCallback(
        boost::bind(&RtmpServer::OnMessage, this, _1, _2, _3));
}

void RtmpServer::Start()
{
    m_server.start();
}

void RtmpServer::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    LOG_INFO << "RtmpServer - " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
        ContextPtr context(new Context(boost::bind(&RtmpServer::SendMessage,
                                                   this, conn, _1, _2),
                                       boost::bind(&RtmpServer::SendMessage,
                                                   this, conn, _1, _2)));
        conn->setContext(context);
    }
}

void RtmpServer::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
    ContextPtr context = boost::any_cast<ContextPtr>(conn->getContext());

    if (!context->handShake.IsComplete())
    {
        std::string data(buf->peek(), buf->readableBytes());
        if (data.empty())
            return;

        int rt = context->handShake.Parse(&data[0], data.size());
        if (context->handShake.IsComplete())
            LOG_INFO << "Hand shake success!";

        if (rt < 0)
        {
            conn->shutdown();
            return;
        }
        else
            buf->retrieve(rt);
    }

    if (context->handShake.IsComplete())
    {
        int rt = -1;
        do {
            std::string data(buf->peek(), buf->readableBytes());
            if (data.empty())
                return;

            rt = context->streamProcess.Process(&data[0], data.size());
            context->streamProcess.Dump();

            if (rt < 0)
            {
                conn->shutdown();
                return;
            }
            else
                buf->retrieve(rt);
        } while (rt > 0);
    }
}

void RtmpServer::SendMessage(const muduo::net::TcpConnectionPtr &conn,
                             const char *data, size_t len)
{
    conn->send(data, len);
}

int main()
{
    LOG_INFO << "pid = " << getpid();
    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(1935);
    RtmpServer server(&loop, listenAddr);
    server.Start();
    loop.loop();
    return 0;
}
