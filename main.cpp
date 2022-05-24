#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <modbus.h>
#include <yaml.h>
#include <iostream>
#include <fstream>
#include <pthread.h>

#define SERVER_ID         17

enum {
    TCP,
    RTU
};

pthread_mutex_t timeMut;

typedef struct{
    int use_backend;
    modbus_mapping_t *mapping;
    modbus_t *ctx;
}threadData;


void * times(void *&mapping)
{
    modbus_mapping_t *mb_mapping = (modbus_mapping_t*) mapping;
    for (;;)
    {
        usleep(100000);
        pthread_mutex_lock(&timeMut);
        mb_mapping->tab_registers[0x0C80]+=100;
        if (mb_mapping->tab_registers[0x0C80] >= 999)
        {
            mb_mapping->tab_registers[0x0C80] = 0;
            mb_mapping->tab_registers[0x0C81]++;
        }
        if (mb_mapping->tab_registers[0x0C81] % 0x100 >= 60)
        {
            mb_mapping->tab_registers[0x0C81]-=60;
            mb_mapping->tab_registers[0x0C81]+=0x100;
        }
        if (mb_mapping->tab_registers[0x0C81] / 0x100 >= 60)
        {
            mb_mapping->tab_registers[0x0C81] = 0;
            mb_mapping->tab_registers[0x0C82]++;
        }
        if (mb_mapping->tab_registers[0x0C82] % 0x100 >= 24)
        {
            mb_mapping->tab_registers[0x0C82]-=24;
            mb_mapping->tab_registers[0x0C82]+=0x100;
        }
        if (mb_mapping->tab_registers[0x0C82] / 0x100 >= 30)
        {
            mb_mapping->tab_registers[0x0C82] = 0;
            mb_mapping->tab_registers[0x0C83]++;
        }
        if (mb_mapping->tab_registers[0x0C83] % 0x100 >= 12)
        {
            mb_mapping->tab_registers[0x0C83]-=12;
            mb_mapping->tab_registers[0x0C83]+=0x100;
        }
        pthread_mutex_unlock(&timeMut);

    }
}

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

int serv(void* thrData) {
    threadData *data = (threadData*) thrData;
    modbus_mapping_t* mb_mapping = data->mapping;
    int use_backend = data->use_backend;
    modbus_t *ctx = data->ctx;
    int rc;
    int i;
    uint8_t *query;
    int header_length;
    header_length = modbus_get_header_length(ctx);

    if (use_backend == TCP) {
        query = static_cast<uint8_t *>(malloc(MODBUS_TCP_MAX_ADU_LENGTH));
    } else {
        query = static_cast<uint8_t *>(malloc(MODBUS_RTU_MAX_ADU_LENGTH));
    }

    for (;;) {
        // Пропуск пустых запросов серверу
        do {
            rc = modbus_receive(ctx, query);
        } while (rc == 0);
        if (rc == -1 && errno != EMBBADCRC) {
            break;
        }

        rc = modbus_reply(ctx, query, rc, mb_mapping);
        if (rc == -1) {
            break;
        }

        // Использование буфера ТУ - команды управления
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x0D60) {
            switch (query[header_length + 7]) {
                case 1: // БВ вкл/откл
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x80;
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFF7F;
                        // Изменение буфера накоп. информации
                        for (i=0; i < 4; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = mb_mapping->tab_registers[0x0C80 + i];
                        mb_mapping->tab_registers[0x0CC4]++;
                        // аварийные отключения
                        // выработанный ресурс
                        // сумма макс токов отключения
                        mb_mapping->tab_registers[0x0CCA] = mb_mapping->tab_registers[0x0C84];
                        mb_mapping->tab_registers[0x0CCB] = mb_mapping->tab_registers[0x0C85];
                    }
                    break;
                case 2: // ОР вкл/откл
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x20;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFDF;
                    }
                    break;
                case 3: // ВЭ вкл/откл
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x8;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFF7;
                    }
                    break;
                case 4: // АПВ БВ ввести/вывести
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D41] |= 0x4;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] &= 0xFFFB;
                    }
                    break;
                case 5: // Вкл уставку 2 / Вкл уставку 1 (взаимоисключение? добавить if в оба на проверку второго)
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x400;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] |= 0x800;
                    }
                    break;
                case 6: // Квитировать
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] &= 0x2FFF;
                        mb_mapping->tab_registers[0x0D41] &= 0xFFFC;
                        mb_mapping->tab_registers[0x0D42] = 0;
                        mb_mapping->tab_registers[0x0D43] = 0;
                        mb_mapping->tab_registers[0x0D44] = 0;
                    }
                    break;
                case 7: // Сброс накопительной инф-ии
                    if (query[header_length + 9] == 0x40)
                        for (i = 0; i < 12; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = 0;
                    break;
                case 8: // Резерв
                    break;
            }
            // Изменение буфера ТС из-за статусов
            if (query[header_length + 7] != 7 && query[header_length + 7] != 8) {
                for (i = 0; i < 6; i++)
                    mb_mapping->tab_registers[0x0AFE + i] = mb_mapping->tab_registers[0x0D40 + i];
                for (i = 0; i < 4; i++)
                    mb_mapping->tab_registers[0x0B04 + i] = mb_mapping->tab_registers[0x0C80 + i];
            }
        }

        // Обновление буфера измерений из-за коррекции времени
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x024C) {
            pthread_mutex_lock(&timeMut);
            for (i = 0; i < 4; i++)
                mb_mapping->tab_registers[0x0C80 + i] = mb_mapping->tab_registers[0x024C + i];
            pthread_mutex_unlock(&timeMut);
        }
    }

    free(query);
    return 0;
}



int main(int argc, char*argv[])
{
    int s = -1;
    modbus_mapping_t *mb_mapping;
    int rc;
    int use_backend;
    modbus_t **ctx = (modbus_t**) malloc(10*sizeof(modbus_t*));
    std::string fname = "../config.yaml";
    YAML::Node config = YAML::LoadFile(fname);
    if(!(pthread_mutex_init(&timeMut, NULL)))
        perror("pthread_mutex_init");

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
        for (int i = 0; i < 10; i++)
            ctx[i] = modbus_new_tcp("127.0.0.1", 1502);
    } else {
        for (int i = 0; i < 10; i++) {
            ctx[i] = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
            modbus_set_slave(ctx[i], SERVER_ID);
        }
    }


    mb_mapping = modbus_mapping_new(0,0,0xFFFF,0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        return -1;
    }

    // Инициализация регистров из файла
    for (int i=0; i < config.size(); i++) { // Количество буферов
        for (int j = 0; j < config[i].begin()->second.size(); j++) { // Количество в них регистров
            mb_mapping->tab_registers[config[i].begin()->first.as<uint16_t>() + j] = config[i].begin()->second[j].as<uint16_t>() ;
        }
    }

    threadData* data = (threadData*) malloc(10*sizeof(threadData));
    for (int i = 0; i < 10; i++) {
        data[i].mapping = mb_mapping;
        data[i].use_backend = use_backend;
    }



    pthread_t thread2;
    pthread_create(&thread2, NULL, reinterpret_cast<void *(*)(void *)>(times), &mb_mapping);
    pthread_detach(thread2);

    //modbus_set_debug(ctx, TRUE);

    pthread_t* threads = (pthread_t*) malloc(10 * sizeof(pthread_t));
    int numThr = 0;


    s = modbus_tcp_listen(ctx[numThr], 1);
    for (int i = 0; i < 10; i++) {
        if (use_backend == TCP) {
            modbus_tcp_accept(ctx[numThr], &s);
            std::cout << "accept" << std::endl;
        } else {
            rc = modbus_connect(ctx[numThr]);
            if (rc == -1) {
                fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
                modbus_free(ctx[numThr]);
                return -1;
            }
        }

        data[numThr].ctx = ctx[numThr];

        pthread_create(&threads[numThr], NULL, reinterpret_cast<void *(*)(void *)>(serv), &data[numThr]);
        numThr++;
        std::cout << "fin" << std::endl;
    }
    for (int i = 0; i < 10; i++)
        pthread_join(threads[i],NULL);

    // Сохранение регистров обратно в файл
    for (int i=0; i < config.size(); i++) {
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
    for (int i = 0; i < 10; i++) {
        modbus_close(ctx[i]);
        modbus_free(ctx[i]);
    }
    free(threads);
    free(data);
    modbus_free(*ctx);
    return 0;
}