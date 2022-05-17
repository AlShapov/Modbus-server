#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <iostream>

#include "Const.h"

int main(int argc, char*argv[])
{
    int s = -1;
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    int rc;
    int i;
    uint8_t *query;
    int header_length;

    ctx = modbus_new_tcp("127.0.0.1", 1502);
    query = static_cast<uint8_t *>(malloc(MODBUS_TCP_MAX_ADU_LENGTH));
    header_length = modbus_get_header_length(ctx);

    //modbus_set_debug(ctx, TRUE);


    mb_mapping = modbus_mapping_new(0,0,0x1A,0);

    for (int i=0; i < 26; i++) {
        mb_mapping->tab_registers[i] = UT_REGISTERS_TAB[i];
    }

    s = modbus_tcp_listen(ctx, 1);
    modbus_tcp_accept(ctx, &s);

    for (;;) {

        do {
            rc = modbus_receive(ctx, query);
        } while (rc == 0);

        if (rc == -1 && errno != EMBBADCRC) {
            break;
        }

        // Переадресация в query

        rc = modbus_reply(ctx, query, rc, mb_mapping);
        if (rc == -1) {
            break;
        }
    }

    printf("Quit the loop: %s\n", modbus_strerror(errno));

    if (s != -1) {
        close(s);
    }
    modbus_mapping_free(mb_mapping);
    free(query);

    return 0;
}