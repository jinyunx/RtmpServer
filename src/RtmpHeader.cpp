#include "RtmpHeader.h"
#include <stdio.h>

ParseState RtmpHeader::Parse(char *data, size_t len,
                             const ChuckMsgHeader *lastMsgHeader,
                             bool lastHasExtended)
{
    if (!m_byteStream.Initialize(data, len))
        return ParseState_NotEnoughData;

    ParseState state = ParseBasicHeader();
    if (state != ParseState_Ok)
        return state;

    state = ParseMsgHeader(lastMsgHeader);
    if (state != ParseState_Ok)
        return state;

    state = ParseExtenedTimestamp(lastMsgHeader, lastHasExtended);
    if (state == ParseState_Ok)
        m_complete = true;

    return state;
}

ChuckMsgHeader RtmpHeader::GetMsgHeader()
{
    return m_msgHeader;
}

bool RtmpHeader::IsComplete()
{
    return m_complete;
}

bool RtmpHeader::HasExtenedTimestamp()
{
    return m_hasExtenedTimestamp;
}

void RtmpHeader::Reset()
{
    m_complete = false;
    m_hasExtenedTimestamp = false;
    m_basicHeader.Reset();
    m_msgHeader.Reset();
    m_extenedTimestamp.Reset();
    m_byteStream.Initialize(0, 0);
}

void RtmpHeader::Dump()
{
    fprintf(stdout, "fmt: %d, csid: %d, timestamp: %d,"
            " length: %d, typeId: %d, streamId: %d\n",
            m_basicHeader.fmt, m_basicHeader.csId, m_msgHeader.timestamp,
            m_msgHeader.length, m_msgHeader.typeId, m_msgHeader.streamId);
}

ParseState RtmpHeader::ParseBasicHeader()
{
    if (!m_byteStream.Require(1))
        return ParseState_NotEnoughData;

    char oneByte = m_byteStream.Read1Bytes();
    m_basicHeader.csId = static_cast<unsigned int>(oneByte & 0x3f);
    m_basicHeader.fmt = static_cast<unsigned int>(oneByte >> 6 & 0x03);

    switch (m_basicHeader.csId)
    {
    case 0:
        if (!m_byteStream.Require(1))
            return ParseState_NotEnoughData;

        m_basicHeader.csId =
            64 + static_cast<unsigned int>(m_byteStream.Read1Bytes());
        return ParseState_Ok;

    case 1:
        if (!m_byteStream.Require(2))
            return ParseState_NotEnoughData;

        m_basicHeader.csId = 64 +
            static_cast<unsigned int>(m_byteStream.Read1Bytes()) +
            static_cast<unsigned int>(m_byteStream.Read1Bytes()) * 256;
        return ParseState_Ok;

    default:
        return ParseState_Ok;
    }
}

ParseState RtmpHeader::ParseMsgHeader(const ChuckMsgHeader *lastMsgHeader)
{
    if (m_basicHeader.fmt > 0 && !lastMsgHeader)
        return ParseState_Error;

    switch (m_basicHeader.fmt)
    {
    case 0:
        if (!m_byteStream.Require(11))
            return ParseState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        // Inputed stream id is little endian already
        m_msgHeader.streamId =
            static_cast<unsigned int>(m_byteStream.Read4BytesKeepOriOrder());
        return ParseState_Ok;

    case 1:
        if (!m_byteStream.Require(7))
            return ParseState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = m_byteStream.Read3Bytes();
        m_msgHeader.typeId =
            static_cast<unsigned int>(m_byteStream.Read1Bytes());
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return ParseState_Ok;

    case 2:
        if (!m_byteStream.Require(3))
            return ParseState_NotEnoughData;

        HandleTimestamp(lastMsgHeader);
        m_msgHeader.length = lastMsgHeader->length;
        m_msgHeader.typeId = lastMsgHeader->typeId;
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return ParseState_Ok;

    case 3:
        m_msgHeader.timestamp = lastMsgHeader->timestamp;
        m_msgHeader.length = lastMsgHeader->length;
        m_msgHeader.typeId = lastMsgHeader->typeId;
        m_msgHeader.streamId = lastMsgHeader->streamId;
        return ParseState_Ok;

    default:
        return ParseState_Error;
    }
}

ParseState RtmpHeader::ParseExtenedTimestamp(const ChuckMsgHeader *lastMsgHeader,
                                             bool lastHasExtended)
{

    if (m_hasExtenedTimestamp)
    {
        if (!m_byteStream.Require(4))
            return ParseState_NotEnoughData;

        m_extenedTimestamp.t = m_byteStream.Read4Bytes();
        m_msgHeader.timestamp = m_extenedTimestamp.t;
    }
    // See https://forums.adobe.com/thread/542749
    else if (m_basicHeader.fmt == 3 && lastHasExtended)
    {
        if (!m_byteStream.Require(4))
            return ParseState_NotEnoughData;

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
    return ParseState_Ok;
}

void RtmpHeader::HandleTimestamp(const ChuckMsgHeader *lastMsgHeader)
{
    m_msgHeader.timestamp = m_byteStream.Read3Bytes();
    if (m_msgHeader.timestamp >= 0x00ffffff)
        m_hasExtenedTimestamp = true;
    else if (m_basicHeader.fmt != 0)
        m_msgHeader.timestamp += lastMsgHeader->timestamp;
}
