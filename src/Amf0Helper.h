#ifndef AMF0_HELPER_H
#define AMF0_HELPER_H

#include "libamfx/amf.h"
#include <map>

typedef std::map<std::string, std::string> Amf0Obj;

class Amf0Helper
{
public:
    Amf0Helper(char *data, size_t len)
    {
        m_decoder.buf = std::string(data, len);
        m_decoder.pos = 0;
        m_decoder.version = 0;
    }

    size_t GetLeftSize()
    {
        return m_decoder.buf.size() - m_decoder.pos;
    }

    char *GetData()
    {
        if(m_decoder.pos < m_decoder.buf.size())
            return &m_decoder.buf[m_decoder.pos];
        return 0;
    }

    std::string GetString()
    {
        if(m_value.type() == AMF_STRING)
            return m_value.as_string();
        return std::string();
    }

    int64_t GetNumber()
    {
        if (m_value.type() == AMF_NUMBER)
            return m_value.as_number();
        return 0;
    }

    Amf0Obj GetObj()
    {
        Amf0Obj obj;
        if (m_value.type() == AMF_OBJECT)
        {
            amf_object_t amfObj = m_value.as_object();
            amf_object_t::iterator it = amfObj.begin();
            for (; it != amfObj.end(); ++it)
            {
                if (it->second.type() != AMF_STRING)
                    continue;

                obj.insert(std::make_pair(it->first, it->second.as_string()));
            }
        }
        return obj;
    }

    bool Expect(int type)
    {
        while (m_decoder.pos < m_decoder.buf.size())
        {
            m_value = amf_load(&m_decoder);
            if (m_value.type() != type)
                continue;

            return true;
        }
        return false;
    }

private:
    Decoder m_decoder;
    AMFValue m_value;
};

#endif // AMF0_HELPER_H
