#ifndef RTMP_SERVER_H
#define RTMP_SERVER_H

#include "HandShake.h"
#include "StreamProcess.h"
#include "DataCache.h"

#include "CoSocket/net/TcpServer.h"

#include <unistd.h>
#include <string>
#include <memory>

class RtmpServer
{
public:
    RtmpServer(TcpServer::ConnectorPtr &connector, DataCache &dataCache);
    ~RtmpServer();

    void operator ()();

private:
    bool OnData(const char *buffer, std::size_t bufferLength);
    void CleanDataCache();
    void HandleMessage(const PacketContext &packet, const void *info);
    void Play();
    ssize_t WriteResponse(const char *buffer, size_t size);
    void Shutdown();

    static const int kTimeoutMs = 60000;

    bool m_stop;
    bool m_playing;
    int readTimeout;
    int writeTimeout;
    TcpServer::ConnectorPtr m_connector;
    DataCache &m_dataCache;
    std::unique_ptr<HandShake> m_handShake;
    std::unique_ptr<StreamProcess> m_streamProcess;
    AVMessageQueuePtr m_playQueue;
    std::string m_buffer;
};

#endif // RTMP_SERVER_H
