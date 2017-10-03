#include "libamf/amf0.h"
#include <string.h>

int main()
{
    uint8_t buffer[] = "\x02\x00\x08\x6f\x6e\x53\x74\x61\x74\x75\x73";

    for (unsigned int i = 0; i < sizeof(buffer); ++i)
        printf("%x ", buffer[i]);
    printf("\n");

    amf0_data *amfData = amf0_data_buffer_read(buffer, sizeof(buffer));
    fprintf(stdout, "type: %d\n", amf0_data_get_type(amfData));
    amf0_data_dump(stdout, amfData, 0);
    printf("\n");
    return 0;
}
