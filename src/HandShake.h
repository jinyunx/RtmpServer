#ifndef HAND_SHAKE_H
#define HAND_SHAKE_H
#include "boost/function.hpp"
#include <arpa/inet.h>

typedef boost::function<void(const char *, size_t)> OnC0C1;
typedef boost::function<void(const char *, size_t)> OnS0S1S2;
typedef boost::function<void(const char *, size_t)> OnC2;

class HandShake
{
public:
    HandShake(const OnS0S1S2 & onS0S1S2)
        : m_complete(false), m_waitC2(false), m_S0S1S2(0), m_onS0S1S2(onS0S1S2)
    {
        m_S0S1S2 = new char[kS0S1S2Size];
    }

    ~HandShake()
    {
        delete[]m_S0S1S2;
    }

    int Parse(char *data, size_t len)
    {
        if (m_waitC2)
        {
            if (len < kC2Size)
                return 0;
            if (m_onC2)
                m_onC2(data, kC2Size);
            m_complete = true;
            return kC2Size;
        }

        if (!data || len < kC0C1Size)
            return 0;

        if (data[0] != kVersion)
            return -1;

        if (m_onC0C1)
            m_onC0C1(data, kC0C1Size);

        SetS0S1S2(data);

        if (m_onS0S1S2)
            m_onS0S1S2(m_S0S1S2, kS0S1S2Size);

        m_waitC2 = true;
        return kC0C1Size;
    }

    void SetOnC0C1(const OnC0C1 & onC0C1)
    {
        m_onC0C1 = onC0C1;
    }

    void SetOnS0S1S2(const OnS0S1S2 & onS0S1S2)
    {
        m_onS0S1S2 = onS0S1S2;
    }

    bool IsComplete()
    {
        return m_complete;
    }

private:
    static const int kC0C1Size = 1537;
    static const int kC2Size = 1536;

    static const int kVersion = 3;
    static const int kS0S1S2Size = 1537 + 1536;

    void RadomBytes(char *buf, int len)
    {
        for (int i = 0; i < len; ++i)
            buf[i] = random();
    }

    void SetS0S1S2(char *C0C1)
    {
        RadomBytes(m_S0S1S2, kS0S1S2Size);

        // Version
        m_S0S1S2[0] = kVersion;
        *(int*)&(m_S0S1S2[1]) = 0;
        *(int*)&(m_S0S1S2[5]) = 0;

        memcpy(m_S0S1S2 + kC0C1Size, &C0C1[1], kC0C1Size - 1);
    }

    bool m_complete;
    bool m_waitC2;
    char *m_S0S1S2;

    OnC0C1 m_onC0C1;
    OnS0S1S2 m_onS0S1S2;
    OnC2 m_onC2;
};

#endif // HAND_SHAKE_H
