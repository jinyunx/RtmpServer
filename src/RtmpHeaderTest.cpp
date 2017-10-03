#include "RtmpHeader.h"

int main()
{
    // fmt: 0, csid: 6, timestamp: 0, length: 723, typeId: 9, streamId: 1
    char buff1[] = "\x06\x00\x00\x00\x00\x02\xd3\x09\x01\x00\x00\x00";

    // fmt: 1, csid: 6, timestamp: 33, length: 190, typeId: 9, streamId: 1
    char buff2[] = "\x46\x00\x00\x21\x00\x00\xbe\x09";

    // fmt: 3, csid : 6, timestamp : 33, length : 190, typeId : 9, streamId : 1
    char buff3[] = "\xc6";

    // fmt: 2, csid: 6, timestamp: 67, length: 190, typeId: 9, streamId: 1
    char buff4[] = "\x86\x00\x00\x22";

    ChuckMsgHeader lastMsgHeader;
    bool lastHasExtended = false;

    RtmpHeader rtmpHeader;
    rtmpHeader.Parse(buff1, sizeof(buff1) - 1, 0, false);
    rtmpHeader.Dump();

    lastMsgHeader = rtmpHeader.GetMsgHeader();
    lastHasExtended = rtmpHeader.HasExtenedTimestamp();
    rtmpHeader.Reset();
    
    rtmpHeader.Parse(buff2, sizeof(buff2) - 1, &lastMsgHeader, lastHasExtended);
    rtmpHeader.Dump();

    lastMsgHeader = rtmpHeader.GetMsgHeader();
    lastHasExtended = rtmpHeader.HasExtenedTimestamp();
    rtmpHeader.Reset();

    rtmpHeader.Parse(buff3, sizeof(buff3) - 1, &lastMsgHeader, lastHasExtended);
    rtmpHeader.Dump();

    lastMsgHeader = rtmpHeader.GetMsgHeader();
    lastHasExtended = rtmpHeader.HasExtenedTimestamp();
    rtmpHeader.Reset();

    rtmpHeader.Parse(buff4, sizeof(buff4) - 1, &lastMsgHeader, lastHasExtended);
    rtmpHeader.Dump();
    return 0;
}
