#ifndef RTMP_HEADER_H
#define RTMP_HEADER_H

#include "ByteStream.h"
#include <map>

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
    // tsField: timestamp field, it would be
    // 1. absolute time
    // 2. delta time
    // 3. 0xFFFFFF
    unsigned int tsField;
    unsigned int length;
    unsigned int typeId;
    unsigned int streamId;

    // the absolute time of message
    unsigned int timestamp;

    ChunkMsgHeader()
        : tsField(0), length(0), typeId(0),
          streamId(0), timestamp(0)
    { }

    void Reset()
    {
        tsField = 0;
        length = 0;
        typeId = 0;
        streamId = 0;
        timestamp = 0;
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

typedef std::map<int, ChunkMsgHeader> CsIdMsgHeader;
typedef std::map<int, bool> CsIdHasExtended;

class RtmpHeaderDecode
{
public:
    RtmpHeaderDecode()
        : m_complete(false), m_hasExtenedTimestamp(false)
    { }

    RtmpHeaderState Decode(char *data, size_t len, bool startNewMsg);
    RtmpHeaderState DecodeCsId(char *data, size_t len);

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
    RtmpHeaderState DecodeMsgHeader(bool startNewMsg);

    bool m_complete;
    bool m_hasExtenedTimestamp;
    ChunkBasicHeader m_basicHeader;
    ChunkMsgHeader m_msgHeader;
    ExtendedTimestamp m_extenedTimestamp;
    ByteStream m_byteStream;

    CsIdMsgHeader m_csIdMsgHeader;
};

class RtmpHeaderEncode
{
public:
    RtmpHeaderEncode();

    RtmpHeaderState Encode(char *data, size_t *len,
                           unsigned int csId,
                           const ChunkMsgHeader &msgHeader);

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

    CsIdMsgHeader m_csIdMsgHeader;
    CsIdHasExtended m_csIdHasExtended;
};

#endif // RTMP_HEADER_H
