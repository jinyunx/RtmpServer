#ifndef DATA_CACHE_H
#define DATA_CACHE_H
#include "boost/function.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>

typedef std::vector<std::string> Gop;
typedef boost::function<void (const char *, size_t)> Player;

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
    std::string meta;
    std::string spspps;
    PlayerSet players;
};

typedef std::map<std::string, StreamCache> StreamCacheMap;

class DataCache
{
public:
    void SetMetaData(const std::string &app, const std::string &streamName,
                     const char *data, size_t len);
    void SetSpsPps(const std::string &app, const std::string &streamName,
                   const char *data, size_t len);
    void AddVideo(const std::string &app, const std::string &streamName,
                  bool isKeyFrame, const char *data, size_t len);
    void AddAudio(const std::string &app, const std::string &streamName,
                  const char *data, size_t len);

    void DeleteStream(const std::string &app, const std::string &streamName);

    void AddPlayer(const std::string &app, const std::string &streamName,
                   const Player &player);
    void DeletePlayer(const std::string &app, const std::string &streamName,
                      const Player &player);

private:
    std::string GetAppStream(const std::string &app,
                             const std::string &streamName);
    void PushToPlayer(const std::string &app, const std::string &streamName,
                      const char *data, size_t len);

    StreamCacheMap m_streamCache;
};

#endif // DATA_CACHE_H
