#ifndef RTMP_HEADER_H
#define RTMP_HEADER_H

#include "ByteStream.h"

enum ParseState
{
    ParseState_Ok = 0,
    ParseState_NotEnoughData,
    ParseState_Error,
};

struct ChuckBasicHeader
{
    unsigned int fmt;
    unsigned int csId;

    ChuckBasicHeader()
        : fmt(0), csId(0)
    { }

    void Reset()
    {
        fmt = 0;
        csId = 0;
    }
};

struct ChuckMsgHeader
{
    unsigned int timestamp;
    unsigned int length;
    unsigned int typeId;
    unsigned int streamId;

    ChuckMsgHeader()
        : timestamp(0), length(0), typeId(0),
          streamId(0)
    { }

    void Reset()
    {
        timestamp = 0;
        length = 0;
        typeId = 0;
        streamId = 0;
    }
};

struct ExtendedTimestamp
{
    unsigned int t;

    ExtendedTimestamp()
        : t(0)
    { }

    void Reset()
    {
        t = 0;
    }
};

class RtmpHeader
{
public:
    RtmpHeader()
        : m_complete(false), m_hasExtenedTimestamp(false)
    { }

    ParseState Parse(char *data, size_t len,
                     const ChuckMsgHeader *lastMsgHeader,
                     bool lastHasExtended);

    ChuckMsgHeader GetMsgHeader();

    bool IsComplete();
    bool HasExtenedTimestamp();
    void Reset();

    void Dump();

private:
    ParseState ParseBasicHeader();
    ParseState ParseMsgHeader(const ChuckMsgHeader *lastMsgHeader);
    ParseState ParseExtenedTimestamp(const ChuckMsgHeader *lastMsgHeader,
                                     bool lastHasExtended);

    void HandleTimestamp(const ChuckMsgHeader *lastMsgHeader);

    bool m_complete;
    bool m_hasExtenedTimestamp;
    ChuckBasicHeader m_basicHeader;
    ChuckMsgHeader m_msgHeader;
    ExtendedTimestamp m_extenedTimestamp;
    ByteStream m_byteStream;
};

#endif // RTMP_HEADER_H
