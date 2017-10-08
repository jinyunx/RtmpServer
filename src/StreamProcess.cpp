#include "StreamProcess.h"
#include "Amf0Helper.h"

// TODO:
// 1.amf0_data free obj

int StreamProcess::Process(char *data, size_t len)
{
    ChunkMsgHeader *lastMsgHeader = &m_lastMsgHeader;
    if (m_first)
        lastMsgHeader = 0;

    RtmpHeaderState state = m_headerDecoder.Decode(
        data, len, lastMsgHeader, m_lastHasExtended);

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

    m_lastMsgHeader = m_headerDecoder.GetMsgHeader();
    m_lastHasExtended = m_headerDecoder.HasExtenedTimestamp();
    m_lastBasicHeader = m_headerDecoder.GetBasicHeader();

    PacketType type;
    if (!Amf0Decode(data + headLen, bodyLen, &type))
        return -1;
    else
        return needLen;
}

void StreamProcess::Dump()
{
    printf("connect name: %s, id: %d, app: %s, tcUrl: %s\n",
           m_connectCommand.name.c_str(), m_connectCommand.transactionId,
           m_connectCommand.app.c_str(), m_connectCommand.tcUrl.c_str());
}

bool StreamProcess::Amf0Decode(char *data, size_t len, PacketType *type)
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
        if (!ConnectCommandDecode(helper.GetData(), helper.GetLeftSize(),
                                  name, transactionId))
            return false;
        *type = PacketType_Connect;
    }
    return true;
}

bool StreamProcess::ConnectCommandDecode(char *data, size_t len,
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
