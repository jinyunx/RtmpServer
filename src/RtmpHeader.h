#ifndef RTMP_HEADER_H
#define RTMP_HEADER_H

#include "ByteStream.h"

enum RtmpHeaderState
{
    RtmpHeaderState_Ok = 0,
    RtmpHeaderState_NotEnoughData,
    RtmpHeaderState_Error,
};

struct ChunkBasicHeader
{
    unsigned int fmt;
    unsigned int csId;

    ChunkBasicHeader()
        : fmt(0), csId(0)
    { }

    void Reset()
    {
        fmt = 0;
        csId = 0;
    }
};

struct ChunkMsgHeader
{
    unsigned int timestamp;
    unsigned int length;
    unsigned int typeId;
    unsigned int streamId;

    ChunkMsgHeader()
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
                           const ChunkMsgHeader *lastMsgHeader,
                           bool lastHasExtended);

    ChunkMsgHeader GetMsgHeader() const;
    ChunkBasicHeader GetBasicHeader() const;
    ExtendedTimestamp GetExtendedTimestamp() const;
    int GetConsumeDataLen();

    bool IsComplete();
    bool HasExtenedTimestamp();
    void Reset();

    void Dump();

private:
    RtmpHeaderState DecodeBasicHeader();
    RtmpHeaderState DecodeMsgHeader(const ChunkMsgHeader *lastMsgHeader);
    RtmpHeaderState DecodeExtenedTimestamp(const ChunkMsgHeader *lastMsgHeader,
                                           bool lastHasExtended);

    void HandleTimestamp(const ChunkMsgHeader *lastMsgHeader);

    bool m_complete;
    bool m_hasExtenedTimestamp;
    ChunkBasicHeader m_basicHeader;
    ChunkMsgHeader m_msgHeader;
    ExtendedTimestamp m_extenedTimestamp;
    ByteStream m_byteStream;
};

class RtmpHeaderEncode
{
public:
    RtmpHeaderEncode();

    RtmpHeaderState Encode(char *data, size_t *len,
                           unsigned int csId,
                           const ChunkMsgHeader *msgHeader,
                           const ChunkMsgHeader *lastMsgHeader,
                           bool lastHasExtended);

    bool HasExtendedTimestamp();

    void Dump();

private:
    unsigned int GetFmt(const ChunkMsgHeader *msgHeader,
                        const ChunkMsgHeader *lastMsgHeader);

    void SetBasicHeader(unsigned int fmt, unsigned int csIdchar);
    void SetMsgHeader(unsigned int fmt,
                      const ChunkMsgHeader *msgHeader,
                      const ChunkMsgHeader *lastMsgHeader,
                      bool lastHasExtended);

    static const int kMaxBytes = 3 + 11;

    bool m_hasExtended;
    ByteStream m_byteStream;
};

#endif // RTMP_HEADER_H
