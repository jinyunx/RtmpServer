#include "StreamProcess.h"
#include "stdio.h"

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
    : m_sendChunkSize(kSendChunkSize), m_revcChunkSize(kRecvChunkSize),
      m_onChunkSend(onChunkSend)
{ }

int StreamProcess::Process(char *data, size_t len)
{
    RtmpHeaderDecode headerDecoder;
    RtmpHeaderState state = headerDecoder.DecodeCsId(data, len);
    if (state != RtmpHeaderState_Ok)
        return state;
    unsigned int csId = headerDecoder.GetBasicHeader().csId;

    PacketContext &context = m_packetContext[csId];
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

    if (len < needLen)
        return 0;

    context.payload.append(data, needLen);

    if (context.payload.size() == msgLen)
    {
        switch (context.headerDecoder.GetMsgHeader().typeId)
        {
        case 0x14:
            if (!Amf0Decode(&context.payload[0], context.payload.size(),
                            context))
                return -1;
            break;

        case 0x09:
            WriteH264(&context.payload[0], context.payload.size());
            break;

        default:
            break;
        }
    }

    context.stage = Stage_Header;
    return needLen;
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

size_t StreamProcess::GetNeedLength(size_t body)
{
    if (body <= m_revcChunkSize)
        return body;

    return body + (body-1) / m_revcChunkSize;
}

char * StreamProcess::MergeChunk(std::string &buf, char *data, size_t len)
{
    if (len <= m_revcChunkSize)
        return data;

    size_t i = 0;
    while (i < len)
    {
        size_t copy = len - i;
        if (copy > m_revcChunkSize)
            copy = m_revcChunkSize;
        buf.append(data + i, copy);

        // Add 1 to skip basic header
        i += copy + 1;
    }
    return &buf[0];
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

    printf("onFCPublish\n");
    SendChunk(5, 0x14, 0, 1, &invoke.buf[0], invoke.buf.size());

    printf("onFCPublish result\n");
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

    return true;
}

void StreamProcess::OnPublish(const PacketContext &context,
                              const PublishCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(context, &command);

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


    printf("OnPublish\n");
    SendChunk(5, 0x14, 0, 1, &invoke.buf[0], invoke.buf.size());

    printf("OnPublish result\n");
    ResponseResult(command.transactionId);
}

void StreamProcess::SendChunk(
    int csId, int typeId, unsigned int timestamp,
    int streamId, char *data, size_t len)
{
    if (!data || len <= 0)
        return ;

    ChunkMsgHeader msgHeader;
    msgHeader.length = len;
    msgHeader.streamId = streamId;
    msgHeader.timestamp = timestamp;
    msgHeader.typeId = typeId;

    size_t pos = 0;
    while (pos < len)
    {
        std::string buf;
        int sizeToSend = len - pos;
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

    SendChunk(csId, 0x05, 0, 0, buf, sizeof(buf));
}

void StreamProcess::SetChunkSize()
{
    int csId = 2;

    char buf[4] = { 0 };
    ByteStream byteStream;
    byteStream.Initialize(buf, sizeof(buf));
    byteStream.Write4Bytes(kSendChunkSize);

    SendChunk(csId, 0x01, 0, 0, buf, sizeof(buf));
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

    SendChunk(csId, 0x14, 0, 0, &encoder.buf[0], encoder.buf.size());
}
