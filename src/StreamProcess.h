#ifndef STREAM_PROCESS_H
#define STREAM_PROCESS_H

#include "RtmpHeader.h"
#include "boost/function.hpp"
#include <string>

enum PacketType
{
    PacketType_Connect,
    PacketType_FCPublish,
    PacketType_CreateStream,
    PacketType_Publish,
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

struct FCPublishCommand
{
    std::string name;
    int transactionId;
    std::string streamName;
};

struct CreateStreamCommand
{
    std::string name;
    int transactionId;
};

struct PublishCommand
{
    std::string name;
    int transactionId;
    std::string streamName;
    std::string app;
};

class StreamProcess
{
public:
    typedef boost::function<void(const PacketMeta &, const void *)> OnChunkRecv;
    typedef boost::function<void(const char *, size_t)> OnChunkSend;

    int Process(char *data, size_t len);
    void SetOnChunkRecv(const OnChunkRecv &onChunkRecv);
    void Dump();

private:
    bool Amf0Decode(char *data, size_t len, PacketMeta &meta);

    bool ConnectDecode(char *data, size_t len,
                       const std::string &name,
                       int transactionId);
    void OnConnect(const PacketMeta &meta,
                   const ConnectCommand &command);

    bool FCPublishDecode(char *data, size_t len,
                         const std::string &name,
                         int transactionId);
    void OnFCPublish(const PacketMeta &meta,
                     const FCPublishCommand &command);

    bool CreateStreamDecode(char *data, size_t len,
                            const std::string &name,
                            int transactionId);
    void OnCreateStream(const PacketMeta &meta,
                        const CreateStreamCommand &command);

    bool PublishDecode(char *data, size_t len,
                       const std::string &name,
                       int transactionId);
    void OnPublish(const PacketMeta &meta,
                   const PublishCommand &command);

    void SetWinAckSize();
    void SetPeerBandwidth();
    void SetChunkSize();

    static const int kWinAckSize = 2500000;
    static const int kChunkSize = 60000;

    RtmpHeaderDecode m_headerDecoder;
    RtmpHeaderEncode m_headerEncoder;

    OnChunkRecv m_onChunkRecv;
    OnChunkSend m_onChunkSend;

    ConnectCommand m_connectCommand;
    FCPublishCommand m_fcpublishCommand;
    CreateStreamCommand m_csCommand;
    PublishCommand m_publishCommand;
};

#endif // STREAM_PROCESS_H
