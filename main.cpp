#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <yaml.h>
#include <iostream>
#include <fstream>

#define SERVER_ID         17

// Перевод 16-ричного числа в строку
std::string hex_to_str(uint16_t H){
    std::string S = "0x";
    int z = 0;
    int Por;
    int Ost = H;
    int St = 16*16*16;
    for (int i=0; i<4; i++){
        Por = Ost / St;
        Ost = Ost % St;
        St = St / 16;
        if (Por)
            z = 1;
        switch (Por) {
            case (0):
                if (z)
                    S += "0";
                break;
            case (10):
                S += "A";
                break;
            case (11):
                S += "B";
                break;
            case (12):
                S += "C";
                break;
            case (13):
                S += "D";
                break;
            case (14):
                S += "E";
                break;
            case (15):
                S += "F";
                break;
            default:
                S += std::to_string(Por);
                break;
        }
    }
    if (S == "0x")
        S += "0";

    return S;
}

enum {
    TCP,
    RTU
};

int main(int argc, char*argv[])
{
    int s = -1;
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    int rc;
    int i;
    int use_backend;
    uint8_t *query;
    int header_length;
    std::string fname = "../config.yaml";
    YAML::Node config = YAML::LoadFile(fname);

    if (argc > 1) {
        if (strcmp(argv[1], "tcp") == 0) {
            use_backend = TCP;
        } else if (strcmp(argv[1], "rtu") == 0) {
            use_backend = RTU;
        } else {
            printf("Usage:\n  %s [tcp|rtu] - Modbus server for unit testing\n\n", argv[0]);
            return -1;
        }
    } else {
        use_backend = TCP;
    }

    if (use_backend == TCP) {
        ctx = modbus_new_tcp("127.0.0.1", 1502);
        query = static_cast<uint8_t *>(malloc(MODBUS_TCP_MAX_ADU_LENGTH));
    } else {
        ctx = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
        modbus_set_slave(ctx, SERVER_ID);
        query = static_cast<uint8_t *>(malloc(MODBUS_RTU_MAX_ADU_LENGTH));
    }
    header_length = modbus_get_header_length(ctx);

    //modbus_set_debug(ctx, TRUE);

    mb_mapping = modbus_mapping_new(0,0,0xFFFF,0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    // Инициализация регистров из файла
    for (i=0; i < config.size(); i++) { // Количество буферов
        for (int j = 0; j < config[i].begin()->second.size(); j++) { // Количество в них регистров
            mb_mapping->tab_registers[config[i].begin()->first.as<uint16_t>() + j] = config[i].begin()->second[j].as<uint16_t>() ;
        }
    }

    if (use_backend == TCP) {
        s = modbus_tcp_listen(ctx, 1);
        modbus_tcp_accept(ctx, &s);
    } else {
        rc = modbus_connect(ctx);
        if (rc == -1) {
            fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
            modbus_free(ctx);
            return -1;
        }
    }

    for (;;) {
        // Пропуск пустых запросов серверу
        do {
            rc = modbus_receive(ctx, query);
        } while (rc == 0);
        if (rc == -1 && errno != EMBBADCRC) {
            break;
        }

        // Использование буфера ТУ - команды управления
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x0D60) {
            switch(query[header_length+7]) {
                case 1: // БВ вкл/откл
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x80;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFF7F;
                    }
                    break;
                case 2: // ОР вкл/откл
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x20;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFDF;
                    }
                    break;
                case 3: // ВЭ вкл/откл
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x8;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFF7;
                    }
                    break;
                case 4: // АПВ БВ ввести/вывести
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D41] |= 0x4;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] &= 0xFFFB;
                    }
                    break;
                case 5: // Вкл уставку 2 / Вкл уставку 1 (взаимоисключение? добавить if в оба на проверку второго)
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x400;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] |= 0x800;
                    }
                    break;
                case 6: // Квитировать
                    if (query[header_length+9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] &= 0x2FFF;
                        mb_mapping->tab_registers[0x0D41] &= 0xFFFC;
                        mb_mapping->tab_registers[0x0D42] = 0;
                        mb_mapping->tab_registers[0x0D43] = 0;
                        mb_mapping->tab_registers[0x0D44] = 0;
                    }
                    break;
                case 7: // Сброс накопительной инф-ии
                    if (query[header_length+9] == 0x40)
                        for (i=0; i < 12; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = 0;
                    break;
                case 8: // Резерв
                    break;
            }
        }

        rc = modbus_reply(ctx, query, rc, mb_mapping);
        if (rc == -1) {
            break;
        }
    }

    // Сохранение регистров обратно в файл
    for (i=0; i < config.size(); i++) {
        for (int j = 0; j < config[i].begin()->second.size(); j++) {
            config[i].begin()->second[j] = hex_to_str(mb_mapping->tab_registers[config[i].begin()->first.as<uint16_t>() + j]);
        }
    }
    std::ofstream fout(fname);
    fout << config;

    printf("Quit the loop: %s\n", modbus_strerror(errno));

    if (use_backend == TCP) {
        if (s != -1) {
            close(s);
        }
    }
    modbus_mapping_free(mb_mapping);
    free(query);
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}