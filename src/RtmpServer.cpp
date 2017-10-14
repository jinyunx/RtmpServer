#include "RtmpServer.h"
#include "HandShake.h"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/TimeZone.h>
#include <muduo/base/AsyncLogging.h>

struct Context
{
    HandShake handShake;
    StreamProcess streamProcess;
    Player player;

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

        context->streamProcess.SetOnChunkRecv(boost::bind(&RtmpServer::RecvMessage,
                                              this, conn, _1, _2));

        conn->setContext(context);
    }
    else
    {
        ContextPtr context = boost::any_cast<ContextPtr>(conn->getContext());
        const std::string &app = context->streamProcess.GetApp();
        const std::string &streamName = context->streamProcess.GetStreamName();
        Role r = context->streamProcess.GetRole();
        if (r == Role_Player)
        {
            LOG_INFO << "Remove player " << conn->peerAddress().toIpPort()
                     << "from stream " << app << "/" << streamName;
            m_dataCache.DeletePlayer(app, streamName, context->player);
        }
        else if (r == Role_Publisher)
        {
            LOG_INFO << "Remove stream " << app << "/" << streamName;
            m_dataCache.DeleteStream(app, streamName);
        }
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
            LOG_ERROR << "Handshake error";
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
                LOG_ERROR << "Process message error";
                conn->shutdown();
                conn->forceCloseWithDelay(3);
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

void RtmpServer::RecvMessage(const muduo::net::TcpConnectionPtr &conn,
                             const PacketContext &packet, const void *info)
{
    ContextPtr context = boost::any_cast<ContextPtr>(conn->getContext());
    const std::string &app = context->streamProcess.GetApp();
    const std::string &streamName = context->streamProcess.GetStreamName();

    switch (packet.type)
    {
    case PacketType_MetaData:
    {
        LOG_INFO << "Recv meta data message";
        m_dataCache.SetMetaData(app, streamName, packet.csId,
                                packet.headerDecoder.GetMsgHeader(),
                                &packet.payload[0]);
        break;
    }

    case PacketType_Video:
    {
        VideoInfo *vinfo = (VideoInfo *)info;
        if (vinfo->isSpspps)
        {
            LOG_INFO << "Recv sps/pps message";
            m_dataCache.SetSpspps(app, streamName, packet.csId,
                                  packet.headerDecoder.GetMsgHeader(),
                                  &packet.payload[0]);
        }
        else
        {
            m_dataCache.AddVideo(app, streamName, packet.csId,
                                 packet.headerDecoder.GetMsgHeader(),
                                 vinfo->isKeyFrame, &packet.payload[0]);
        }
        break;
    }

    case PacketType_Audio:
    {
        AudioInfo *ainfo = (AudioInfo *)info;
        if (ainfo->isSeqHeader)
        {
            LOG_INFO << "Recv aac sequence header message";
            m_dataCache.SetSeqheader(app, streamName, packet.csId,
                                     packet.headerDecoder.GetMsgHeader(),
                                     &packet.payload[0]);
        }
        else
        {
            m_dataCache.AddAudio(app, streamName, packet.csId,
                                 packet.headerDecoder.GetMsgHeader(),
                                 &packet.payload[0]);
        }
        break;
    }

    case PacketType_Play:
    {
        LOG_INFO << "Add player " << conn->peerAddress().toIpPort()
                 << " to stream " << app << "/" << streamName;
        context->player = boost::bind(&RtmpServer::Play, this,
                                      &context->streamProcess, _1);
        m_dataCache.AddPlayer(app, streamName, context->player);

        break;
    }

    default:
        break;
    }
}

void RtmpServer::Play(StreamProcess *process, const AVMessage &message)
{
    process->SendChunk(message.csId, message.msgHeader, &message.payload[0]);
}


boost::scoped_ptr<muduo::AsyncLogging> g_asyncLog;
const int kRollSize = 100 * 1000 * 1000;

void asyncOutput(const char* msg, int len)
{
    g_asyncLog->append(msg, len);
}

void setLogging(const char* argv0)
{
    muduo::TimeZone beijing(8 * 3600, "CST");
    muduo::Logger::setTimeZone(beijing);
    muduo::Logger::setOutput(asyncOutput);
    char name[256];
    strncpy(name, argv0, 256);
    g_asyncLog.reset(new muduo::AsyncLogging(::basename(name), kRollSize));
    g_asyncLog->start();
}

int main(int argc, char *argv[])
{
    setLogging(argv[0]);

    LOG_INFO << "pid = " << getpid();
    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(1935);
    RtmpServer server(&loop, listenAddr);
    server.Start();
    loop.loop();
    return 0;
}
