#include"ByteStream.h"
#include <stdio.h>
#include <string.h>

ByteStream::ByteStream()
{
    m_pos = m_bytes = NULL;
    m_size = 0;
}

ByteStream::~ByteStream()
{
}

bool ByteStream::Initialize(char *buf, int size)
{
    if (!buf || size == 0)
        return false;

    m_size = size;
    m_pos = m_bytes = buf;

    return true;
}

char *ByteStream::Data()
{
    return m_bytes;
}

int ByteStream::Size()
{
    return m_size;
}

int ByteStream::Pos()
{
    return (int)(m_pos - m_bytes);
}

bool ByteStream::Empty()
{
    return !m_bytes || (m_pos >= m_bytes + m_size);
}

bool ByteStream::Require(int required_size)
{
    return required_size <= m_size - (m_pos - m_bytes);
}

void ByteStream::Skip(int size)
{
    m_pos += size;
}

int8_t ByteStream::Read1Bytes()
{
    return (int8_t)*m_pos++;
}

int16_t ByteStream::Read2Bytes()
{
    int16_t value;
    char* pp = (char*)&value;
    pp[1] = *m_pos++;
    pp[0] = *m_pos++;

    return value;
}

int32_t ByteStream::Read3Bytes()
{
    int32_t value = 0;
    char* pp = (char*)&value;
    pp[2] = *m_pos++;
    pp[1] = *m_pos++;
    pp[0] = *m_pos++;
    
    return value;
}

int32_t ByteStream::Read4Bytes()
{
    int32_t value = 0;
    char* pp = (char*)&value;
    pp[3] = *m_pos++;
    pp[2] = *m_pos++;
    pp[1] = *m_pos++;
    pp[0] = *m_pos++;

    return value;
}

int32_t ByteStream::Read4BytesKeepOriOrder()
{
    int32_t value = 0;
    char* pp = (char*)&value;
    pp[0] = *m_pos++;
    pp[1] = *m_pos++;
    pp[2] = *m_pos++;
    pp[3] = *m_pos++;

    return value;
}

int64_t ByteStream::Read8Bytes()
{
    int64_t value = 0;
    char* pp = (char*)&value;
    pp[7] = *m_pos++;
    pp[6] = *m_pos++;
    pp[5] = *m_pos++;
    pp[4] = *m_pos++;
    pp[3] = *m_pos++;
    pp[2] = *m_pos++;
    pp[1] = *m_pos++;
    pp[0] = *m_pos++;
    
    return value;
}

std::string ByteStream::ReadString(int len)
{
    std::string value;
    value.append(m_pos, len);
    
    m_pos += len;
    
    return value;
}

void ByteStream::ReadBytes(char* data, int size)
{
    memcpy(data, m_pos, size);
    
    m_pos += size;
}

void ByteStream::Write1Bytes(int8_t value)
{
    *m_pos++ = value;
}

void ByteStream::Write2Bytes(int16_t value)
{
    char* pp = (char*)&value;
    *m_pos++ = pp[1];
    *m_pos++ = pp[0];
}

void ByteStream::Write4Bytes(int32_t value)
{
    char* pp = (char*)&value;
    *m_pos++ = pp[3];
    *m_pos++ = pp[2];
    *m_pos++ = pp[1];
    *m_pos++ = pp[0];
}

void ByteStream::Write4BytesKeepOriOrder(int32_t value)
{
    char* pp = (char*)&value;
    *m_pos++ = pp[0];
    *m_pos++ = pp[1];
    *m_pos++ = pp[2];
    *m_pos++ = pp[3];
}

void ByteStream::Write3Bytes(int32_t value)
{
    char* pp = (char*)&value;
    *m_pos++ = pp[2];
    *m_pos++ = pp[1];
    *m_pos++ = pp[0];
}

void ByteStream::Write8Bytes(int64_t value)
{
    char* pp = (char*)&value;
    *m_pos++ = pp[7];
    *m_pos++ = pp[6];
    *m_pos++ = pp[5];
    *m_pos++ = pp[4];
    *m_pos++ = pp[3];
    *m_pos++ = pp[2];
    *m_pos++ = pp[1];
    *m_pos++ = pp[0];
}

void ByteStream::WriteString(std::string value)
{
    memcpy(m_pos, value.data(), value.length());
    m_pos += value.length();
}

void ByteStream::WriteBytes(char* data, int size)
{
    memcpy(m_pos, data, size);
    m_pos += size;
}

BitStream::BitStream()
{
    cb = 0;
    cb_left = 0;
    stream = NULL;
}

BitStream::~BitStream()
{
}

bool BitStream::Initialize(ByteStream* s)
{
    stream = s;
    return true;
}

bool BitStream::Empty()
{
    if (cb_left)
    {
        return false;
    }
    return stream->Empty();
}

int8_t BitStream::ReadBit()
{
    if (!cb_left)
    {
        cb = stream->Read1Bytes();
        cb_left = 8;
    }
    
    int8_t v = (cb >> (cb_left - 1)) & 0x01;
    cb_left--;
    return v;
}
