#include "RtmpHeader.h"

int main()
{
    char buff0[3 + 11];
    size_t size = 3 + 11;

    // fmt: 0, csid: 6, timestamp: 0, length: 723, typeId: 9, streamId: 1
    char buff1[] = "\x06\x00\x00\x00\x00\x02\xd3\x09\x01\x00\x00\x00";

    // fmt: 1, csid: 6, timestamp: 33, length: 190, typeId: 9, streamId: 1
    char buff2[] = "\x46\x00\x00\x21\x00\x00\xbe\x09";

    // fmt: 3, csid : 6, timestamp : 33, length : 190, typeId : 9, streamId : 1
    char buff3[] = "\xc6";

    // fmt: 2, csid: 6, timestamp: 67, length: 190, typeId: 9, streamId: 1
    char buff4[] = "\x86\x00\x00\x22";

    RtmpHeaderDecode decoder;
    RtmpHeaderEncode encoder;

    decoder.Decode(buff1, sizeof(buff1) - 1, true);
    decoder.Dump();

    size = 14;
    encoder.Encode(buff0, &size, decoder.GetBasicHeader().csId,
                   decoder.GetMsgHeader());
    encoder.Dump();

    decoder.Decode(buff2, sizeof(buff2) - 1, true);
    decoder.Dump();

    size = 14;
    encoder.Encode(buff0, &size, decoder.GetBasicHeader().csId,
                   decoder.GetMsgHeader());
    encoder.Dump();

    decoder.Decode(buff3, sizeof(buff3) - 1, false);
    decoder.Dump();

    size = 14;
    encoder.Encode(buff0, &size, decoder.GetBasicHeader().csId,
                   decoder.GetMsgHeader());
    encoder.Dump();

    decoder.Decode(buff4, sizeof(buff4) - 1, true);
    decoder.Dump();

    size = 14;
    encoder.Encode(buff0, &size, decoder.GetBasicHeader().csId,
                   decoder.GetMsgHeader());
    encoder.Dump();

    return 0;
}
