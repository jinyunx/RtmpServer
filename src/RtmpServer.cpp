#include "RtmpServer.h"
#include "HandShake.h"
#include "StreamProcess.h"

class RtmpSession : public Session
{
public:
    RtmpSession(boost::asio::io_service &service,
                DataCache &dataCache)
        : Session(service), m_dataCache(dataCache),
          m_handShake(boost::bind(&RtmpSession::WriteResponse, this, _1, _2)),
          m_streamProcess(boost::bind(&RtmpSession::WriteResponse, this, _1, _2))
    {
        m_streamProcess.SetOnChunkRecv(boost::bind(
            &RtmpSession::HandleMessage, this, _1, _2));
    }

    ~RtmpSession()
    { }

private:
    virtual bool OnData(const char *buffer, std::size_t bufferLength)
    {
        m_buffer.append(buffer, bufferLength);

        if (!m_handShake.IsComplete())
        {
            int rt = m_handShake.Parse(&m_buffer[0], m_buffer.size());
            if (m_handShake.IsComplete())
                std::cerr << "Hand shake success." << std::endl;

            if (rt < 0)
            {
                std::cerr << "Handshake error" << std::endl;
                return false;
            }
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + rt);
        }

        if (m_handShake.IsComplete())
        {
            int rt = -1;
            do {
                rt = m_streamProcess.Process(const_cast<char *>(m_buffer.data()), m_buffer.size());
                m_streamProcess.Dump();

                if (rt < 0)
                {
                    std::cerr << "Process message error" << std::endl;
                    return false;
                }
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + rt);
            } while (rt > 0);
        }
        return true;
    }

    virtual void OnClose()
    {
        const std::string &app = m_streamProcess.GetApp();
        const std::string &streamName = m_streamProcess.GetStreamName();
        Role r = m_streamProcess.GetRole();
        if (r == Role_Player)
        {
            std::cerr << "Remove player " << GetRemoteIpString()
                << " from stream " << app << "/" << streamName << std::endl;
            m_dataCache.DeletePlayer(app, streamName, m_player);
        }
        else if (r == Role_Publisher)
        {
            std::cerr << "Remove stream " << app << "/" << streamName << std::endl;
            m_dataCache.DeleteStream(app, streamName);
        }

        std::cerr << "OnClose finished" << std::endl;
    }

    void HandleMessage(const PacketContext &packet, const void *info)
    {
        const std::string &app = m_streamProcess.GetApp();
        const std::string &streamName = m_streamProcess.GetStreamName();

        switch (packet.type)
        {
        case PacketType_MetaData:
        {
            std::cerr << "Recv meta data message" << std::endl;
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
                std::cerr << "Recv sps/pps message" << std::endl;
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
                std::cerr << "Recv aac sequence header message" << std::endl;
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
            std::cerr << "Add player " << GetRemoteIpString()
                << " to stream " << app << "/" << streamName << std::endl;
            m_player = boost::bind(&RtmpSession::Play, this, _1);
            if (!m_dataCache.AddPlayer(app, streamName, m_player))
            {
                std::cerr << "No stream " << app << "/" << streamName << std::endl;
                Shutdown();
            }
            break;
        }

        default:
            break;
        }
    }

    void Play(const AVMessage &message)
    {
        switch (message.type)
        {
        case MessageType_Close:
            Shutdown();
            break;

        default:
            m_streamProcess.SendChunk(message.csId, message.msgHeader,
                                      &message.payload[0]);
            break;
        }
    }

    DataCache &m_dataCache;
    HandShake m_handShake;
    StreamProcess m_streamProcess;
    Player m_player;
    std::string m_buffer;
};

SessionPtr NewSession(boost::asio::io_service &service,
                      DataCache &dataCache)
{
    return SessionPtr(new RtmpSession(service, dataCache));
}

RtmpServer::RtmpServer(boost::asio::io_service &service)
    : //m_httpDispatch(80, service),
      m_tcpServer(1935, service, boost::bind(
          &NewSession, boost::ref(service), boost::ref(m_dataCache)))
{
    //m_httpDispatch.AddHandler("/status", boost::bind(&RtmpServer::HandleStatus,
    //                                                 this, _1, _2));
}

void RtmpServer::Start()
{
    //m_httpDispatch.Go();
    m_tcpServer.Go();
}

void RtmpServer::HandleStatus(const HttpRequester &request,
                              HttpResponser &response)
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

    response.SetStatusCode(HttpResponser::StatusCode_200Ok);
    response.SetBody(body.str().c_str());
}

int main(int argc, char *argv[])
{
    std::cerr << "pid = " << getpid() << std::endl;
    boost::asio::io_service service;
    RtmpServer server(service);
    server.Start();
    service.run();
    return 0;
}
