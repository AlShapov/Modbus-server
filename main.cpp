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

// Мьютексы для потоков
pthread_mutex_t timeMut;
pthread_mutex_t buf_st_1;
pthread_mutex_t buf_st_2;
pthread_mutex_t buf_v_1;
pthread_mutex_t buf_v_2;
pthread_mutex_t buf_def_1;
pthread_mutex_t nakop_inf;
pthread_mutex_t buf_sr;
pthread_mutex_t buf_sr_st;
pthread_mutex_t buf_sr_kv;
pthread_mutex_t threadQue;

// Сруктура для передачи данных потокам сервера
typedef struct{
    int use_backend;
    modbus_mapping_t *mapping;
    modbus_t *ctx;
    int nm_thr;
}threadData;

// Поток, считающий время
void * times(void *&mapping)
{
    modbus_mapping_t *mb_mapping = (modbus_mapping_t*) mapping;
    while(mb_mapping->tab_registers[0x0D39])
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
        if (mb_mapping->tab_registers[0x0C82] / 0x100 > 30)
        {
            mb_mapping->tab_registers[0x0C82] = 0;
            mb_mapping->tab_registers[0x0C83]++;
        }
        if (mb_mapping->tab_registers[0x0C83] % 0x100 > 12)
        {
            mb_mapping->tab_registers[0x0C83]-=12;
            mb_mapping->tab_registers[0x0C83]+=0x100;
        }
        pthread_mutex_unlock(&timeMut);

    }
    return nullptr;
}
// Поток, сохраняющий контроль приказа
void *ctrl_order(void *&mapping){
    modbus_mapping_t *mb_mapping = (modbus_mapping_t*) mapping;
    while(mb_mapping->tab_registers[0x0D39]){
        if (mb_mapping->tab_registers[0x0D41] >> 15){
            sleep(10);
            pthread_mutex_lock(&buf_st_2);
            mb_mapping->tab_registers[0x0D41] &= 0x7FFF;
            pthread_mutex_unlock(&buf_st_2);
        }
    }
    return nullptr;
}
// Поток симулирования сквитирований
void *sim_sensor(void *&mapping){
    modbus_mapping_t *mb_mapping = (modbus_mapping_t*) mapping;
    int num;
    while (mb_mapping->tab_registers[0x0D39]){
        std::cout << "Какой буфер? \n1.Буфер статуса\n2.Буфер ВЫЗОВ\n3.Буфер защит ЦЗА\n0.Выключение сервера\n";
        std::cin >> num;
        switch(num){
            case 1:
                std::cout << "1.Цепи сигнал. БВ не исправны/исправны.\n";
                std::cout << "2.Цепи сигнал. ОР не исправны/исправны.\n";
                std::cout << "3.Цепи сигнал. ВЭ не исправны/исправны.\n";
                std::cout << "4.ЗР включен/отключен.\n";
                std::cout << "5.Цепи сигнал. ЗР не исправны/исправны.\n";
                std::cout << "6.Аварийная сигнал. сработала.\n";
                std::cout << "7.Предупредит. сигнал. (ВЫЗОВ) сработала.\n";
                std::cout << "8.Местное управлеие включено/отключено.\n";
                std::cout << "9.ОТКАЗ призошел.\n";
                std::cout << "10.ОКЦ не в норме/в норме.\n";
                std::cout << "11.Успешное АПВ БВ произошло.\n";
                std::cout << "12.Неуспешное АПВ БВ произошло.\n";
                std::cout << "13.Перегрев контактного провода есть/нет.\n";
                std::cin >> num;
                pthread_mutex_lock(&buf_st_1);
                pthread_mutex_lock(&buf_st_2);
                switch(num){
                    case 1:
                        if ((mb_mapping->tab_registers[0x0D40] >> 1) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFFD;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x2;
                        }
                        break;
                    case 2:
                        if ((mb_mapping->tab_registers[0x0D40] >> 3) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFF7;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x8;
                        }
                        break;
                    case 3:
                        if ((mb_mapping->tab_registers[0x0D40] >> 5) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFDF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x20;
                        }
                        break;
                    case 4:
                        if ((mb_mapping->tab_registers[0x0D40] >> 6) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFBF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x40;
                        }
                        break;
                    case 5:
                        if ((mb_mapping->tab_registers[0x0D40] >> 7) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFF7F;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x80;
                        }
                        break;
                    case 6:
                        mb_mapping->tab_registers[0x0D40] |= 0x100;
                        break;
                    case 7:
                        mb_mapping->tab_registers[0x0D40] |= 0x200;
                        break;
                    case 8:
                        if ((mb_mapping->tab_registers[0x0D40] >> 10) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFBFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x400;
                        }
                        break;
                    case 9:
                        mb_mapping->tab_registers[0x0D40] |= 0x800;
                        break;
                    case 10:
                        if ((mb_mapping->tab_registers[0x0D40] >> 12) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xEFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x1000;
                        }
                        break;
                    case 11:
                        mb_mapping->tab_registers[0x0D41] |= 0x40;
                        break;
                    case 12:
                        mb_mapping->tab_registers[0x0D41] |= 0x80;
                        break;
                    case 13:
                        if ((mb_mapping->tab_registers[0x0D41] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D41] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D41] |= 0x100;
                        }
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                pthread_mutex_unlock(&buf_st_1);
                pthread_mutex_unlock(&buf_st_2);
                break;
            case 2:
                std::cout << "1.Неисправность БВ есть.\n";
                std::cout << "2.Неисправность ОР есть.\n";
                std::cout << "3.Неисправность цепей сигнал. ЗР есть.\n";
                std::cout << "4.Неисправность цепей сигнал. БВ есть.\n";
                std::cout << "5.Неисправность цепей сигнал. ОР есть.\n";
                std::cout << "6.Неисправность цепей сигнал. ВЭ есть.\n";
                std::cout << "7.Неисправность ИнТер есть.\n";
                std::cout << "8.Ограничение ресурса БВ есть.\n";
                std::cout << "9.Питание ШУ отсутсвует.\n";
                std::cout << "10.Питание ШВ отсутсвует.\n";
                std::cout << "11.Питание ОР отсутствует.\n";
                std::cout << "12.Питание цепей управления ОР, ВЭ отсутствует.\n";
                std::cout << "13.Питание привода ВЭ отсутствует.\n";
                std::cout << "14.Срабатывание ИКЗ произошло.\n";
                std::cout << "15.Неисправность ВЭ есть.\n";
                std::cout << "16.Перегрев контактного провода есть.\n";
                std::cin >> num;
                pthread_mutex_lock(&buf_v_1);
                pthread_mutex_lock(&buf_v_2);
                switch(num){
                    case 1:
                        mb_mapping->tab_registers[0x0D42] |= 0x1;
                        break;
                    case 2:
                        mb_mapping->tab_registers[0x0D42] |= 0x2;
                        break;
                    case 3:
                        mb_mapping->tab_registers[0x0D42] |= 0x4;
                        break;
                    case 4:
                        mb_mapping->tab_registers[0x0D42] |= 0x8;
                        break;
                    case 5:
                        mb_mapping->tab_registers[0x0D42] |= 0x10;
                        break;
                    case 6:
                        mb_mapping->tab_registers[0x0D42] |= 0x20;
                        break;
                    case 7:
                        mb_mapping->tab_registers[0x0D42] |= 0x40;
                        break;
                    case 8:
                        mb_mapping->tab_registers[0x0D42] |= 0x80;
                        break;
                    case 9:
                        mb_mapping->tab_registers[0x0D42] |= 0x100;
                        break;
                    case 10:
                        mb_mapping->tab_registers[0x0D42] |= 0x200;
                        break;
                    case 11:
                        mb_mapping->tab_registers[0x0D42] |= 0x400;
                        break;
                    case 12:
                        mb_mapping->tab_registers[0x0D42] |= 0x800;
                        break;
                    case 13:
                        mb_mapping->tab_registers[0x0D42] |= 0x1000;
                        break;
                    case 14:
                        mb_mapping->tab_registers[0x0D42] |= 0x4000;
                        break;
                    case 15:
                        mb_mapping->tab_registers[0x0D42] |= 0x8000;
                        break;
                    case 16:
                        mb_mapping->tab_registers[0x0D43] |= 0x100;
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                pthread_mutex_unlock(&buf_v_1);
                pthread_mutex_unlock(&buf_v_2);
                break;
            case 3:
                std::cout << "1.3 ступень МТЗ сработала.\n";
                std::cout << "2.Ресурс БВ исчерпан.\n";
                std::cout << "3.НЦС БВ сработала.\n";
                std::cout << "4.ДЗ сработала.\n";
                std::cout << "5.ЗМН сработала.\n";
                std::cout << "6.Внешняя защита БВ сработала.\n";
                std::cout << "7.Самопроизвольное отключение БВ произошло.\n";
                std::cout << "8.1 ступень МТЗ сработала.\n";
                std::cout << "9.2 ступень МТЗ сработала.\n";
                std::cout << "10.ЗПТ сработала.\n";
                std::cout << "11.ЗСНТ сработала.\n";
                std::cout << "12.ПТЗ сработала.\n";
                std::cout << "13.МТЗ отриц. сработала.\n";
                std::cout << "14.АСЗ сработала.\n";
                std::cout << "15.КвТЗ сработала.\n";
                std::cin >> num;
                pthread_mutex_lock(&buf_def_1);
                switch(num){
                    case 1:
                        mb_mapping->tab_registers[0x0D44] |= 0x1;
                        break;
                    case 2:
                        mb_mapping->tab_registers[0x0D44] |= 0x2;
                        break;
                    case 3:
                        mb_mapping->tab_registers[0x0D44] |= 0x4;
                        break;
                    case 4:
                        mb_mapping->tab_registers[0x0D44] |= 0x10;
                        break;
                    case 5:
                        mb_mapping->tab_registers[0x0D44] |= 0x20;
                        break;
                    case 6:
                        mb_mapping->tab_registers[0x0D44] |= 0x40;
                        break;
                    case 7:
                        mb_mapping->tab_registers[0x0D44] |= 0x80;
                        break;
                    case 8:
                        mb_mapping->tab_registers[0x0D44] |= 0x100;
                        break;
                    case 9:
                        mb_mapping->tab_registers[0x0D44] |= 0x200;
                        break;
                    case 10:
                        mb_mapping->tab_registers[0x0D44] |= 0x400;
                        break;
                    case 11:
                        mb_mapping->tab_registers[0x0D44] |= 0x800;
                        break;
                    case 12:
                        mb_mapping->tab_registers[0x0D44] |= 0x1000;
                        break;
                    case 13:
                        mb_mapping->tab_registers[0x0D44] |= 0x2000;
                        break;
                    case 14:
                        mb_mapping->tab_registers[0x0D44] |= 0x4000;
                        break;
                    case 15:
                        mb_mapping->tab_registers[0x0D44] |= 0x8000;
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                pthread_mutex_unlock(&buf_def_1);
                break;
            case 0:
                mb_mapping->tab_registers[0x0D39] = 0x0;
                break;
            default:
                std::cout << "Не существует.\n";
                break;
        }
    }
    return nullptr;
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

// Поток сервера
int serv(void* thrData) {
    threadData *data = (threadData*) thrData;
    modbus_mapping_t* mb_mapping = data->mapping;
    int use_backend = data->use_backend;
    modbus_t *ctx = data->ctx;
    int my_nm = data->nm_thr;
    int rc;
    int i;
    int j;
    uint8_t *query;
    int header_length;
    int nb_srez = 0;
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
                    pthread_mutex_lock(&buf_st_1);
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x1;
                        pthread_mutex_unlock(&buf_st_1);
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFFE;
                        pthread_mutex_unlock(&buf_st_1);
                        // Изменение буфера накоп. информации
                        pthread_mutex_lock(&nakop_inf);
                        for (i=0; i < 4; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = mb_mapping->tab_registers[0x0C80 + i];
                        mb_mapping->tab_registers[0x0CC4]++;
                        // аварийные отключения
                        // выработанный ресурс
                        // сумма макс токов отключения
                        mb_mapping->tab_registers[0x0CCA] = mb_mapping->tab_registers[0x0C84];
                        mb_mapping->tab_registers[0x0CCB] = mb_mapping->tab_registers[0x0C85];
                        pthread_mutex_unlock(&nakop_inf);
                    }
                    break;
                case 2: // ОР вкл/откл
                    pthread_mutex_lock(&buf_st_1);
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x4;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFFB;
                    }
                    pthread_mutex_unlock(&buf_st_1);
                    break;
                case 3: // ВЭ вкл/откл
                    pthread_mutex_lock(&buf_st_1);
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x10;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFEF;
                    }
                    pthread_mutex_unlock(&buf_st_1);
                    break;
                case 4: // АПВ БВ ввести/вывести
                    pthread_mutex_lock(&buf_st_2);
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D41] |= 0x20;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] &= 0xFFDF;
                    }
                    pthread_mutex_unlock(&buf_st_2);
                    break;
                case 5: // Вкл уставку 2 / Вкл уставку 1 (взаимоисключение? добавить if в оба на проверку второго)
                    if (query[header_length + 9] == 0x40) {
                        pthread_mutex_lock(&buf_st_1);
                        mb_mapping->tab_registers[0x0D40] |= 0x2000;
                        pthread_mutex_unlock(&buf_st_1);
                    } else if (query[header_length + 9] == 0x20) {
                        pthread_mutex_lock(&buf_st_2);
                        mb_mapping->tab_registers[0x0D41] |= 0x1000;
                        pthread_mutex_unlock(&buf_st_2);
                    }
                    break;
                case 6: // Квитировать
                    if (query[header_length + 9] == 0x40) {
                        pthread_mutex_lock(&buf_st_1);
                        pthread_mutex_lock(&buf_st_2);
                        pthread_mutex_lock(&buf_v_1);
                        pthread_mutex_lock(&buf_v_2);
                        pthread_mutex_lock(&buf_def_1);
                        mb_mapping->tab_registers[0x0D40] &= 0xF4FF;
                        mb_mapping->tab_registers[0x0D41] &= 0xFF1F;
                        mb_mapping->tab_registers[0x0D42] = 0;
                        mb_mapping->tab_registers[0x0D43] = 0;
                        mb_mapping->tab_registers[0x0D44] = 0;
                        pthread_mutex_unlock(&buf_st_1);
                        pthread_mutex_unlock(&buf_st_2);
                        pthread_mutex_unlock(&buf_v_1);
                        pthread_mutex_unlock(&buf_v_2);
                        pthread_mutex_unlock(&buf_def_1);
                    }
                    break;
                case 7: // Сброс накопительной инф-ии
                    if (query[header_length + 9] == 0x40) {
                        pthread_mutex_lock(&nakop_inf);
                        for (i = 0; i < 12; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = 0;
                        pthread_mutex_unlock(&nakop_inf);
                    }
                    break;
                case 8: // Резерв
                    break;
            }

            if (nb_srez<16){
                // Изменение буфера ТС из-за статусов - создание среза
                if (query[header_length+7] != 7  && query[header_length+7] != 8){
                    pthread_mutex_lock(&buf_sr);
                    for(i=0; i < 6; i++)
                        mb_mapping->tab_registers[0x0AFE + 10 * (nb_srez+1) + i] = mb_mapping->tab_registers[0x0D40 + i];
                    for(i=0; i < 4; i++)
                        mb_mapping->tab_registers[0x0B04 + 10 * (nb_srez+1) + i] = mb_mapping->tab_registers[0x0C80 + i];
                    pthread_mutex_unlock(&buf_sr);
                }
                nb_srez++;
            } else {
                pthread_mutex_lock(&buf_sr_st);
                mb_mapping->tab_registers[0x0AFD] |= 0x0002;
                pthread_mutex_unlock(&buf_sr_st);
            }

            // Бит контроля приказа взведен
            pthread_mutex_lock(&buf_st_2);
            mb_mapping->tab_registers[0x0D41] |= 0x8000;
            pthread_mutex_unlock(&buf_st_2);
        }
        // Запись в буфер квитирования
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x0AFC) {
            if (query[header_length+7] % 2){
                if (nb_srez) {
                    pthread_mutex_lock(&buf_sr_kv);
                    for (i = 0; i < nb_srez; i++)
                        for (j = 0; j < 10; j++)
                            mb_mapping->tab_registers[0x0AFE + 10 * i + j] = mb_mapping->tab_registers[0x0AFE + 10 * (i+1) + j];
                    pthread_mutex_unlock(&buf_sr_kv);
                    nb_srez--;
                }
            }
            if ((query[header_length+7] >> 1) % 2){
                pthread_mutex_lock(&buf_sr_st);
                mb_mapping->tab_registers[0x0AFD] &= 0xFFFD;
                pthread_mutex_unlock(&buf_sr_st);
            }
        }
        // Если есть срез, то есть непрочитанные данные
        pthread_mutex_lock(&buf_sr_st);
        if(nb_srez){
            mb_mapping->tab_registers[0x0AFD] |= 0x0001;
        } else {
            mb_mapping->tab_registers[0x0AFD] &= 0xFFFE;
        }
        pthread_mutex_unlock(&buf_sr_st);


        // Обновление буфера измерений из-за коррекции времени
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == 0x024C) {
            pthread_mutex_lock(&timeMut);
            for (i = 0; i < 4; i++)
                mb_mapping->tab_registers[0x0C80 + i] = mb_mapping->tab_registers[0x024C + i];
            pthread_mutex_unlock(&timeMut);
        }
    }

    pthread_mutex_lock(&threadQue);
    mb_mapping->tab_registers[0x0D38] |= 1<<my_nm;
    pthread_mutex_unlock(&threadQue);
    free(query);
    return 0;
}



int main(int argc, char*argv[])
{
    int s = -1;
    modbus_mapping_t *mb_mapping;
    int rc;
    int use_backend;
    int m_num_thr = 1;
    modbus_t **ctx = new modbus_t *[m_num_thr];
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
        for (int i = 0; i < m_num_thr; i++)
            ctx[i] = modbus_new_tcp("127.0.0.1", 1502);
    } else {
        for (int i = 0; i < m_num_thr; i++) {
            ctx[i] = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
            modbus_set_slave(ctx[i], SERVER_ID);
        }
    }
    // Память для мапа
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
    for(int i=0; i < 6; i++)
        mb_mapping->tab_registers[0x0AFE + i] = mb_mapping->tab_registers[0x0D40 + i];
    for(int i=0; i < 4; i++)
        mb_mapping->tab_registers[0x0B04 + i] = mb_mapping->tab_registers[0x0C80 + i];
    mb_mapping->tab_registers[0x0D39] = 0x1;
    mb_mapping->tab_registers[0x0D38] = (1 << m_num_thr) - 1;

    threadData* data = new threadData[m_num_thr];
    for (int i = 0; i < m_num_thr; i++) {
        data[i].mapping = mb_mapping;
        data[i].use_backend = use_backend;
    }

    // Запуск потоков
    pthread_t thread2;
    pthread_create(&thread2, NULL, reinterpret_cast<void *(*)(void *)>(times), &mb_mapping);
    pthread_detach(thread2);
    pthread_t thread4;
    pthread_create(&thread4, NULL, reinterpret_cast<void *(*)(void *)>(ctrl_order), &mb_mapping);
    pthread_detach(thread4);
    pthread_t thread5;
    pthread_create(&thread5, NULL, reinterpret_cast<void *(*)(void *)>(sim_sensor), &mb_mapping);
    pthread_detach(thread5);


    pthread_t* threads = new pthread_t[m_num_thr];
    int k;
    int f;
    s = modbus_tcp_listen(ctx[0], 1);
    while (mb_mapping->tab_registers[0x0D39]) {
        f = 1;
        k = 0;
        while (mb_mapping->tab_registers[0x0D38] && f && mb_mapping->tab_registers[0x0D39]){
            if ((mb_mapping->tab_registers[0x0D38] >> k) % 2){
                if (use_backend == TCP) {
                    modbus_tcp_accept(ctx[k], &s);
                    std::cout << "accept" << std::endl;
                } else {
                    rc = modbus_connect(ctx[k]);
                    if (rc == -1) {
                        fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
                        modbus_free(ctx[k]);
                        return -1;
                    }
                }

                data[k].ctx = ctx[k];
                data[k].nm_thr = k;

                pthread_create(&threads[k], NULL, reinterpret_cast<void *(*)(void *)>(serv), &data[k]);
                std::cout << "fin" << std::endl;
                pthread_mutex_lock(&threadQue);
                mb_mapping->tab_registers[0x0D38] &= (1 << m_num_thr) - 1 - (1<<k);
                pthread_mutex_unlock(&threadQue);
                f = 0;
            }
            k++;
        }
    }

    for (int i = 0; i < m_num_thr; i++)
        pthread_join(threads[i],NULL);

    // Сохранение регистров обратно в файл
    for (int i=0; i < config.size(); i++) {
        for (int j = 0; j < config[i].begin()->second.size(); j++) {
            config[i].begin()->second[j] = hex_to_str(mb_mapping->tab_registers[config[i].begin()->first.as<uint16_t>() + j]);
        }
    }
    std::ofstream fout;
    fout.open(fname);
    fout << config;
    fout.close();

    printf("Quit the loop: %s\n", modbus_strerror(errno));

    if (use_backend == TCP) {
        if (s != -1) {
            close(s);
        }
    }

    modbus_mapping_free(mb_mapping);
    for (int i = 0; i < m_num_thr; i++) {
        modbus_close(ctx[i]);
        modbus_free(ctx[i]);
    }
    delete[] threads;
    delete[] data;
    return 0;
}