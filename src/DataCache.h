#ifndef DATA_CACHE_H
#define DATA_CACHE_H
#include "RtmpHeader.h"
#include "boost/function.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>

struct AVMessage
{
    int csId;
    ChunkMsgHeader msgHeader;
    std::string payload;

    AVMessage()
        : csId(0)
    { }
};

typedef std::vector<AVMessage> Gop;
typedef boost::function<void(const AVMessage &)> Player;

struct Compare
{
    bool operator() (const Player &lhs, const Player &rhs) const
    {
        return &lhs < &rhs;
    }
};

typedef std::set<Player, Compare> PlayerSet;

struct StreamCache
{
    Gop gop;
    AVMessage meta;
    AVMessage spspps;
    AVMessage seqheader;
    PlayerSet players;
};

typedef std::map<std::string, StreamCache> StreamCacheMap;

class DataCache
{
public:
    void SetMetaData(const std::string &app, const std::string &streamName,
                     int csId, ChunkMsgHeader msgHeader, const char *data);
    void SetSpspps(const std::string &app, const std::string &streamName,
                   int csId, ChunkMsgHeader msgHeader, const char *data);
    void SetSeqheader(const std::string &app, const std::string &streamName,
                      int csId, ChunkMsgHeader msgHeader, const char *data);
    void AddVideo(const std::string &app, const std::string &streamName,
                  int csId, ChunkMsgHeader msgHeader, bool isKeyFrame,
                  const char *data);
    void AddAudio(const std::string &app, const std::string &streamName,
                  int csId, ChunkMsgHeader msgHeader, const char *data);

    void DeleteStream(const std::string &app, const std::string &streamName);

    void AddPlayer(const std::string &app, const std::string &streamName,
                   const Player &player);
    void DeletePlayer(const std::string &app, const std::string &streamName,
                      const Player &player);

private:
    std::string GetAppStream(const std::string &app,
                             const std::string &streamName);
    void PushToPlayer(const std::string &app, const std::string &streamName,
                      const AVMessage &avMessage);

    StreamCacheMap m_streamCache;
};

#endif // DATA_CACHE_H