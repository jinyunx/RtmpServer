#ifndef STREAM_PROCESS_H
#define STREAM_PROCESS_H

#include "RtmpHeader.h"
#include "boost/function.hpp"
#include <string>

enum PacketType
{
    PacketType_Connect,
    PacketType_Video,
    PacketType_Audio,
};

struct PacketMeta
{
    ChunkBasicHeader basicHeader;
    ChunkMsgHeader msgHeader;
    ExtendedTimestamp extendedTimestamp;
    PacketType type;
};

struct ConnectCommand
{
    std::string name;
    int transactionId;
    std::string app;
    std::string tcUrl;
};

class StreamProcess
{
public:
    typedef boost::function<void(const PacketMeta &, void *)> OnChunkRev;
    typedef boost::function<void(const char *, size_t)> OnChunkSend;

    int Process(char *data, size_t len);
    void Dump();

private:
    bool Amf0Decode(char *data, size_t len, PacketType *type);
    bool ConnectCommandDecode(char *data, size_t len,
                              const std::string &name,
                              int transactionId);

    bool m_first;
    RtmpHeaderDecode m_headerDecoder;
    ChunkMsgHeader m_lastMsgHeader;
    ChunkBasicHeader m_lastBasicHeader;
    bool m_lastHasExtended;

    ConnectCommand m_connectCommand;
};

#endif // STREAM_PROCESS_H
