#ifndef STREAM_PROCESS_H
#define STREAM_PROCESS_H

#include "RtmpHeader.h"
#include "Amf0Helper.h"
#include "boost/function.hpp"
#include <string>

enum PacketType
{
    PacketType_NONE,
    PacketType_Connect,
    PacketType_FCPublish,
    PacketType_CreateStream,
    PacketType_Publish,
    PacketType_Play,
    PacketType_MetaData,
    PacketType_Video,
    PacketType_Audio,
};

enum Stage
{
    Stage_Header,
    Stage_Body,
};

enum Role
{
    Role_Unknow,
    Role_Publisher,
    Role_Player,
};

struct PacketContext
{
    int csId;
    Stage stage;
    PacketType type;
    RtmpHeaderDecode headerDecoder;
    std::string payload;

    PacketContext()
        : csId(0), stage(Stage_Header),
          type(PacketType_NONE)
    { }
};

struct VideoInfo
{
    bool isKeyFrame;
    bool isSpspps;

    VideoInfo()
        : isKeyFrame(false), isSpspps(false)
    { }
};

struct AudioInfo
{
    bool isSeqHeader;

    AudioInfo()
        : isSeqHeader(false)
    { }
};

struct ConnectCommand
{
    std::string name;
    int transactionId;
    std::string app;
    std::string tcUrl;

    ConnectCommand()
        : transactionId(0)
    { }
};

struct FCPublishCommand
{
    std::string name;
    int transactionId;
    std::string streamName;

    FCPublishCommand()
        : transactionId(0)
    { }
};

struct CreateStreamCommand
{
    std::string name;
    int transactionId;

    CreateStreamCommand()
        : transactionId(0)
    { }
};

struct PublishCommand
{
    std::string name;
    int transactionId;
    std::string app;
    std::string streamName;

    PublishCommand()
        : transactionId(0)
    { }
};

struct PlayCommand
{
    std::string name;
    int transactionId;
    std::string streamName;

    PlayCommand()
        : transactionId(0)
    { }
};

// CS ID to PacketContext
typedef std::map<int, PacketContext> PacketContextMap;

typedef boost::function<void(const PacketContext &, const void *)> OnChunkRecv;
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

    void SendChunk(int csId, ChunkMsgHeader msgHeader, const char *data);

    const std::string &GetApp();
    const std::string &GetStreamName();
    Role GetRole();

private:
    bool Dispatch(PacketContext &context);
    size_t GetNeedLength(size_t body);

    bool Amf0Decode(char *data, size_t len, PacketContext &context);

    bool ConnectDecode(char *data, size_t len,
                       const std::string &name,
                       int transactionId);
    void OnConnect(const PacketContext &context,
                   const ConnectCommand &command);

    bool FCPublishDecode(char *data, size_t len,
                         const std::string &name,
                         int transactionId);
    void OnFCPublish(const PacketContext &context,
                     const FCPublishCommand &command);

    bool CreateStreamDecode(char *data, size_t len,
                            const std::string &name,
                            int transactionId);
    void OnCreateStream(const PacketContext &context,
                        const CreateStreamCommand &command);

    bool PublishDecode(char *data, size_t len,
                       const std::string &name,
                       int transactionId);
    void OnPublish(const PacketContext &context,
                   const PublishCommand &command);

    bool PlayDecode(char *data, size_t len,
                    const std::string &name,
                    int transactionId);
    void OnPlay(const PacketContext &context,
                const PlayCommand &command);

    void OnMetaData(const PacketContext &context,
                    const char *data, size_t len);
    void OnVideo(const PacketContext &context,
                 const char *data, size_t len);
    void OnAudio(const PacketContext &context,
                 const char *data, size_t len);

    void SetWinAckSize();
    void SetPeerBandwidth();
    void SetChunkSize();

    void ResponseResult(int txId, const AMFValue &reply = AMFValue(),
                        const AMFValue &status = AMFValue());

    static const int kWinAckSize = 2500000;
    static const int kSendChunkSize = 60000;
    static const int kRecvChunkSize = 128;
    static const int kMaxHeaderBytes = 3 + 11;

    Role m_role;

    size_t m_sendChunkSize;
    size_t m_revcChunkSize;

    std::string m_app;
    std::string m_streamName;

    PacketContextMap m_packetContext;
    RtmpHeaderEncode m_headerEncoder;

    OnChunkRecv m_onChunkRecv;
    OnChunkSend m_onChunkSend;

    ConnectCommand m_connectCommand;
    FCPublishCommand m_fcpublishCommand;
    CreateStreamCommand m_csCommand;
    PublishCommand m_publishCommand;
    PlayCommand m_playCommand;
};

#endif // STREAM_PROCESS_H
