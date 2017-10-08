#include "libamf/amf0.h"
#include <string.h>

int main()
{
    uint8_t buffer[] = "\x03\x00\x03\x61\x70\x70\x02\x00\x04\x6c\x69"
                       "\x76\x65\x00\x04\x74\x79\x70\x65\x02\x00\x0a"
                       "\x6e\x6f\x6e\x70\x72\x69\x76\x61\x74\x65\x00"
                       "\x05\x74\x63\x55\x72\x6c\x02\x00\x1e\x72\x74"
                       "\x6d\x70\x3a\x2f\x2f\x31\x32\x33\x2e\x32\x30"
                       "\x37\x2e\x39\x32\x2e\x35\x36\x3a\x31\x39\x33"
                       "\x35\x2f\x6c\x69\x76\x65\x00\x00\x09";

    for (unsigned int i = 0; i < sizeof(buffer); ++i)
        printf("%x ", buffer[i]);
    printf("\n");

    size_t nread = 0;
    amf0_data *amfData = amf0_data_buffer_read(buffer, sizeof(buffer), &nread);
    fprintf(stdout, "type: %d\n", amf0_data_get_type(amfData));
    amf0_data_dump(stdout, amfData, 0);
    printf("\n");

    return 0;
}
