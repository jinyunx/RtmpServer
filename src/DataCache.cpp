#include "DataCache.h"

void DataCache::SetMetaData(const std::string &app,
                            const std::string &streamName,
                            const char *data, size_t len)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].meta.clear();
    m_streamCache[appStream].meta.append(data, len);

    PushToPlayer(app, streamName, data, len);
}

void DataCache::SetSpsPps(const std::string &app,
                          const std::string &streamName,
                          const char *data, size_t len)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].spspps.clear();
    m_streamCache[appStream].spspps.append(data, len);

    PushToPlayer(app, streamName, data, len);
}

void DataCache::AddVideo(const std::string &app,
                         const std::string &streamName,
                         bool isKeyFrame, const char *data, size_t len)
{
    std::string appStream = GetAppStream(app, streamName);
    if (isKeyFrame)
        m_streamCache[appStream].gop.clear();
    m_streamCache[appStream].gop.push_back(std::string(data, len));

    PushToPlayer(app, streamName, data, len);
}

void DataCache::AddAudio(const std::string &app,
                         const std::string &streamName,
                         const char *data, size_t len)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].gop.push_back(std::string(data, len));

    PushToPlayer(app, streamName, data, len);
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

    if (m_streamCache[appStream].meta.size())
        player(&m_streamCache[appStream].meta[0], m_streamCache[appStream].meta.size());
    if (m_streamCache[appStream].spspps.size())
        player(&m_streamCache[appStream].spspps[0], m_streamCache[appStream].spspps.size());

    for (size_t i = 0; i < m_streamCache[appStream].gop.size(); ++i)
    {
        if (m_streamCache[appStream].gop[i].size())
            player(&m_streamCache[appStream].gop[i][0], m_streamCache[appStream].gop[i].size());
    }
}

void DataCache::DeletePlayer(const std::string &app, const std::string &streamName,
                             const Player &player)
{
    std::string appStream = GetAppStream(app, streamName);
    m_streamCache[appStream].players.erase(player);
}

std::string DataCache::GetAppStream(const std::string &app,
                                    const std::string &streamName)
{
    return app + ":" + streamName;
}

void DataCache::PushToPlayer(const std::string &app, const std::string &streamName,
                             const char *data, size_t len)
{
    std::string appStream = GetAppStream(app, streamName);
    PlayerSet::iterator it = m_streamCache[appStream].players.begin();
    for (; it != m_streamCache[appStream].players.end(); ++it)
        (*it)(data, len);
}
