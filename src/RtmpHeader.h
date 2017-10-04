#ifndef RTMP_HEADER_H
#define RTMP_HEADER_H

#include "ByteStream.h"

enum RtmpHeaderState
{
    RtmpHeaderState_Ok = 0,
    RtmpHeaderState_NotEnoughData,
    RtmpHeaderState_Error,
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

class RtmpHeaderDecode
{
public:
    RtmpHeaderDecode()
        : m_complete(false), m_hasExtenedTimestamp(false)
    { }

    RtmpHeaderState Decode(char *data, size_t len,
                           const ChuckMsgHeader *lastMsgHeader,
                           bool lastHasExtended);

    ChuckMsgHeader GetMsgHeader();
    ChuckBasicHeader GetBasicHeader();

    bool IsComplete();
    bool HasExtenedTimestamp();
    void Reset();

    void Dump();

private:
    RtmpHeaderState DecodeBasicHeader();
    RtmpHeaderState DecodeMsgHeader(const ChuckMsgHeader *lastMsgHeader);
    RtmpHeaderState DecodeExtenedTimestamp(const ChuckMsgHeader *lastMsgHeader,
                                           bool lastHasExtended);

    void HandleTimestamp(const ChuckMsgHeader *lastMsgHeader);

    bool m_complete;
    bool m_hasExtenedTimestamp;
    ChuckBasicHeader m_basicHeader;
    ChuckMsgHeader m_msgHeader;
    ExtendedTimestamp m_extenedTimestamp;
    ByteStream m_byteStream;
};

class RtmpHeaderEncode
{
public:
    RtmpHeaderEncode();

    RtmpHeaderState Encode(char *data, size_t *len,
                           unsigned int csId,
                           const ChuckMsgHeader *msgHeader,
                           const ChuckMsgHeader *lastMsgHeader,
                           bool lastHasExtended);

    bool HasExtendedTimestamp();

    void Dump();

private:
    unsigned int GetFmt(const ChuckMsgHeader *msgHeader,
                        const ChuckMsgHeader *lastMsgHeader);

    void SetBasicHeader(unsigned int fmt, unsigned int csIdchar);
    void SetMsgHeader(unsigned int fmt,
                      const ChuckMsgHeader *msgHeader,
                      const ChuckMsgHeader *lastMsgHeader,
                      bool lastHasExtended);

    static const int kMaxBytes = 3 + 11;

    bool m_hasExtended;
    ByteStream m_byteStream;
};

#endif // RTMP_HEADER_H
