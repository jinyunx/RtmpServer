#include "RtmpServer.h"
#include "HandShake.h"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/TimeZone.h>
#include <muduo/base/AsyncLogging.h>
#include <sstream>

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
                       const muduo::net::InetAddress &listenRtmp,
                       const muduo::net::InetAddress &listenHttp)
    : m_httpDispatch(kTimeout, loop, listenHttp, "RtmpServer"),
      m_server(loop, listenRtmp, "RtmpServer")
{
    m_httpDispatch.AddHander("/status", boost::bind(&RtmpServer::HandleStatus,
                                                    this, _1, _2));
    m_server.setConnectionCallback(
        boost::bind(&RtmpServer::OnConnection, this, _1));
    m_server.setMessageCallback(
        boost::bind(&RtmpServer::OnMessage, this, _1, _2, _3));
}

void RtmpServer::Start()
{
    m_httpDispatch.Start();
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
                     << " from stream " << app << "/" << streamName;
            m_dataCache.DeletePlayer(app, streamName, context->player);
            conn->setContext(boost::any());
        }
        else if (r == Role_Publisher)
        {
            LOG_INFO << "Remove stream " << app << "/" << streamName;
            m_dataCache.DeleteStream(app, streamName);
            conn->setContext(boost::any());
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
            Close(conn);
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
                Close(conn);
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
        context->player = boost::bind(&RtmpServer::Play, this, conn, _1);
        if (!m_dataCache.AddPlayer(app, streamName, context->player))
        {
            LOG_ERROR << "No stream " << app << "/" << streamName;
            Close(conn);
        }
        break;
    }

    default:
        break;
    }
}

void RtmpServer::Play(const muduo::net::TcpConnectionPtr &conn, const AVMessage &message)
{
    switch (message.type)
    {
    case MessageType_Close:
        Close(conn);
        break;

    default:
        ContextPtr context = boost::any_cast<ContextPtr>(conn->getContext());
        context->streamProcess.SendChunk(message.csId, message.msgHeader,
                                         &message.payload[0]);
        break;
    }
}

void RtmpServer::HandleStatus(const boost::shared_ptr<HttpRequester>& request,
                              boost::shared_ptr<HttpResponser>& response)
{
    const StreamCacheMap &cache = m_dataCache.GetStreamCache();

    std::ostringstream body;
    body << "{";
    StreamCacheMap::const_iterator it = cache.begin();
    for (; it != cache.end(); ++it)
    {
        body << "\n"
             << "stream name: " << it->first.c_str() << "\n"
             << "client num: " << it->second.players.size() << "\n";
    }
    body << "}";

    response->SetStatusCode(HttpResponser::StatusCode_200Ok);
    response->SetBody(body.str().c_str());
}

void RtmpServer::Close(const muduo::net::TcpConnectionPtr &conn)
{
    conn->shutdown();
    conn->forceCloseWithDelay(3);
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
    muduo::net::InetAddress listenRtmp(1935);
    muduo::net::InetAddress listenHttp(80);
    RtmpServer server(&loop, listenRtmp, listenHttp);
    server.Start();
    loop.loop();
    return 0;
}
