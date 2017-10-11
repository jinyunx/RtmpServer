#ifndef STREAM_PROCESS_H
#define STREAM_PROCESS_H

#include "RtmpHeader.h"
#include "Amf0Helper.h"
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

enum Stage
{
    Stage_Header,
    Stage_Body,
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

typedef boost::function<void(const PacketMeta &, const void *)> OnChunkRecv;
typedef boost::function<void(const char *, size_t)> OnChunkSend;

class StreamProcess
{
public:
    StreamProcess(const OnChunkSend &onChunkSend);

    // 0 Not enough data
    // -1 Error
    // +N size data to parse
    int Process(char *data, size_t len);
    void SetOnChunkRecv(const OnChunkRecv &onChunkRecv);
    void Dump();

private:
    size_t GetNeedLength(size_t body);
    char *MergeChunk(std::string &buf, char *data, size_t len);

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

    void SendChunk(int csId, int typeId, unsigned int timestamp,
                   int streamId, char *data, size_t len);

    void SetWinAckSize();
    void SetPeerBandwidth();
    void SetChunkSize();

    void ResponseResult(int txId, const AMFValue &reply = AMFValue(),
                        const AMFValue &status = AMFValue());

    static const int kWinAckSize = 2500000;
    static const int kSendChunkSize = 60000;
    static const int kRecvChunkSize = 128;
    static const int kMaxHeaderBytes = 3 + 11;

    Stage m_stage;

    size_t m_sendChunkSize;
    size_t m_revcChunkSize;

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
