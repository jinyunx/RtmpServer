#include "RtmpHeader.h"
#include <stdio.h>

RtmpHeaderState RtmpHeaderDecode::Decode(
    char *data, size_t len, const ChunkMsgHeader *lastMsgHeader,
    bool lastHasExtended)
{
    if (!m_byteStream.Initialize(data, len))
        return RtmpHeaderState_NotEnoughData;

    RtmpHeaderState state = DecodeBasicHeader();
    if (state != RtmpHeaderState_Ok)
        return state;

    state = DecodeMsgHeader(lastMsgHeader);
    if (state != RtmpHeaderState_Ok)
        return state;

    state = DecodeExtenedTimestamp(lastMsgHeader, lastHasExtended);
    if (state == RtmpHeaderState_Ok)
        m_complete = true;

    return state;
}

ChunkMsgHeader RtmpHeaderDecode::GetMsgHeader() const
{
    return m_msgHeader;
}

ChunkBasicHeader RtmpHeaderDecode::GetBasicHeader() const
{
    return m_basicHeader;
}

ExtendedTimestamp RtmpHeaderDecode::GetExtendedTimestamp() const
{
    return m_extenedTimestamp;
}

int RtmpHeaderDecode::GetConsumeDataLen()
{
    return m_byteStream.Pos();
}

bool RtmpHeaderDecode::IsComplete()
{
    return m_complete;
}

bool RtmpHeaderDecode::HasExtenedTimestamp()
{
    return m_hasExtenedTimestamp;
}

void RtmpHeaderDecode::Reset()
{
    m_complete = false;
    m_hasExtenedTimestamp = false;
    m_basicHeader.Reset();
    m_msgHeader.Reset();
    m_extenedTimestamp.Reset();
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
    m_basicHeader.csId = static_cast<unsigned int>(oneByte & 0x3f);
    m_basicHeader.fmt = static_cast<unsigned int>(oneByte >> 6 & 0x03);

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

RtmpHeaderState RtmpHeaderDecode::DecodeMsgHeader(
    const ChunkMsgHeader *lastMsgHeader)
{
    if (m_basicHeader.fmt > 0 && !lastMsgHeader)
        return RtmpHeaderState_Error;

    switch (m_basicHeader.fmt)
    {
    case 0:
        if (!m_byteStream.Require(11))
            return RtmpHeaderState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        // Inputed stream id is little endian already
        m_msgHeader.streamId =
            static_cast<unsigned int>(m_byteStream.Read4BytesKeepOriOrder());
        return RtmpHeaderState_Ok;

    case 1:
        if (!m_byteStream.Require(7))
            return RtmpHeaderState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return RtmpHeaderState_Ok;

    case 2:
        if (!m_byteStream.Require(3))
            return RtmpHeaderState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = lastMsgHeader->length;
        m_msgHeader.typeId = lastMsgHeader->typeId;
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return RtmpHeaderState_Ok;

    case 3:
        m_msgHeader.timestamp = lastMsgHeader->timestamp;
        m_msgHeader.length = lastMsgHeader->length;
        m_msgHeader.typeId = lastMsgHeader->typeId;
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return RtmpHeaderState_Ok;

    default:
        return RtmpHeaderState_Error;
    }
}

RtmpHeaderState RtmpHeaderDecode::DecodeExtenedTimestamp(
    const ChunkMsgHeader *lastMsgHeader, bool lastHasExtended)
{

    if (m_hasExtenedTimestamp)
    {
        if (!m_byteStream.Require(4))
            return RtmpHeaderState_NotEnoughData;

        m_extenedTimestamp.t = m_byteStream.Read4Bytes();
        m_msgHeader.timestamp = m_extenedTimestamp.t;
    }
    // See https://forums.adobe.com/thread/542749
    else if (m_basicHeader.fmt == 3 && lastHasExtended)
    {
        if (!m_byteStream.Require(4))
            return RtmpHeaderState_NotEnoughData;

        if (lastMsgHeader->timestamp ==
                static_cast<unsigned int>(m_byteStream.Read4Bytes()))
        {
            m_extenedTimestamp.t = lastMsgHeader->timestamp;
            m_msgHeader.timestamp = m_extenedTimestamp.t;
            m_hasExtenedTimestamp = true;
        }
        else
        {
            m_byteStream.Skip(-4);
        }
    }
    return RtmpHeaderState_Ok;
}

void RtmpHeaderDecode::HandleTimestamp(
    const ChunkMsgHeader *lastMsgHeader)
{
    m_msgHeader.timestamp = m_byteStream.Read3Bytes();
    if (m_msgHeader.timestamp >= 0x00ffffff)
        m_hasExtenedTimestamp = true;
    else if (m_basicHeader.fmt != 0)
        m_msgHeader.timestamp += lastMsgHeader->timestamp;
}

RtmpHeaderEncode::RtmpHeaderEncode()
    : m_hasExtended(false)
{ }

RtmpHeaderState RtmpHeaderEncode::Encode(
    char *data, size_t *len,
    unsigned int csId,
    const ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader,
    bool lastHasExtended)
{
    if (!msgHeader || *len < kMaxBytes ||
        csId < 2 || csId > 0x3fffff)
        return RtmpHeaderState_Error;

    m_byteStream.Initialize(data, *len);

    unsigned int fmt = GetFmt(msgHeader, lastMsgHeader);
    SetBasicHeader(fmt, csId);

    SetMsgHeader(fmt, msgHeader, lastMsgHeader, lastHasExtended);
    *len = m_byteStream.Pos();

    return RtmpHeaderState_Ok;
}

bool RtmpHeaderEncode::HasExtendedTimestamp()
{
    return m_hasExtended;
}

void RtmpHeaderEncode::Dump()
{
    for (int i = 0; i < m_byteStream.Pos(); ++i)
        printf("%x ", m_byteStream.Data()[i] & 0xff);
    printf("\n");
}

unsigned int RtmpHeaderEncode::GetFmt(
    const ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader)
{
    unsigned int fmt = 0;
    if (!lastMsgHeader)
        return fmt;

    if (msgHeader->streamId == lastMsgHeader->streamId)
        fmt += 1;
    else
        return fmt;

    if (msgHeader->length == lastMsgHeader->length &&
        msgHeader->typeId == lastMsgHeader->typeId)
        fmt += 1;
    else
        return fmt;

    if (msgHeader->timestamp == lastMsgHeader->timestamp)
        fmt += 1;

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
    const ChunkMsgHeader *msgHeader,
    const ChunkMsgHeader *lastMsgHeader,
    bool lastHasExtended)
{
    unsigned int delta = 0;
    switch (fmt)
    {
    case 0:
        if (msgHeader->timestamp >= 0x00ffffff)
            m_byteStream.Write3Bytes(0x00ffffff);
        else
            m_byteStream.Write3Bytes(msgHeader->timestamp);
        m_byteStream.Write3Bytes(msgHeader->length);
        m_byteStream.Write1Bytes(msgHeader->typeId);
        m_byteStream.Write4BytesKeepOriOrder(msgHeader->streamId);

        if (msgHeader->timestamp >= 0x00ffffff)
        {
            m_hasExtended = true;
            m_byteStream.Write4Bytes(msgHeader->timestamp);
        }
        
        break;

    case 1:
        delta = msgHeader->timestamp - lastMsgHeader->timestamp;
        if (delta >= 0x00ffffff)
            m_byteStream.Write3Bytes(0x00ffffff);
        else
            m_byteStream.Write3Bytes(delta);
        m_byteStream.Write3Bytes(msgHeader->length);
        m_byteStream.Write1Bytes(msgHeader->typeId);

        if (delta >= 0x00ffffff)
        {
            m_hasExtended = true;
            m_byteStream.Write4Bytes(delta);
        }

        break;

    case 2:
        delta = msgHeader->timestamp - lastMsgHeader->timestamp;
        if (delta >= 0x00ffffff)
            m_byteStream.Write3Bytes(0x00ffffff);
        else
            m_byteStream.Write3Bytes(delta);

        if (delta >= 0x00ffffff)
        {
            m_hasExtended = true;
            m_byteStream.Write4Bytes(delta);
        }

        break;

    case 3:
        if (lastHasExtended)
            m_byteStream.Write4Bytes(msgHeader->timestamp);

        break;

    default:
        break;
    }
}
