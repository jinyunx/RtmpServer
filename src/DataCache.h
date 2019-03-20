#ifndef DATA_CACHE_H
#define DATA_CACHE_H
#include "RtmpHeader.h"
#include "CoSocket/net/CoQueue.h"
#include <functional>
#include <string>
#include <vector>
#include <set>
#include <map>

enum MessageType
{
    MessageType_Data,
    MessageType_Close,
};

struct AVMessage
{
    MessageType type;
    int csId;
    ChunkMsgHeader msgHeader;
    std::string payload;

    AVMessage()
        : type(MessageType_Data), csId(0)
    { }
};

typedef CoQueue<AVMessage> AVMessageQueue;
typedef std::shared_ptr<AVMessageQueue> AVMessageQueuePtr;
typedef AVMessageQueue::DataPtr AVMessagePtr;

typedef std::vector<AVMessagePtr> Gop;
typedef std::set<AVMessageQueuePtr> PlayerQueueSet;

struct StreamCache
{
    Gop gop;
    AVMessagePtr meta;
    AVMessagePtr spspps;
    AVMessagePtr seqheader;
    PlayerQueueSet playerQueues;
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

    bool AddPlayerQueue(const std::string &app, const std::string &streamName,
                        const AVMessageQueuePtr &player);
    void DeletePlayerQueue(const std::string &app, const std::string &streamName,
                           const AVMessageQueuePtr &player);

    const StreamCacheMap &GetStreamCache();

private:
    std::string GetAppStream(const std::string &app,
                             const std::string &streamName);
    void PushToPlayerQueue(const std::string &app, const std::string &streamName,
                           const AVMessagePtr &avMessage);

    StreamCacheMap m_streamCache;
};

#endif // DATA_CACHE_H
