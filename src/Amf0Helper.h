#ifndef AMF0_HELPER_H
#define AMF0_HELPER_H

#include "libamf/amf0.h"
#include <map>

typedef std::map<std::string, std::string> Amf0Obj;

class Amf0Helper
{
public:
    Amf0Helper(char *data, size_t len)
        : m_data(data), m_len(len), m_left(len), m_amfData(0)
    { }

    ~Amf0Helper()
    {
        FreeAmfData();
    }

    size_t GetLeftSize()
    {
        return m_left;
    }

    char *GetData()
    {
        return m_data;
    }

    amf0_data *GetAmf0Data()
    {
        return m_amfData;
    }

    std::string GetString()
    {
        if(m_amfData && m_amfData->type == AMF0_TYPE_STRING)
            return std::string((char *)m_amfData->string_data.mbstr,
                               m_amfData->string_data.size);

        return std::string();
    }

    int64_t GetNumber()
    {
        if (m_amfData && m_amfData->type == AMF0_TYPE_NUMBER)
            return m_amfData->number_data;
        return 0;
    }

    Amf0Obj GetObj()
    {
        Amf0Obj obj;
        if (m_amfData && m_amfData->type == AMF0_TYPE_OBJECT)
        {
            amf0_node * node = amf0_object_first(m_amfData);
            while (node != 0)
            {
                amf0_data *k = amf0_object_get_name(node);
                amf0_data *v = amf0_object_get_data(node);
                node = amf0_object_next(node);

                if (k->type != AMF0_TYPE_STRING ||
                    v->type != AMF0_TYPE_STRING)
                    continue;

                std::string key((char *)k->string_data.mbstr, k->string_data.size);
                std::string value((char *)v->string_data.mbstr, v->string_data.size);
                obj.insert(std::make_pair(key, value));
            }
        }
        return obj;
    }

    bool Expect(int type)
    {
        FreeAmfData();

        while (m_left > 0)
        {
            FreeAmfData();

            size_t nread= 0;
            m_amfData = amf0_data_buffer_read(
                    (uint8_t *)m_data, m_left, &nread);

            if (!m_amfData)
                return false;

            m_data += nread;
            m_left -= nread;

            if (m_amfData->type != type)
                continue;

            return true;
        }
        return false;
    }

    void FreeAmfData()
    {
        if (m_amfData)
            amf0_data_free(m_amfData);
        m_amfData = 0;
    }

private:
    char *m_data;
    size_t m_len;
    size_t m_left;
    amf0_data *m_amfData;
};

#endif // AMF0_HELPER_H
