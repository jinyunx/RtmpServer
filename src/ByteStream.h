#ifndef BYTE_STREAM_H
#define BYTE_STREAM_H

#include <sys/types.h>
#include <string>

// input data is bit-endian(network byte order), output is little-endian
// Read4BytesKeepOriOrder keep the input order

/**
* m_bytes utility, used to:
* convert basic types to m_bytes,
* build basic types from m_bytes.
*/
class ByteStream
{
public:
    ByteStream();
    ~ByteStream();

    /**
    * initialize the stream from m_bytes.
    * @b, the m_bytes to convert from/to basic types.
    * @nb, the size of m_bytes, total number of m_bytes for stream.
    * @remark, stream never free the m_bytes, user must free it.
    * @remark, return error when m_bytes NULL.
    * @remark, return error when size is not positive.
    */
    bool Initialize(char *buf, int size);

    /**
    * get data of stream, set by initialize.
    * current m_bytes = data() + m_pos()
    */
    char* Data();
    /**
    * the total stream size, set by initialize.
    * left m_bytes = size() - m_pos().
    */
    int Size();
    /**
    * tell the current m_pos.
    */
    int Pos();
    /**
    * whether stream is empty.
    * if empty, user should never read or write.
    */
    bool Empty();
    /**
    * whether required size is ok.
    * @return true if stream can read/write specified required_size m_bytes.
    * @remark assert required_size positive.
    */
    bool Require(int required_size);

    /**
    * to skip some size.
    * @param size can be any value. positive to forward; nagetive to backward.
    * @remark to skip(m_pos()) to reset stream.
    * @remark assert initialized, the data() not NULL.
    */
    void Skip(int size);

    /**
    * get 1bytes char from stream.
    */
    int8_t Read1Bytes();
    /**
    * get 2bytes int from stream.
    */
    int16_t Read2Bytes();
    /**
    * get 3bytes int from stream.
    */
    int32_t Read3Bytes();
    /**
    * get 4bytes int from stream.
    */
    int32_t Read4Bytes();
    /**
    * get 4bytes int from stream.
    */
    int32_t Read4BytesKeepOriOrder();
    /**
    * get 8bytes int from stream.
    */
    int64_t Read8Bytes();
    /**
    * get string from stream, length specifies by param len.
    */
    std::string ReadString(int len);
    /**
    * get m_bytes from stream, length specifies by param len.
    */
    void ReadBytes(char* data, int size);

    /**
    * write 1bytes char to stream.
    */
    void Write1Bytes(int8_t value);
    /**
    * write 2bytes int to stream.
    */
    void Write2Bytes(int16_t value);
    /**
    * write 4bytes int to stream.
    */
    void Write4Bytes(int32_t value);
    /**
    * write 3bytes int to stream.
    */
    void Write3Bytes(int32_t value);
    /**
    * write 8bytes int to stream.
    */
    void Write8Bytes(int64_t value);
    /**
    * write string to stream
    */
    void WriteString(std::string value);
    /**
    * write m_bytes to stream
    */
    void WriteBytes(char* data, int size);

private:
    // current position at m_bytes.
    char* m_pos;
    // the m_bytes data for stream to read or write.
    char* m_bytes;
    // the total number of m_bytes.
    int m_size;

};

/**
 * the bit stream.
 */
class BitStream
{

public:
    BitStream();
    ~BitStream();

    bool Initialize(ByteStream* s);
    bool Empty();
    int8_t ReadBit();

private:
    int8_t cb;
    u_int8_t cb_left;
    ByteStream* stream;
};

#endif // BYTE_STREAM_H
