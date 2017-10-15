#include "DataCache.h"

void DataCache::SetMetaData(const std::string &app,  const std::string &streamName,
                            int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].meta.payload.clear();
    m_streamCache[appStream].meta.payload.append(data, msgHeader.length);
    m_streamCache[appStream].meta.csId = csId;
    m_streamCache[appStream].meta.msgHeader = msgHeader;

    PushToPlayer(app, streamName, m_streamCache[appStream].meta);
}

void DataCache::SetSpspps(const std::string &app, const std::string &streamName,
                          int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].spspps.payload.clear();
    m_streamCache[appStream].spspps.payload.append(data, msgHeader.length);
    m_streamCache[appStream].spspps.csId = csId;
    m_streamCache[appStream].spspps.msgHeader = msgHeader;

    PushToPlayer(app, streamName, m_streamCache[appStream].spspps);
}

void DataCache::SetSeqheader(const std::string &app, const std::string &streamName,
                             int csId, ChunkMsgHeader msgHeader, const char *data)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].seqheader.payload.clear();
    m_streamCache[appStream].seqheader.payload.append(data, msgHeader.length);
    m_streamCache[appStream].seqheader.csId = csId;
    m_streamCache[appStream].seqheader.msgHeader = msgHeader;

    PushToPlayer(app, streamName, m_streamCache[appStream].seqheader);
}

void DataCache::AddVideo(const std::string &app, const std::string &streamName,
                         int csId, ChunkMsgHeader msgHeader, bool isKeyFrame,
                         const char *data)
{
    AVMessage message;
    message.payload.append(data, msgHeader.length);
    message.csId = csId;
    message.msgHeader = msgHeader;

    std::string appStream = GetAppStream(app, streamName);
    if (isKeyFrame)
        m_streamCache[appStream].gop.clear();
    m_streamCache[appStream].gop.push_back(message);

    PushToPlayer(app, streamName, message);
}

void DataCache::AddAudio(const std::string &app, const std::string &streamName,
                         int csId, ChunkMsgHeader msgHeader, const char *data)
{
    AVMessage message;
    message.payload.append(data, msgHeader.length);
    message.csId = csId;
    message.msgHeader = msgHeader;

    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].gop.push_back(message);

    PushToPlayer(app, streamName, message);
}

void DataCache::DeleteStream(const std::string &app,
                             const std::string &streamName)
{
    std::string appStream = GetAppStream(app, streamName);
    StreamCacheMap::iterator it = m_streamCache.find(appStream);
    if (it != m_streamCache.end())
        m_streamCache.erase(it);
}

void DataCache::AddPlayer(const std::string &app, const std::string &streamName,
                          const Player &player)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].players.insert(player);

    player(m_streamCache[appStream].meta);
    player(m_streamCache[appStream].spspps);
    player(m_streamCache[appStream].seqheader);

    for (size_t i = 0; i < m_streamCache[appStream].gop.size(); ++i)
        player(m_streamCache[appStream].gop[i]);
}

void DataCache::DeletePlayer(const std::string &app, const std::string &streamName,
                             const Player &player)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].players.erase(player);
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

void DataCache::PushToPlayer(const std::string &app, const std::string &streamName,
                             const AVMessage &avMessage)
{
    std::string appStream = GetAppStream(app, streamName);
    PlayerSet::iterator it = m_streamCache[appStream].players.begin();
    for (; it != m_streamCache[appStream].players.end(); ++it)
        (*it)(avMessage);
}
