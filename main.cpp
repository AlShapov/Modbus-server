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

    mb_mapping = modbus_mapping_new(0,0,0xFFFF,0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    for (i=0; i < sizeof(UT_ADDRESS_BUFF_TAB) / sizeof(uint16_t); i++) {
        for (int j = 0; j < NB_REGISTERS_BUFF[i]; j++) {
            mb_mapping->tab_registers[UT_ADDRESS_BUFF_TAB[i] + j] = UT_REGISTERS_TAB[i][j];
        }
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

        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x0D60) {
            switch(query[header_length+7]) {
                case 1:
                    if (query[header_length+9] == 64 && !((mb_mapping->tab_registers[0x0D40]>>7) % 2)) {
                        mb_mapping->tab_registers[0x0D40] += 1<<7;
                    } else if (query[header_length+9] == 32 && (mb_mapping->tab_registers[0x0D40]>>7) % 2) {
                        mb_mapping->tab_registers[0x0D40] -= 1<<7;
                    }
                    break;
                case 2:
                    if (query[header_length+9] == 64 && !((mb_mapping->tab_registers[0x0D40]>>5) % 2)) {
                        mb_mapping->tab_registers[0x0D40] += 1<<5;
                    } else if (query[header_length+9] == 32 && (mb_mapping->tab_registers[0x0D40]>>5) % 2) {
                        mb_mapping->tab_registers[0x0D40] -= 1<<5;
                    }
                    break;
                case 3:
                    if (query[header_length+9] == 64 && !((mb_mapping->tab_registers[0x0D40]>>3) % 2)) {
                        mb_mapping->tab_registers[0x0D40] += 1<<3;
                    } else if (query[header_length+9] == 32 && (mb_mapping->tab_registers[0x0D40]>>3) % 2) {
                        mb_mapping->tab_registers[0x0D40] -= 1<<3;
                    }
                    break;
                case 4:
                    if (query[header_length+9] == 64 && !((mb_mapping->tab_registers[0x0D41]>>2) % 2)) {
                        mb_mapping->tab_registers[0x0D41] += 1<<2;
                    } else if (query[header_length+9] == 32 && (mb_mapping->tab_registers[0x0D41]>>2) % 2) {
                        mb_mapping->tab_registers[0x0D41] -= 1<<2;
                    }
                    break;
                case 5:
                    if (query[header_length+9] == 64 && !((mb_mapping->tab_registers[0x0D40]>>10) % 2)) {
                        mb_mapping->tab_registers[0x0D40] += 1<<10;
                    } else if (query[header_length+9] == 32 && !((mb_mapping->tab_registers[0x0D41]>>11) % 2)) {
                        mb_mapping->tab_registers[0x0D41] -= 1<<11;
                    }
                    break;
                case 6:
                    break;
                case 7:
                    if (query[header_length+9] == 64)
                        for (i=0; i < 12; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = 0;
                    break;
                case 8:
                    break;
            }
        }



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