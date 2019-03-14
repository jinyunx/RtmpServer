#include "RtmpHeader.h"
#include <stdio.h>

RtmpHeaderState RtmpHeaderDecode::Decode(
    char *data, size_t len, bool startNewMsg)
{
    if (!m_byteStream.Initialize(data, len))
        return RtmpHeaderState_NotEnoughData;

    RtmpHeaderState state = DecodeBasicHeader();
    if (state != RtmpHeaderState_Ok)
        return state;

    state = DecodeMsgHeader(startNewMsg);
    if (state == RtmpHeaderState_Ok)
    {
        m_complete = true;
        m_csIdMsgHeader[m_basicHeader.csId] = m_msgHeader;
    }

    return state;
}

RtmpHeaderState RtmpHeaderDecode::DecodeCsId(char * data, size_t len)
{
    if (!m_byteStream.Initialize(data, len))
        return RtmpHeaderState_NotEnoughData;
    return DecodeBasicHeader();
}

ChunkMsgHeader RtmpHeaderDecode::GetMsgHeader() const
{
    return m_msgHeader;
}

ChunkBasicHeader RtmpHeaderDecode::GetBasicHeader() const
{
    return m_basicHeader;
}

int RtmpHeaderDecode::GetConsumeDataLen()
{
    return m_byteStream.Pos();
}

bool RtmpHeaderDecode::IsComplete()
{
    return m_complete;
}

void RtmpHeaderDecode::Reset()
{
    m_complete = false;
    m_basicHeader.Reset();
    m_msgHeader.Reset();
    m_csIdMsgHeader.clear();
    m_byteStream.Initialize(0, 0);
}

void RtmpHeaderDecode::Dump()
{
    fprintf(stdout, "fmt: %d, csid: %d, timestamp: %d,"
            " length: %d, typeId: %d, streamId: %d\n",
            m_basicHeader.fmt, m_basicHeader.csId, m_msgHeader.timestamp,
            m_msgHeader.length, m_msgHeader.typeId, m_msgHeader.streamId);
}

RtmpHeaderState RtmpHeaderDecode::DecodeBasicHeader()
{
    if (!m_byteStream.Require(1))
        return RtmpHeaderState_NotEnoughData;

    char oneByte = m_byteStream.Read1Bytes();
    m_basicHeader.fmt = static_cast<unsigned int>(oneByte >> 6 & 0x03);
    m_basicHeader.csId = static_cast<unsigned int>(oneByte & 0x3f);

    switch (m_basicHeader.csId)
    {
    case 0:
        if (!m_byteStream.Require(1))
            return RtmpHeaderState_NotEnoughData;

        m_basicHeader.csId =
            64 + static_cast<unsigned int>(m_byteStream.Read1Bytes());
        return RtmpHeaderState_Ok;

    case 1:
        if (!m_byteStream.Require(2))
            return RtmpHeaderState_NotEnoughData;

        m_basicHeader.csId = 64 +
            static_cast<unsigned int>(m_byteStream.Read1Bytes()) +
            static_cast<unsigned int>(m_byteStream.Read1Bytes()) * 256;
        return RtmpHeaderState_Ok;

    default:
        return RtmpHeaderState_Ok;
    }
}

RtmpHeaderState RtmpHeaderDecode::DecodeMsgHeader(bool startNewMsg)
{
    ChunkMsgHeader *lastMsgHeader = 0;
    if (m_basicHeader.fmt > 0)
    {
        CsIdMsgHeader::iterator it = m_csIdMsgHeader.find(m_basicHeader.csId);
        if (it == m_csIdMsgHeader.end())
            return RtmpHeaderState_Error;
        lastMsgHeader = &it->second;
    }

    switch (m_basicHeader.fmt)
    {
    case 0:
        if (!m_byteStream.Require(11))
            return RtmpHeaderState_NotEnoughData;

        m_msgHeader.tsField = m_byteStream.Read3Bytes();
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        // Inputed stream id is little endian already
        m_msgHeader.streamId =
            static_cast<unsigned int>(m_byteStream.Read4BytesKeepOriOrder());
        break;

    case 1:
        if (!m_byteStream.Require(7))
            return RtmpHeaderState_NotEnoughData;

        m_msgHeader.tsField = m_byteStream.Read3Bytes();
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        m_msgHeader.streamId = lastMsgHeader->streamId;
        break;

    case 2:
        if (!m_byteStream.Require(3))
            return RtmpHeaderState_NotEnoughData;

        m_msgHeader.tsField = m_byteStream.Read3Bytes();
        m_msgHeader.length = lastMsgHeader->length;
        m_msgHeader.typeId = lastMsgHeader->typeId;
        m_msgHeader.streamId = lastMsgHeader->streamId;
        break;

    case 3:
        m_msgHeader = *lastMsgHeader;
        // Not the start of new msg means
        // the chunk is park of one message
        // had already started, so just copy
        // the last msg header and drop the
        // extern timestamp when need, then return.
        if (!startNewMsg)
        {
            if (m_msgHeader.tsField >= 0xFFFFFF)
            {
                if (!m_byteStream.Require(4))
                    return RtmpHeaderState_NotEnoughData;
                m_byteStream.Read4Bytes();
            }
            return RtmpHeaderState_Ok;
        }
        break;

    default:
        return RtmpHeaderState_Error;
    }

    if (m_msgHeader.tsField >= 0xFFFFFF)
    {
        if (!m_byteStream.Require(4))
            return RtmpHeaderState_NotEnoughData;
        m_msgHeader.timestamp = m_byteStream.Read4Bytes();
    }
    else
    {
        m_msgHeader.timestamp = m_msgHeader.tsField;
    }

    if (m_basicHeader.fmt != 0)
        m_msgHeader.timestamp += lastMsgHeader->timestamp;

    return RtmpHeaderState_Ok;
}

RtmpHeaderEncode::RtmpHeaderEncode()
{ }

RtmpHeaderState RtmpHeaderEncode::Encode(
    char *data, size_t *len,
    unsigned int csId,
    ChunkMsgHeader &msgHeader,
    bool startNewMsg)
{
    if (*len < kMaxBytes ||
        csId < 2 || csId > 0x3fffff)
        return RtmpHeaderState_Error;

    m_byteStream.Initialize(data, *len);

    ChunkMsgHeader *lastMsgHeader = 0;
    CsIdMsgHeader::iterator it = m_csIdMsgHeader.find(csId);
    if (it != m_csIdMsgHeader.end())
        lastMsgHeader = &it->second;

    if (!startNewMsg && *lastMsgHeader != msgHeader)
        return RtmpHeaderState_Error;

    if (startNewMsg)
        EncodeStartMsg(csId, &msgHeader, lastMsgHeader);
    else
        EncodeInterMsg(csId, &msgHeader);

    *len = m_byteStream.Pos();
    m_csIdMsgHeader[csId] = msgHeader;

    return RtmpHeaderState_Ok;
}

RtmpHeaderState RtmpHeaderEncode::EncodeStartMsg(
    unsigned int csId,
    ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader)
{
    SetTsField(msgHeader, lastMsgHeader);

    unsigned int fmt = GetFmt(msgHeader, lastMsgHeader);
    SetBasicHeader(fmt, csId);
    SetMsgHeader(fmt, msgHeader);
}

RtmpHeaderState RtmpHeaderEncode::EncodeInterMsg(
    unsigned int csId,
    const ChunkMsgHeader *msgHeader)
{
    SetBasicHeader(3, csId);
    SetMsgHeader(3, msgHeader);
}

void RtmpHeaderEncode::Dump()
{
    for (int i = 0; i < m_byteStream.Pos(); ++i)
        printf("%x ", m_byteStream.Data()[i] & 0xff);
    printf("\n");
}

bool RtmpHeaderEncode::IsFmt0(
    const ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader)
{
    return (!lastMsgHeader ||
            msgHeader->timestamp < lastMsgHeader->timestamp ||
            msgHeader->streamId != lastMsgHeader->streamId);
}

void RtmpHeaderEncode::SetTsField(ChunkMsgHeader *msgHeader,
                                  const ChunkMsgHeader *lastMsgHeader)
{
    if (IsFmt0(msgHeader, lastMsgHeader))
        msgHeader->tsField = msgHeader->timestamp;
    else
        msgHeader->tsField = msgHeader->timestamp -
            lastMsgHeader->timestamp;

    if (msgHeader->tsField >= 0xFFFFFF)
    {
        msgHeader->externedTimestamp = msgHeader->tsField;
        msgHeader->tsField = 0xFFFFFF;
    }
}

unsigned int RtmpHeaderEncode::GetFmt(
    const ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader)
{
    unsigned int fmt = 0;
    if (IsFmt0(msgHeader, lastMsgHeader))
        return fmt;

    fmt = 1;

    if (msgHeader->length != lastMsgHeader->length ||
        msgHeader->typeId != lastMsgHeader->typeId)
        return fmt;

    fmt = 2;

    if (msgHeader->tsField == lastMsgHeader->tsField)
        fmt = 3;

    return fmt;
}

void RtmpHeaderEncode::SetBasicHeader(
    unsigned int fmt, unsigned int csId)
{
    if (csId < 64)
        m_byteStream.Write1Bytes(static_cast<char>(csId));
    else if (csId < 65599)
        m_byteStream.Write2Bytes(static_cast<unsigned short>(csId));
    else
        m_byteStream.Write3Bytes(csId);

    m_byteStream.Data()[0] |= fmt << 6;
}

void RtmpHeaderEncode::SetMsgHeader(
    unsigned int fmt,
    const ChunkMsgHeader *msgHeader)
{
    switch (fmt)
    {
    case 0:
        m_byteStream.Write3Bytes(msgHeader->tsField);
        m_byteStream.Write3Bytes(msgHeader->length);
        m_byteStream.Write1Bytes(msgHeader->typeId);
        m_byteStream.Write4BytesKeepOriOrder(msgHeader->streamId);
        break;

    case 1:
        m_byteStream.Write3Bytes(msgHeader->tsField);
        m_byteStream.Write3Bytes(msgHeader->length);
        m_byteStream.Write1Bytes(msgHeader->typeId);
        break;

    case 2:
        m_byteStream.Write3Bytes(msgHeader->tsField);
        break;

    case 3:
        break;

    default:
        break;
    }
    if (msgHeader->tsField >= 0xFFFFFF)
        m_byteStream.Write4Bytes(msgHeader->externedTimestamp);
}
