#include "StreamProcess.h"
#include "Amf0Helper.h"

// TODO:
// 1.amf0_data free obj

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
    if (helper.Expect(AMF0_TYPE_STRING))
        name = helper.GetString();
    else
        return false;

    int transactionId = 0;
    if (helper.Expect(AMF0_TYPE_NUMBER))
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
    if (helper.Expect(AMF0_TYPE_OBJECT))
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

    SetWinAckSize();
    // SetPeerBandwidth
    SetChunkSize();
    // ResponseConnect();
}

bool StreamProcess::FCPublishDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    Amf0Helper helper(data, len);

    std::string streamName;
    if (helper.Expect(AMF0_TYPE_STRING))
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

    // ResponseFCPublish();
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

    // ResponseCreateStream();
}

bool StreamProcess::PublishDecode(
    char *data, size_t len, const std::string &name, int transactionId)
{
    Amf0Helper helper(data, len);

    std::string streamName;
    if (helper.Expect(AMF0_TYPE_STRING))
        streamName = helper.GetString();
    else
        return false;

    std::string app;
    if (helper.Expect(AMF0_TYPE_STRING))
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

    // ResponsePublish();
}

void StreamProcess::SetWinAckSize()
{
    int csId = 2;
    ChunkMsgHeader msgHeader;
    msgHeader.length = 4;
    msgHeader.streamId = 0;
    msgHeader.timestamp = 0;
    msgHeader.typeId = 0x05;

    char buf[14 + 4] = { 0 };
    size_t size = sizeof(buf);
    m_headerEncoder.Encode(buf, &size, csId, msgHeader);

    ByteStream byteStream;
    byteStream.Initialize(&buf[size], sizeof(buf) - size);
    byteStream.Write4Bytes(kWinAckSize);

    PrintBuf(buf, size + byteStream.Pos());

    if (m_onChunkSend)
        m_onChunkSend(buf, size + byteStream.Pos());
}

void StreamProcess::SetChunkSize()
{
    int csId = 2;
    ChunkMsgHeader msgHeader;
    msgHeader.length = 4;
    msgHeader.streamId = 0;
    msgHeader.timestamp = 0;
    msgHeader.typeId = 0x01;

    char buf[14 + 4] = { 0 };
    size_t size = sizeof(buf);
    m_headerEncoder.Encode(buf, &size, csId, msgHeader);

    ByteStream byteStream;
    byteStream.Initialize(&buf[size], sizeof(buf) - size);
    byteStream.Write4Bytes(kChunkSize);

    PrintBuf(buf, size + byteStream.Pos());

    if (m_onChunkSend)
        m_onChunkSend(buf, size + byteStream.Pos());
}
