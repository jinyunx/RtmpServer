#ifndef RTMP_HEADER_H
#define RTMP_HEADER_H

/*
+--------------+----------------+--------------------+--------------+
| Basic Header | Message Header | Extended Timestamp | Chunk Data   |
+--------------+----------------+--------------------+--------------+
|                                                    |
|<------------------- Chunk Header ----------------->|

#####################################################################

 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+
|fmt| cs id     |
+-+-+-+-+-+-+-+-+
Chunk basic header 1

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|fmt| 0         | cs id - 64    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk basic header 2

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|fmt| 1         |         cs id - 64            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk basic header 3

#####################################################################

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| timestamp                                     |message length |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| message length (cont)       |message type id  | msg stream id |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| message stream id (cont)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk Message Header - fmt 0

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| timestamp delta                               |message length |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| message length (cont)         |message type id|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk Message Header - fmt 1

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| timestamp delta                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk Message Header - fmt 2

 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Chunk Message Header - fmt 3

*/

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
    // 2. delta time (if fmt != 0)
    // 3. 0xFFFFFF
    unsigned int tsField;

    unsigned int length;

    // video type or audio type and so on
    unsigned int typeId;
    unsigned int streamId;

    // the absolute time of message
    unsigned int timestamp;

    // externed timestamp
    unsigned int externedTimestamp;

    ChunkMsgHeader()
        : tsField(0), length(0), typeId(0),
          streamId(0), timestamp(0), externedTimestamp(0)
    { }

    void Reset()
    {
        tsField = 0;
        length = 0;
        typeId = 0;
        streamId = 0;
        timestamp = 0;
        externedTimestamp = 0;
    }

    bool operator !=(const ChunkMsgHeader &header) const
    {
        return (tsField != header.tsField ||
                length != header.length ||
                typeId != header.typeId ||
                streamId != header.streamId ||
                timestamp != header.timestamp ||
                externedTimestamp != header.externedTimestamp);
    }
};

typedef std::map<int, ChunkMsgHeader> CsIdMsgHeader;

class RtmpHeaderDecode
{
public:
    RtmpHeaderDecode()
        : m_complete(false)
    { }

    RtmpHeaderState Decode(char *data, size_t len, bool startNewMsg);
    RtmpHeaderState DecodeCsId(char *data, size_t len);

    ChunkMsgHeader GetMsgHeader() const;
    ChunkBasicHeader GetBasicHeader() const;
    int GetConsumeDataLen();

    bool IsComplete();
    void Reset();

    void Dump();

private:
    RtmpHeaderState DecodeBasicHeader();
    RtmpHeaderState DecodeMsgHeader(bool startNewMsg);

    bool m_complete;
    ChunkBasicHeader m_basicHeader;
    ChunkMsgHeader m_msgHeader;
    ByteStream m_byteStream;

    CsIdMsgHeader m_csIdMsgHeader;
};

class RtmpHeaderEncode
{
public:
    RtmpHeaderEncode();

    // Would modify tsField and externedTimestamp
    // in msgHeader
    RtmpHeaderState Encode(char *data, size_t *len,
                           unsigned int csId,
                           ChunkMsgHeader &msgHeader,
                           bool startNewMsg);

    void Dump();

private:
    RtmpHeaderState EncodeStartMsg(
        unsigned int csId, ChunkMsgHeader *msgHeader,
        const ChunkMsgHeader *lastMsgHeader);
    RtmpHeaderState EncodeInterMsg(
        unsigned int csId, const ChunkMsgHeader *msgHeader);

    bool IsFmt0(const ChunkMsgHeader *msgHeader,
                const ChunkMsgHeader *lastMsgHeader);
    void SetTsField(ChunkMsgHeader *msgHeader,
                    const ChunkMsgHeader *lastMsgHeader);
    unsigned int GetFmt(const ChunkMsgHeader *msgHeader,
                        const ChunkMsgHeader *lastMsgHeader);

    void SetBasicHeader(unsigned int fmt, unsigned int csIdchar);
    void SetMsgHeader(unsigned int fmt,
                      const ChunkMsgHeader *msgHeader);

    static const int kMaxBytes = 3 + 11;

    ByteStream m_byteStream;

    CsIdMsgHeader m_csIdMsgHeader;
};

#endif // RTMP_HEADER_H
