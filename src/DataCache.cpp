#include "DataCache.h"

void DataCache::SetMetaData(const std::string &app,  const std::string &streamName,
                            int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCache &streamCache = m_streamCache[appStream];
    if (!streamCache.meta)
        streamCache.meta.reset(new AVMessage);
    streamCache.meta->payload.clear();
    streamCache.meta->payload.append(data, msgHeader.length);
    streamCache.meta->csId = csId;
    streamCache.meta->msgHeader = msgHeader;

    PushToPlayerQueue(app, streamName, streamCache.meta);
}

void DataCache::SetSpspps(const std::string &app, const std::string &streamName,
                          int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCache &streamCache = m_streamCache[appStream];
    if (!streamCache.spspps)
        streamCache.spspps.reset(new AVMessage);
    streamCache.spspps->payload.clear();
    streamCache.spspps->payload.append(data, msgHeader.length);
    streamCache.spspps->csId = csId;
    streamCache.spspps->msgHeader = msgHeader;

    PushToPlayerQueue(app, streamName, streamCache.spspps);
}

void DataCache::SetSeqheader(const std::string &app, const std::string &streamName,
                             int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCache &streamCache = m_streamCache[appStream];
    if (!streamCache.seqheader)
        streamCache.seqheader.reset(new AVMessage);
    streamCache.seqheader->payload.clear();
    streamCache.seqheader->payload.append(data, msgHeader.length);
    streamCache.seqheader->csId = csId;
    streamCache.seqheader->msgHeader = msgHeader;

    PushToPlayerQueue(app, streamName, streamCache.seqheader);
}

void DataCache::AddVideo(const std::string &app, const std::string &streamName,
                         int csId, ChunkMsgHeader msgHeader, bool isKeyFrame,
                         const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    {
        AVMessagePtr message(new AVMessage);
        message->payload.append(data, msgHeader.length);
        message->csId = csId;
        message->msgHeader = msgHeader;

        if (isKeyFrame)
            m_streamCache[appStream].gop.clear();
        m_streamCache[appStream].gop.push_back(message);
    }

    PushToPlayerQueue(app, streamName, m_streamCache[appStream].gop.back());
}

void DataCache::AddAudio(const std::string &app, const std::string &streamName,
                         int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    {
        AVMessagePtr message(new AVMessage);
        message->payload.append(data, msgHeader.length);
        message->csId = csId;
        message->msgHeader = msgHeader;

        m_streamCache[appStream].gop.push_back(message);
    }

    PushToPlayerQueue(app, streamName, m_streamCache[appStream].gop.back());
}

void DataCache::DeleteStream(const std::string &app,
                             const std::string &streamName)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCacheMap::iterator it = m_streamCache.find(appStream);
    if (it != m_streamCache.end())
    {
        AVMessagePtr closeMessage(new AVMessage);
        closeMessage->type = MessageType_Close;
        PushToPlayerQueue(app, streamName, closeMessage);
        m_streamCache.erase(it);
    }
}

bool DataCache::AddPlayerQueue(const std::string &app, const std::string &streamName,
                               const AVMessageQueuePtr &playerQueue)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCacheMap::iterator it = m_streamCache.find(appStream);
    if (it == m_streamCache.end())
        return false;

    it->second.playerQueues.insert(playerQueue);

    if (it->second.meta)
        playerQueue->Enque(it->second.meta);

    if (it->second.spspps)
        playerQueue->Enque(it->second.spspps);

    if (it->second.seqheader)
        playerQueue->Enque(it->second.seqheader);

    for (size_t i = 0; i < it->second.gop.size(); ++i)
        playerQueue->Enque(it->second.gop[i]);

    return true;
}

void DataCache::DeletePlayerQueue(const std::string &app, const std::string &streamName,
                                  const AVMessageQueuePtr &playerQueue)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCacheMap::iterator it = m_streamCache.find(appStream);
    if (it == m_streamCache.end())
        return;

    AVMessagePtr closeMessage(new AVMessage);
    closeMessage->type = MessageType_Close;

    playerQueue->Enque(closeMessage);
    it->second.playerQueues.erase(playerQueue);
}

const StreamCacheMap & DataCache::GetStreamCache()
{
    return m_streamCache;
}

std::string DataCache::GetAppStream(const std::string &app,
                                    const std::string &streamName)
{
    return app + ":" + streamName;
}

void DataCache::PushToPlayerQueue(const std::string &app, const std::string &streamName,
                                  const AVMessagePtr &avMessage)
{
    std::string appStream = GetAppStream(app, streamName);
    PlayerQueueSet::iterator it = m_streamCache[appStream].playerQueues.begin();
    for (; it != m_streamCache[appStream].playerQueues.end(); ++it)
        (*it)->Enque(avMessage);
}
