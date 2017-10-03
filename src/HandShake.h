#ifndef HAND_SHAKE_H
#define HAND_SHAKE_H
#include "boost/function.hpp"
#include <arpa/inet.h>

enum HandShakeState
{
    HandShakeState_Ok = 0,
    HandShakeState_NotEnoughData,
    HandShakeState_Error
};

class HandShake
{
public:
    typedef boost::function<void(const char *data, size_t len)> OnC0C1;
    typedef boost::function<void(const char *data, size_t len)> OnS0S1S2;
    typedef boost::function<void(const char *data, size_t len)> OnC2;

    HandShake()
        : m_complete(false), m_waitC2(false), m_S0S1S2(0)
    {
        m_S0S1S2 = new char[kS0S1S2Size];
    }

    ~HandShake()
    {
        delete[]m_S0S1S2;
    }

    HandShakeState Parse(char *data, size_t len)
    {
        if (m_waitC2)
        {
            if (len < kC0C1C2Size)
                return HandShakeState_NotEnoughData;
            if (m_onC2)
                m_onC2(data, kC0C1C2Size);
            m_complete = true;
            return HandShakeState_Ok;
        }

        if (!data || len < kC0C1Size)
            return HandShakeState_NotEnoughData;

        if (data[0] != kVersion)
            return HandShakeState_Error;

        if (m_onC0C1)
        {
            m_onC0C1(data, kC0C1Size);
        }

        SetS0S1S2(data);

        if (m_onS0S1S2)
            m_onS0S1S2(m_S0S1S2, kS0S1S2Size);

        m_waitC2 = true;
        return HandShakeState_Ok;
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
    static const int kC0C1C2Size = 1537 + 1536;

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

        // Server time
        *(int*)&(m_S0S1S2[1]) = htonl(time(0));

        // Client time
        *(int*)&(m_S0S1S2[5]) = *(int*)&(C0C1[1]);
    }

    bool m_complete;
    bool m_waitC2;
    char *m_S0S1S2;

    OnC0C1 m_onC0C1;
    OnS0S1S2 m_onS0S1S2;
    OnC2 m_onC2;
};

#endif // HAND_SHAKE_H
