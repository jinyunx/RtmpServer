#include "StreamProcess.h"
#include "libamf/amf0.h"

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
    size_t nread = 0;
    amf0_data *amfData = amf0_data_buffer_read((uint8_t *)data, len, &nread);
    if (!amfData || amfData->type != AMF0_TYPE_STRING)
        return false;

    std::string name((char *)amfData->string_data.mbstr,
                     amfData->string_data.size);

    data += nread;
    len -= nread;
    amf0_data_free(amfData);

    if (len == 0)
        return false;

    nread = 0;
    amfData = amf0_data_buffer_read((uint8_t *)data, len, &nread);
    if (!amfData || amfData->type != AMF0_TYPE_NUMBER)
        return false;

    int transactionId = amfData->number_data;

    data += nread;
    len -= nread;
    amf0_data_free(amfData);

    if (len == 0)
        return false;

    if (name == "connect")
    {
        if (!ConnectCommandDecode(data, len, name, transactionId))
            return false;
        *type = PacketType_Connect;
    }
    return true;
}

bool StreamProcess::ConnectCommandDecode(char *data, size_t len,
                                         const std::string &name,
                                         int transactionId)
{
    amf0_data *amfData = 0;
    while (len > 0)
    {
        size_t nread = 0;
        amfData = amf0_data_buffer_read((uint8_t *)data, len, &nread);
        if (!amfData)
            return false;

        data += nread;
        len -= nread;

        if (amfData->type != AMF0_TYPE_OBJECT)
        {
            amf0_data_free(amfData);
            amfData = 0;
            continue;
        }
        break;
    }

    if (!amfData)
        return false;

    amf0_node * node = amf0_object_first(amfData);
    while (node != 0)
    {
        amf0_data *k = amf0_object_get_name(node);
        amf0_data *v = amf0_object_get_data(node);
        node = amf0_object_next(node);

        if (k->type != AMF0_TYPE_STRING ||
            v->type != AMF0_TYPE_STRING)
            continue;

        std::string key((char *)k->string_data.mbstr, k->string_data.size);
        std::string value((char *)v->string_data.mbstr, v->string_data.size);
        if (key == "app")
            m_connectCommand.app = value;
        else if (key == "tcUrl")
            m_connectCommand.tcUrl = value;
    }
    amf0_data_free(amfData);

    m_connectCommand.name = name;
    m_connectCommand.transactionId = transactionId;
    return true;
}

