#include "StreamProcess.h"
#include "stdio.h"

#define MSG_TYPE_CHUNK_SIZE 0x01
#define MSG_TYPE_WINACK_SIZE 0x05
#define MSG_TYPE_COMMAND 0x14
#define MSG_TYPE_METADATA 0x12
#define MSG_TYPE_VIDEO 0x09
#define MSG_TYPE_AUDIO 0x08

namespace
{
    void PrintBuf(char *buf, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            printf("%x ", buf[i] & 0xff);
            if ((i + 1) % 16 == 0)
                printf("\n");
        }
        printf("\n");
    }

    void WriteH264(char *buf, size_t size)
    {
        static FILE *fd = fopen("dump.h264", "wb");
        size_t useSize = 0;

        if (buf[1] == 0x00)
        {
            // sps pps
            // Skip 1.frame type and codec id[1]
            //      2.AVCPacketType[1]
            //      3.CompositionTime[3]
            //      4.version[1]
            //      5.profile indication[1]
            //      6.profile compatibility[1]
            //      7.level indication[1]
            //      8.length size munus one[1]
            //      9.number of sps[1]
            buf += 11;
            useSize += 11;

            while (useSize < size)
            {
                int length = 0;
                char *p = (char *)&length;
                p[0] = buf[1];
                p[1] = buf[0];

                // 00 00 00 01
                fwrite("\x00\x00\x00\x01", 4, 1, fd);
                
                fwrite(buf+2, length, 1, fd);

                // skip 01
                buf += length + 2 + 1;
                useSize += length + 2 + 1;
            }
        }
        else
        {
            // Skip 1.frame type and codec id[1]
            //      2.AVCPacketType[1]
            //      3.CompositionTime[3]
            buf += 1 + 1 + 3;
            useSize += 5;

            while (useSize < size)
            {
                int length = 0;
                char *p = (char *)&length;
                p[0] = buf[3];
                p[1] = buf[2];
                p[2] = buf[1];
                p[3] = buf[0];

                // 00 00 00 01
                buf[0] = 0;
                buf[1] = 0;
                buf[2] = 0;
                buf[3] = 1;

                fwrite(buf, length + 4, 1, fd);

                buf += length + 4;
                useSize += length + 4;
            }
        }
    }
}

StreamProcess::StreamProcess(const OnChunkSend &onChunkSend)
    : m_role(Role_Unknow), m_sendChunkSize(kSendChunkSize),
      m_revcChunkSize(kRecvChunkSize), m_onChunkSend(onChunkSend)
{ }

int StreamProcess::Process(char *data, size_t len)
{
    RtmpHeaderDecode headerDecoder;
    RtmpHeaderState state = headerDecoder.DecodeCsId(data, len);
    if (state == RtmpHeaderState_NotEnoughData)
        return 0;
    else if (state == RtmpHeaderState_Error)
        return -1;

    unsigned int csId = headerDecoder.GetBasicHeader().csId;

    PacketContext &context = m_packetContext[csId];
    context.csId = csId;
    if (context.stage == Stage_Header)
    {
        RtmpHeaderState state = context.headerDecoder.Decode(data, len);

        if (state == RtmpHeaderState_Error)
            return -1;
        else if (state == RtmpHeaderState_NotEnoughData)
            return 0;

        context.headerDecoder.Dump();
        context.stage = Stage_Body;
    }

    // Skip header
    data += context.headerDecoder.GetConsumeDataLen();
    len -= context.headerDecoder.GetConsumeDataLen();

    size_t msgLen = context.headerDecoder.GetMsgHeader().length;
    size_t needLen = msgLen - context.payload.size();

    if (needLen > m_revcChunkSize)
        needLen = m_revcChunkSize;

    printf("len = %ld, needLen = %ld\n", len, needLen);

    if (len < needLen)
        return 0;

    context.payload.append(data, needLen);

    if (context.payload.size() == msgLen)
    {
        Dispatch(context);
        context.payload.clear();
    }

    context.stage = Stage_Header;
    return needLen + context.headerDecoder.GetConsumeDataLen();
}

void StreamProcess::SetOnChunkRecv(const OnChunkRecv &onChunkRecv)
{
    m_onChunkRecv = onChunkRecv;
}

void StreamProcess::Dump()
{
    printf("connect name: %s, id: %d, app: %s, tcUrl: %s\n",
           m_connectCommand.name.c_str(), m_connectCommand.transactionId,
           m_connectCommand.app.c_str(), m_connectCommand.tcUrl.c_str());
}

const std::string & StreamProcess::GetApp()
{
    return m_app;
}

const std::string & StreamProcess::GetStreamName()
{
    return m_streamName;
}

Role StreamProcess::GetRole()
{
    return m_role;
}

bool StreamProcess::Dispatch(PacketContext &context)
{
    if (!context.payload.size())
        return false;

    switch (context.headerDecoder.GetMsgHeader().typeId)
    {
    case MSG_TYPE_COMMAND:
        if (!Amf0Decode(&context.payload[0], context.payload.size(),
            context))
            return false;
        break;

    case MSG_TYPE_METADATA:
        context.type = PacketType_MetaData;
        OnMetaData(context, &context.payload[0], context.payload.size());
        break;

    case MSG_TYPE_VIDEO:
        context.type = PacketType_Video;
        WriteH264(&context.payload[0], context.payload.size());
        OnVideo(context, &context.payload[0], context.payload.size());
        break;

    case MSG_TYPE_AUDIO:
        context.type = PacketType_Audio;
        OnAudio(context, &context.payload[0], context.payload.size());
        break;

    default:
        break;
    }
    return true;
}

size_t StreamProcess::GetNeedLength(size_t body)
{
    if (body <= m_revcChunkSize)
        return body;

    return body + (body-1) / m_revcChunkSize;
}

bool StreamProcess::Amf0Decode(char *data, size_t len, PacketContext &context)
{
    Amf0Helper helper(data, len);

    std::string name;
    if (helper.Expect(AMF_STRING))
        name = helper.GetString();
    else
        return false;

    int transactionId = 0;
    if (helper.Expect(AMF_NUMBER))
        transactionId = helper.GetNumber();
    else
        return false;

    if (name == "connect")
    {
        if (!ConnectDecode(helper.GetData(), helper.GetLeftSize(),
                           name, transactionId))
            return false;

        context.type = PacketType_Connect;
        OnConnect(context, m_connectCommand);
    }
    else if (name == "FCPublish")
    {
        if (!FCPublishDecode(helper.GetData(), helper.GetLeftSize(),
                             name, transactionId))
            return false;

        context.type = PacketType_FCPublish;
        OnFCPublish(context, m_fcpublishCommand);
    }
    else if (name == "createStream")
    {
        if (!CreateStreamDecode(helper.GetData(), helper.GetLeftSize(),
                                name, transactionId))
            return false;

        context.type = PacketType_CreateStream;
        OnCreateStream(context, m_csCommand);
    }
    else if (name == "publish")
    {
        if (!PublishDecode(helper.GetData(), helper.GetLeftSize(),
                           name, transactionId))
            return false;

        context.type = PacketType_Publish;
        OnPublish(context, m_publishCommand);
    }
    else if (name == "play")
    {
        if (!PlayDecode(helper.GetData(), helper.GetLeftSize(),
                        name, transactionId))
            return false;

        context.type = PacketType_Play;
        OnPlay(context, m_playCommand);
    }

    return true;
}

bool StreamProcess::ConnectDecode(char *data, size_t len,
                                  const std::string &name,
                                  int transactionId)
{
    Amf0Helper helper(data, len);

    Amf0Obj obj;
    if (helper.Expect(AMF_OBJECT))
        obj = helper.GetObj();
    else
        return false;

    m_connectCommand.app = obj["app"];
    m_connectCommand.tcUrl = obj["tcUrl"];
    m_connectCommand.name = name;
    m_connectCommand.transactionId = transactionId;

    m_app = m_connectCommand.app;
    return true;
}

void StreamProcess::OnConnect(const PacketContext &context,
                              const ConnectCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

    SetChunkSize();
    SetWinAckSize();

    amf_object_t version;
    version.insert(std::make_pair("fmsVer", std::string("FMS/4,5,1,484")));
    version.insert(std::make_pair("capabilities", 255.0));
    version.insert(std::make_pair("mode", 1.0));

    amf_object_t status;
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetConnection.Connect.Success")));
    status.insert(std::make_pair("description", std::string("Connection succeeded.")));

    printf("OnConnect response\n");
    ResponseResult(command.transactionId, version, status);
}

bool StreamProcess::FCPublishDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    Amf0Helper helper(data, len);

    std::string streamName;
    if (helper.Expect(AMF_STRING))
        streamName = helper.GetString();
    else
        return false;

    m_fcpublishCommand.name = name;
    m_fcpublishCommand.transactionId = transactionId;
    m_fcpublishCommand.streamName = streamName;

    m_streamName = streamName;
    return true;
}

void StreamProcess::OnFCPublish(const PacketContext &context,
                                const FCPublishCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

    amf_object_t status;
    status.insert(std::make_pair("code", std::string("NetStream.Publish.Start")));
    status.insert(std::make_pair("description", command.streamName));

    Encoder invoke;
    amf_write(&invoke, std::string("onFCPublish"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);

    ChunkMsgHeader msgHeader;
    msgHeader.typeId = MSG_TYPE_COMMAND;
    msgHeader.streamId = 1;
    msgHeader.timestamp = 0;
    msgHeader.length = invoke.buf.size();

    printf("onFCPublish\n");
    SendChunk(5, msgHeader, &invoke.buf[0]);

    printf("onFCPublish result: %d\n", command.transactionId);
    ResponseResult(command.transactionId);
}

bool StreamProcess::CreateStreamDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    m_csCommand.name = name;
    m_csCommand.transactionId = transactionId;
    return true;
}

void StreamProcess::OnCreateStream(const PacketContext &context,
                                   const CreateStreamCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

    printf("OnCreateStream result\n");
    ResponseResult(command.transactionId);
}

bool StreamProcess::PublishDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    Amf0Helper helper(data, len);

    std::string streamName;
    if (helper.Expect(AMF_STRING))
        streamName = helper.GetString();
    else
        return false;

    std::string app;
    if (helper.Expect(AMF_STRING))
        app = helper.GetString();
    else
        return false;

    m_publishCommand.name = name;
    m_publishCommand.transactionId = transactionId;
    m_publishCommand.streamName = streamName;
    m_publishCommand.app = app;

    m_streamName = streamName;
    m_app = app;
    return true;
}

void StreamProcess::OnPublish(const PacketContext &context,
                              const PublishCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

    m_role = Role_Publisher;

    amf_object_t status;
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetStream.Publish.Start")));
    status.insert(std::make_pair("description", std::string("Stream is now published.")));
    status.insert(std::make_pair("details", command.streamName));

    Encoder invoke;
    amf_write(&invoke, std::string("onStatus"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);

    ChunkMsgHeader msgHeader;
    msgHeader.typeId = MSG_TYPE_COMMAND;
    msgHeader.streamId = 1;
    msgHeader.timestamp = 0;
    msgHeader.length = invoke.buf.size();

    printf("OnPublish\n");
    SendChunk(5, msgHeader, &invoke.buf[0]);

    printf("OnPublish result\n");
    ResponseResult(command.transactionId);
}

bool StreamProcess::PlayDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    Amf0Helper helper(data, len);

    std::string streamName;
    if (helper.Expect(AMF_STRING))
        streamName = helper.GetString();
    else
        return false;

    m_playCommand.name = name;
    m_playCommand.transactionId = transactionId;
    m_playCommand.streamName = streamName;

    m_streamName = streamName;
    return true;
}

void StreamProcess::OnPlay(const PacketContext &context,
                           const PlayCommand &command)
{
    printf("OnPlay result\n");
    ResponseResult(command.transactionId);

    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

    m_role = Role_Player;
}

void StreamProcess::OnMetaData(const PacketContext &context,
                               const char *data, size_t len)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, 0);
}

void StreamProcess::OnVideo(const PacketContext &context,
                            const char *data, size_t len)
{
    VideoInfo videoInfo;
    videoInfo.isKeyFrame = data[0] & 0x10;
    videoInfo.isSpspps = data[1] == 0;

    if (m_onChunkRecv)
        m_onChunkRecv(context, &videoInfo);
}

void StreamProcess::OnAudio(const PacketContext &context,
                            const char *data, size_t len)
{
    AudioInfo audioInfo;
    audioInfo.isSeqHeader = data[1] == 0;

    if (m_onChunkRecv)
        m_onChunkRecv(context, &audioInfo);
}

void StreamProcess::SendChunk(int csId, ChunkMsgHeader msgHeader, const char *data)
{
    if (!data || msgHeader.length <= 0)
        return ;

    size_t pos = 0;
    while (pos < msgHeader.length)
    {
        std::string buf;
        int sizeToSend = msgHeader.length - pos;
        if (sizeToSend > kSendChunkSize)
            sizeToSend = kSendChunkSize;

        buf.resize(kMaxHeaderBytes);
        size_t size = buf.size();
        m_headerEncoder.Encode(&buf[0], &size, csId, msgHeader);
        buf.resize(size);

        buf.append(data + pos, sizeToSend);

        PrintBuf(&buf[0], buf.size());

        if (m_onChunkSend)
            m_onChunkSend(&buf[0], buf.size());

        pos += sizeToSend;
    }
}

void StreamProcess::SetWinAckSize()
{
    int csId = 2;

    char buf[4] = { 0 };
    ByteStream byteStream;
    byteStream.Initialize(buf, sizeof(buf));
    byteStream.Write4Bytes(kWinAckSize);

    ChunkMsgHeader msgHeader;
    msgHeader.typeId = MSG_TYPE_WINACK_SIZE;
    msgHeader.streamId = 0;
    msgHeader.timestamp = 0;
    msgHeader.length = sizeof(buf);

    printf("SetWinAckSize\n");
    SendChunk(csId, msgHeader, buf);
}

void StreamProcess::SetChunkSize()
{
    int csId = 2;

    char buf[4] = { 0 };
    ByteStream byteStream;
    byteStream.Initialize(buf, sizeof(buf));
    byteStream.Write4Bytes(kSendChunkSize);

    ChunkMsgHeader msgHeader;
    msgHeader.typeId = MSG_TYPE_CHUNK_SIZE;
    msgHeader.streamId = 0;
    msgHeader.timestamp = 0;
    msgHeader.length = sizeof(buf);

    printf("SetChunkSize\n");
    SendChunk(csId, msgHeader, buf);
}

void StreamProcess::ResponseResult(
    int txId, const AMFValue &reply, const AMFValue &status)
{
    int csId = 3;

    Encoder encoder;
    amf_write(&encoder, std::string("_result"));
    amf_write(&encoder, (double)txId);
    amf_write(&encoder, reply);
    amf_write(&encoder, status);

    ChunkMsgHeader msgHeader;
    msgHeader.typeId = MSG_TYPE_COMMAND;
    msgHeader.streamId = 0;
    msgHeader.timestamp = 0;
    msgHeader.length = encoder.buf.size();

    SendChunk(csId, msgHeader, &encoder.buf[0]);
}
