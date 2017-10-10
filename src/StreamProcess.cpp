#include "StreamProcess.h"
#include "stdio.h"

namespace
{
    void PrintBuf(char *buf, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            printf("%x ", buf[i] & 0xff);
        }
        printf("\n");
    }
}

StreamProcess::StreamProcess(const OnChunkSend &onChunkSend)
    : m_onChunkSend(onChunkSend)
{ }

int StreamProcess::Process(char *data, size_t len)
{
    RtmpHeaderState state = m_headerDecoder.Decode(data, len);

    if (state == RtmpHeaderState_Error)
        return -1;
    else if (state == RtmpHeaderState_NotEnoughData)
        return 0;

    m_headerDecoder.Dump();

    size_t headLen = m_headerDecoder.GetConsumeDataLen();
    size_t bodyLen = m_headerDecoder.GetMsgHeader().length;
    size_t needLen = headLen + bodyLen;

    if (len < needLen)
        return 0;

    PacketMeta meta;
    meta.basicHeader = m_headerDecoder.GetBasicHeader();
    meta.extendedTimestamp = m_headerDecoder.GetExtendedTimestamp();
    meta.msgHeader = m_headerDecoder.GetMsgHeader();

    if (!Amf0Decode(data + headLen, bodyLen, meta))
        return -1;
    else
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

bool StreamProcess::Amf0Decode(char *data, size_t len, PacketMeta &meta)
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

        meta.type = PacketType_Connect;
        OnConnect(meta, m_connectCommand);
    }
    else if (name == "FCPublish")
    {
        if (!FCPublishDecode(helper.GetData(), helper.GetLeftSize(),
                             name, transactionId))
            return false;

        meta.type = PacketType_FCPublish;
        OnFCPublish(meta, m_fcpublishCommand);
    }
    else if (name == "createStream")
    {
        if (!CreateStreamDecode(helper.GetData(), helper.GetLeftSize(),
                                name, transactionId))
            return false;

        meta.type = PacketType_CreateStream;
        OnCreateStream(meta, m_csCommand);
    }
    else if (name == "publish")
    {
        if (!PublishDecode(helper.GetData(), helper.GetLeftSize(),
                           name, transactionId))
            return false;

        meta.type = PacketType_Publish;
        OnPublish(meta, m_publishCommand);
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

void StreamProcess::OnConnect(const PacketMeta &meta,
                              const ConnectCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(meta, &command);

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

void StreamProcess::OnFCPublish(const PacketMeta &meta,
                                const FCPublishCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(meta, &command);

    amf_object_t status;
    status.insert(std::make_pair("code", std::string("NetStream.Publish.Start")));
    status.insert(std::make_pair("description", command.streamName));

    Encoder invoke;
    amf_write(&invoke, std::string("onFCPublish"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);
    SendChunk(5, 0x14, 0, 1, &invoke.buf[0], invoke.buf.size());

    ResponseResult(command.transactionId);
}

bool StreamProcess::CreateStreamDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    m_csCommand.name = name;
    m_csCommand.transactionId = transactionId;
    return true;
}

void StreamProcess::OnCreateStream(const PacketMeta &meta,
                                   const CreateStreamCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(meta, &command);

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

void StreamProcess::OnPublish(const PacketMeta &meta,
                              const PublishCommand &command)
{
    if (m_onChunkRecv)
        m_onChunkRecv(meta, &command);

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
    SendChunk(5, 0x14, 0, 1, &invoke.buf[0], invoke.buf.size());

    ResponseResult(command.transactionId);
}

void StreamProcess::SendChunk(
    int csId, int typeId, unsigned int timestamp,
    int streamId, char *data, size_t len)
{
    if (!data || len <= 0)
        return ;

    ChunkMsgHeader msgHeader;
    msgHeader.length = 0;
    msgHeader.streamId = streamId;
    msgHeader.timestamp = timestamp;
    msgHeader.typeId = typeId;

    size_t pos = 0;
    while (pos < len)
    {
        std::string buf;
        int sizeToSend = len - pos;
        if (sizeToSend > kChunkSize)
            sizeToSend = kChunkSize;

        msgHeader.length = sizeToSend;

        buf.resize(kMaxHeaderBytes);
        size_t size = buf.size();
        m_headerEncoder.Encode(&buf[0], &size, csId, msgHeader);
        buf.resize(size);
        PrintBuf(&buf[0], buf.size());

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
    byteStream.Write4Bytes(kChunkSize);

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
