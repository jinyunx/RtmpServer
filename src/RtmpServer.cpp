#include "RtmpServer.h"

#include "CoSocket/net/Timer.h"
#include "CoSocket/base/SimpleLog.h"

#include <iostream>

RtmpServer::RtmpServer(TcpServer::ConnectorPtr &connector, DataCache &dataCache)
    : m_stop(false), m_playing(false), readTimeout(kTimeoutMs),
      writeTimeout(kTimeoutMs), m_connector(connector), m_dataCache(dataCache)
{
}

RtmpServer::~RtmpServer()
{
    Timer timer(m_connector->GetCoSocket());
    while (m_playing)
        timer.Wait(500);

    const char *role = 0;
    switch (m_streamProcess->GetRole())
    {
        case Role_Player:
            role = "Player";
            break;

        case Role_Publisher:
            role = "Publisher";
            break;

        case Role_Unknow:
        default:
            role = "Unknow";
            break;
    }
    SIMPLE_LOG("%s role exit", role);
}

void RtmpServer::operator() ()
{
    m_handShake.reset(new HandShake(
        std::bind(&RtmpServer::WriteResponse, this,
                  std::placeholders::_1, std::placeholders::_2)));

    m_streamProcess.reset(new StreamProcess(
        std::bind(&RtmpServer::WriteResponse, this,
                  std::placeholders:: _1, std::placeholders::_2)));

    m_streamProcess->SetOnChunkRecv(
        std::bind(&RtmpServer::HandleMessage, this,
                  std::placeholders:: _1, std::placeholders::_2));

    std::vector<char> buffer(1024);
    while(!m_stop)
    {
        int ret = m_connector->Read(buffer.data(), buffer.size(), readTimeout);
        if (ret <= 0)
        {
            SIMPLE_LOG("read failed, error: %d", ret);
            break;
        }

        if (!OnData(buffer.data(), ret))
            break;
    }
    CleanDataCache();
    Shutdown();
}

bool RtmpServer::OnData(const char *buffer, std::size_t bufferLength)
{
    m_buffer.append(buffer, bufferLength);

    if (!m_handShake->IsComplete())
    {
        int ret = m_handShake->Parse(&m_buffer[0], m_buffer.size());
        if (m_handShake->IsComplete())
            SIMPLE_LOG("Hand shake success.");

        if (ret < 0)
        {
            SIMPLE_LOG("Hand shake error.");
            return false;
        }

        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + ret);
    }

    if (m_handShake->IsComplete())
    {
        int ret = -1;
        do {
            ret = m_streamProcess->Process(const_cast<char *>(m_buffer.data()), m_buffer.size());
            m_streamProcess->Dump();

            if (ret < 0)
            {
                SIMPLE_LOG("Process message error.");
                return false;
            }

            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + ret);
        } while (ret > 0);
    }
    return true;
}

void RtmpServer::CleanDataCache()
{
    const std::string &app = m_streamProcess->GetApp();
    const std::string &streamName = m_streamProcess->GetStreamName();
    Role role = m_streamProcess->GetRole();
    if (role == Role_Player)
    {
        SIMPLE_LOG("Remove player from stream %s/%s", app.c_str(), streamName.c_str());
        m_dataCache.DeletePlayerQueue(app, streamName, m_playQueue);
    }
    else if (role == Role_Publisher)
    {
        SIMPLE_LOG("Remove stream %s/%s", app.c_str(), streamName.c_str());
        m_dataCache.DeleteStream(app, streamName);
    }

    SIMPLE_LOG("Clean data cache finished");
}

void RtmpServer::HandleMessage(const PacketContext &packet, const void *info)
{
    const std::string &app = m_streamProcess->GetApp();
    const std::string &streamName = m_streamProcess->GetStreamName();

    switch (packet.type)
    {
    case PacketType_MetaData:
    {
        SIMPLE_LOG("Recv meta data message");
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
            SIMPLE_LOG("Recv sps/pps message");
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
            SIMPLE_LOG("Recv aac sequence header message");
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
        SIMPLE_LOG("Add player to stream %s/%s", app.c_str(), streamName.c_str());

        m_playQueue.reset(new AVMessageQueue(m_connector->GetCoSocket()));
        if (!m_dataCache.AddPlayerQueue(app, streamName, m_playQueue))
        {
            SIMPLE_LOG("No stream %s/%s", app.c_str(), streamName.c_str());
            Shutdown();
        }

        m_connector->GetCoSocket().NewCoroutine(std::bind(&RtmpServer::Play, this));
        break;
    }

    default:
        break;
    }
}

void RtmpServer::Play()
{
    m_playing = true;
    while (!m_stop)
    {
        AVMessagePtr message = m_playQueue->Deque();
        switch (message->type)
        {
        case MessageType_Close:
            Shutdown();
            break;

        default:
            m_streamProcess->SendChunk(
                message->csId, message->msgHeader, &message->payload[0]);
            break;
        }
    }
    m_playing = false;
    SIMPLE_LOG("Player sub-routine exit");
}

ssize_t RtmpServer::WriteResponse(const char *buffer, size_t size)
{
    ssize_t ret = m_connector->WriteAll(buffer, size, writeTimeout);
    if (ret < 0)
    {
        SIMPLE_LOG("write failed, error: %d", ret);
        Shutdown();
    }

    return ret;
}

void RtmpServer::Shutdown()
{
    m_stop = true;
}
