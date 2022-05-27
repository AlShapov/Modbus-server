#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <modbus.h>
#include <yaml.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>


#define SERVER_ID         17
#define REG_END_SERV      0x0D39 // Регистр для определения количества срезов
#define REG_SREZ_Q        0x0D38 // Регистр-флаг, определяющий завершение сервера
#define BUF_TU            0x0D60 // Адрес буфера ТУ
#define BUF_KVITIR        0x0AFC // Адрес буфера квитирования для срезов
#define BUF_SREZ          0x0AFE // Адрес буфера статуса, следом буфер ТС
#define BUF_COR_TIME      0x024C // Адрес буфера коррекции времени


enum {
    TCP,
    RTU
};

// Мьютексы для потоков
std::mutex meas_time; // Мьютекс измерений и времени
std::mutex buf_st; // Мьютекс статусов
std::mutex buf_sr; // Мьютекс среза
std::mutex end_serv; // Мьютекс конца сервера


// Сруктура для передачи данных потокам сервера
struct threadData{
    int use_backend;
    modbus_mapping_t *mapping;
    modbus_t *ctx;

    threadData(int use_backend, modbus_mapping_t *mapping, modbus_t *ctx): use_backend(use_backend), mapping(mapping), ctx(ctx){}

};
struct mapp_wctx{
    modbus_mapping_t *mapping;
    modbus_t *ctx;
};

// Поток, считающий время
void times(modbus_mapping_t *mb_mapping)
{
    int kon_serv;
    do
    {
        end_serv.lock();
        kon_serv = mb_mapping->tab_registers[REG_END_SERV];
        end_serv.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        meas_time.lock();
        mb_mapping->tab_registers[0x0C80]+=100;
        if (mb_mapping->tab_registers[0x0C80] >= 999)
        {
            mb_mapping->tab_registers[0x0C80] = 0;
            mb_mapping->tab_registers[0x0C81] += 0x100;
        }
        if (mb_mapping->tab_registers[0x0C81] / 0x100 >= 60)
        {
            mb_mapping->tab_registers[0x0C81]-=0x3C00;
            mb_mapping->tab_registers[0x0C81]++;
        }
        if (mb_mapping->tab_registers[0x0C81] % 0x100 >= 60)
        {
            mb_mapping->tab_registers[0x0C81] = 0;
            mb_mapping->tab_registers[0x0C82] += 0x100;
        }
        if (mb_mapping->tab_registers[0x0C82] / 0x100 >= 24)
        {
            mb_mapping->tab_registers[0x0C82]-=0x1800;
            mb_mapping->tab_registers[0x0C82]++;
        }
        if (mb_mapping->tab_registers[0x0C82] % 0x100 > 30)
        {
            mb_mapping->tab_registers[0x0C82] = 0x0001;
            mb_mapping->tab_registers[0x0C83]+=0x100;
        }
        if (mb_mapping->tab_registers[0x0C83] / 0x100 > 12)
        {
            mb_mapping->tab_registers[0x0C83]-=0xC00;
            mb_mapping->tab_registers[0x0C83]++;
        }
        meas_time.unlock();

    } while(kon_serv);
}
// Поток, генерирующий текущие измерения
void curr_meas(modbus_mapping_t *mb_mapping){
    int f_end_serv;
    do {
        end_serv.lock();
        f_end_serv = mb_mapping->tab_registers[REG_END_SERV];
        end_serv.unlock();
        meas_time.lock();
        srand(mb_mapping->tab_registers[0x0C81] / 0x100);
        mb_mapping->tab_registers[0x0C84] = (rand() % 50) / 1.25;
        mb_mapping->tab_registers[0x0C85] = (rand() % 35) / 0.061;
        mb_mapping->tab_registers[0x0C86] = (rand() % 3000 + 140) / 0.0076;
        mb_mapping->tab_registers[0x0C87] = (rand() % 35) / 0.134;
        meas_time.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    } while(f_end_serv);

}
// Поток, сохраняющий контроль приказа
void ctrl_order(modbus_mapping_t *mb_mapping){
    int f_end_serv;
    int cr_prik;
    do {
        end_serv.lock();
        f_end_serv = mb_mapping->tab_registers[REG_END_SERV];
        end_serv.unlock();
        buf_st.lock();
        cr_prik = mb_mapping->tab_registers[0x0D41] >> 15;
        buf_st.unlock();
        if (cr_prik){
            std::this_thread::sleep_for(std::chrono::seconds(10));
            buf_st.lock();
            mb_mapping->tab_registers[0x0D41] &= 0x7FFF;
            buf_st.unlock();
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    } while(f_end_serv);
}
// Поток симулирования сквитирований
void sim_sensor(modbus_mapping_t *mb_mapping, modbus_t *ctx){
    int num;
    while (mb_mapping->tab_registers[REG_END_SERV]){
        std::cout << "Какой буфер? \n1.Буфер статуса\n2.Буфер ВЫЗОВ\n3.Буфер защит ЦЗА\n0.Выключение сервера\n";
        std::cin >> num;
        switch(num){
            case 1:
                std::cout << "1.БВ включен/отключен.\n";
                std::cout << "2.Цепи сигнал. БВ не исправны/исправны.\n";
                std::cout << "3.ОР включен/отключен.\n";
                std::cout << "4.Цепи сигнал. ОР не исправны/исправны.\n";
                std::cout << "5.ВЭ рабочее/контрольное.\n";
                std::cout << "6.Цепи сигнал. ВЭ не исправны/исправны.\n";
                std::cout << "7.ЗР включен/отключен.\n";
                std::cout << "8.Цепи сигнал. ЗР не исправны/исправны.\n";
                std::cout << "9.Аварийная сигнал. сработала/сквитирована.\n";
                std::cout << "10.Предупредит. сигнал. (ВЫЗОВ) сработала/сквитировано.\n";
                std::cout << "11.Местное управлеие включено/отключено.\n";
                std::cout << "12.ОТКАЗ призошел/сквитировано.\n";
                std::cout << "13.ОКЦ не в норме/в норме.\n";
                std::cout << "14.АПВ БВ введено/выведено.\n";
                std::cout << "15.Успешное АПВ БВ произошло/сквитировано.\n";
                std::cout << "16.Неуспешное АПВ БВ произошло/сквитировано.\n";
                std::cout << "17.Перегрев контактного провода есть/нет.\n";
                std::cin >> num;
                buf_st.lock();
                switch(num){
                    case 1:
                        if (mb_mapping->tab_registers[0x0D40] % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFFE;
                            // Изменение буфера накоп. информации
                            meas_time.lock();
                            for (int i=0; i < 4; i++)
                                mb_mapping->tab_registers[0x0CC0 + i] = mb_mapping->tab_registers[0x0C80 + i];
                            mb_mapping->tab_registers[0x0CC4]++;
                            srand(mb_mapping->tab_registers[0x0C81] / 0x100);
                            mb_mapping->tab_registers[0x0CC5]++;
                            mb_mapping->tab_registers[0x0CC6] = rand() / 0.0065536;
                            mb_mapping->tab_registers[0x0CC7] = (rand() % 10) / 80;
                            mb_mapping->tab_registers[0x0CCA] = mb_mapping->tab_registers[0x0C84];
                            mb_mapping->tab_registers[0x0CCB] = mb_mapping->tab_registers[0x0C85];
                            meas_time.unlock();
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0001;
                        }
                        break;
                    case 2:
                        if ((mb_mapping->tab_registers[0x0D40] >> 1) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFFD;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0002;
                        }
                        break;
                    case 3:
                        if ((mb_mapping->tab_registers[0x0D40] >> 2) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFFB;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0004;
                        }
                        break;
                    case 4:
                        if ((mb_mapping->tab_registers[0x0D40] >> 3) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFF7;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0008;
                        }
                        break;
                    case 5:
                        if ((mb_mapping->tab_registers[0x0D40] >> 4) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFEF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0010;
                        }
                        break;
                    case 6:
                        if ((mb_mapping->tab_registers[0x0D40] >> 5) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFDF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0020;
                        }
                        break;
                    case 7:
                        if ((mb_mapping->tab_registers[0x0D40] >> 6) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFFBF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0040;
                        }
                        break;
                    case 8:
                        if ((mb_mapping->tab_registers[0x0D40] >> 7) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFF7F;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0080;
                        }
                        break;
                    case 9:
                        if ((mb_mapping->tab_registers[0x0D40] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0100;
                        }
                        break;
                    case 10:
                        if ((mb_mapping->tab_registers[0x0D40] >> 9) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFDFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0200;
                        }
                        break;
                    case 11:
                        if ((mb_mapping->tab_registers[0x0D40] >> 10) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xFBFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0400;
                        }
                        break;
                    case 12:
                        if ((mb_mapping->tab_registers[0x0D40] >> 11) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xF7FF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x0800;
                        }
                        break;
                    case 13:
                        if ((mb_mapping->tab_registers[0x0D40] >> 12) % 2){
                            mb_mapping->tab_registers[0x0D40] &= 0xEFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D40] |= 0x1000;
                        }
                        break;
                    case 14:
                        if ((mb_mapping->tab_registers[0x0D41] >> 5) % 2){
                            mb_mapping->tab_registers[0x0D41] &= 0xFFDF;
                        } else{
                            mb_mapping->tab_registers[0x0D41] |= 0x0020;
                        }
                        break;
                    case 15:
                        if ((mb_mapping->tab_registers[0x0D41] >> 6) % 2){
                            mb_mapping->tab_registers[0x0D41] &= 0xFFBF;
                        } else{
                            mb_mapping->tab_registers[0x0D41] |= 0x0040;
                        }
                        break;
                    case 16:
                        if ((mb_mapping->tab_registers[0x0D41] >> 7) % 2){
                            mb_mapping->tab_registers[0x0D41] &= 0xFF7F;
                        } else{
                            mb_mapping->tab_registers[0x0D41] |= 0x0080;
                        }
                        break;
                    case 17:
                        if ((mb_mapping->tab_registers[0x0D41] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D41] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D41] |= 0x0100;
                        }
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                buf_st.unlock();
                break;
            case 2:
                std::cout << "1.Неисправность БВ есть/сквитировано.\n";
                std::cout << "2.Неисправность ОР есть/сквитировано.\n";
                std::cout << "3.Неисправность цепей сигнал. ЗР есть/сквитировано.\n";
                std::cout << "4.Неисправность цепей сигнал. БВ есть/сквитировано.\n";
                std::cout << "5.Неисправность цепей сигнал. ОР есть/сквитировано.\n";
                std::cout << "6.Неисправность цепей сигнал. ВЭ есть/сквитировано.\n";
                std::cout << "7.Неисправность ИнТер есть/сквитировано.\n";
                std::cout << "8.Ограничение ресурса БВ есть/сквитировано.\n";
                std::cout << "9.Питание ШУ отсутсвует/сквитировано.\n";
                std::cout << "10.Питание ШВ отсутсвует/сквитировано.\n";
                std::cout << "11.Питание ОР отсутствует/сквитировано.\n";
                std::cout << "12.Питание цепей управления ОР, ВЭ отсутствует/сквитировано.\n";
                std::cout << "13.Питание привода ВЭ отсутствует/сквитировано.\n";
                std::cout << "14.Срабатывание ИКЗ произошло/сквитировано.\n";
                std::cout << "15.Неисправность ВЭ есть/сквитировано.\n";
                std::cout << "16.Перегрев контактного провода есть/сквитировано.\n";
                std::cin >> num;
                buf_st.lock();
                switch(num){
                    case 1:
                        if (mb_mapping->tab_registers[0x0D42] % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFFE;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0001;
                        }
                        break;
                    case 2:
                        if ((mb_mapping->tab_registers[0x0D42] >> 1) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFFD;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0002;
                        }
                        break;
                    case 3:
                        if ((mb_mapping->tab_registers[0x0D42] >> 2) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFFB;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0004;
                        }
                        break;
                    case 4:
                        if ((mb_mapping->tab_registers[0x0D42] >> 3) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFF7;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0008;
                        }
                        break;
                    case 5:
                        if ((mb_mapping->tab_registers[0x0D42] >> 4) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFEF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0010;
                        }
                        break;
                    case 6:
                        if ((mb_mapping->tab_registers[0x0D42] >> 5) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFDF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0020;
                        }
                        break;
                    case 7:
                        if ((mb_mapping->tab_registers[0x0D42] >> 6) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFFBF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0040;
                        }
                        break;
                    case 8:
                        if ((mb_mapping->tab_registers[0x0D42] >> 7) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFF7F;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0080;
                        }
                        break;
                    case 9:
                        if ((mb_mapping->tab_registers[0x0D42] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0100;
                        }
                        break;
                    case 10:
                        if ((mb_mapping->tab_registers[0x0D42] >> 9) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFDFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0200;
                        }
                        break;
                    case 11:
                        if ((mb_mapping->tab_registers[0x0D42] >> 10) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xFBFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0400;
                        }
                        break;
                    case 12:
                        if ((mb_mapping->tab_registers[0x0D42] >> 11) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xF7FF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x0800;
                        }
                        break;
                    case 13:
                        if ((mb_mapping->tab_registers[0x0D42] >> 12) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xEFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x1000;
                        }
                        break;
                    case 14:
                        if ((mb_mapping->tab_registers[0x0D42] >> 14) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0xBFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x4000;
                        }
                        break;
                    case 15:
                        if ((mb_mapping->tab_registers[0x0D42] >> 15) % 2){
                            mb_mapping->tab_registers[0x0D42] &= 0x7FFF;
                        } else{
                            mb_mapping->tab_registers[0x0D42] |= 0x8000;
                        }
                        break;
                    case 16:
                        if ((mb_mapping->tab_registers[0x0D43] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D43] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D43] |= 0x0100;
                        }
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                buf_st.unlock();
                break;
            case 3:
                std::cout << "1.3 ступень МТЗ сработала/сквитировано.\n";
                std::cout << "2.Ресурс БВ исчерпан/сквитировано.\n";
                std::cout << "3.НЦС БВ сработала/сквитировано.\n";
                std::cout << "4.ДЗ сработала/сквитировано.\n";
                std::cout << "5.ЗМН сработала/сквитировано.\n";
                std::cout << "6.Внешняя защита БВ сработала/сквитировано.\n";
                std::cout << "7.Самопроизвольное отключение БВ произошло/сквитировано.\n";
                std::cout << "8.1 ступень МТЗ сработала/сквитировано.\n";
                std::cout << "9.2 ступень МТЗ сработала/сквитировано.\n";
                std::cout << "10.ЗПТ сработала/сквитировано.\n";
                std::cout << "11.ЗСНТ сработала/сквитировано.\n";
                std::cout << "12.ПТЗ сработала/сквитировано.\n";
                std::cout << "13.МТЗ отриц. сработала/сквитировано.\n";
                std::cout << "14.АСЗ сработала/сквитировано.\n";
                std::cout << "15.КвТЗ сработала/сквитировано.\n";
                std::cin >> num;
                buf_st.lock();
                switch(num){
                    case 1:
                        if ((mb_mapping->tab_registers[0x0D44]) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFFE;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x1;
                        }
                        break;
                    case 2:
                        if ((mb_mapping->tab_registers[0x0D44] >> 1) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFFD;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x2;
                        }
                        break;
                    case 3:
                        if ((mb_mapping->tab_registers[0x0D44] >> 2) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFFB;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x4;
                        }
                        break;
                    case 4:
                        if ((mb_mapping->tab_registers[0x0D44] >> 4) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFEF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x10;
                        }
                        break;
                    case 5:
                        if ((mb_mapping->tab_registers[0x0D44] >> 5) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFDF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x20;
                        }
                        break;
                    case 6:
                        if ((mb_mapping->tab_registers[0x0D44] >> 6) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFFBF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x40;
                        }
                        break;
                    case 7:
                        if ((mb_mapping->tab_registers[0x0D44] >> 7) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFF7F;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x80;
                        }
                        break;
                    case 8:
                        if ((mb_mapping->tab_registers[0x0D44] >> 8) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFEFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x100;
                        }
                        break;
                    case 9:
                        if ((mb_mapping->tab_registers[0x0D44] >> 9) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFDFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x200;
                        }
                        break;
                    case 10:
                        if ((mb_mapping->tab_registers[0x0D44] >> 10) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xFBFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x400;
                        }
                        break;
                    case 11:
                        if ((mb_mapping->tab_registers[0x0D44] >> 11) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xF7FF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x800;
                        }
                        break;
                    case 12:
                        if ((mb_mapping->tab_registers[0x0D44] >> 12) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xEFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x1000;
                        }
                        break;
                    case 13:
                        if ((mb_mapping->tab_registers[0x0D44] >> 13) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xDFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x2000;
                        }
                        break;
                    case 14:
                        if ((mb_mapping->tab_registers[0x0D44] >> 14) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0xBFFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x4000;
                        }
                        break;
                    case 15:
                        if ((mb_mapping->tab_registers[0x0D44] >> 15) % 2){
                            mb_mapping->tab_registers[0x0D44] &= 0x7FFF;
                        } else{
                            mb_mapping->tab_registers[0x0D44] |= 0x8000;
                        }
                        break;
                    default:
                        std::cout << "Отсутствует.\n";
                        break;
                }
                buf_st.unlock();
                break;
            case 0:
                end_serv.lock();
                mb_mapping->tab_registers[REG_END_SERV] = 0x0;
                end_serv.unlock();
                modbus_connect(ctx);
                modbus_close(ctx);
                modbus_free(ctx);
                break;
            default:
                std::cout << "Не существует.\n";
                break;
        }

        buf_sr.lock();
        if (mb_mapping->tab_registers[REG_SREZ_Q]<16){
            // Изменение буфера ТС из-за статусов - создание среза
            if (num){
                buf_st.lock();
                for(int i=0; i < 6; i++)
                    mb_mapping->tab_registers[BUF_SREZ+1 + 10 * (mb_mapping->tab_registers[REG_SREZ_Q]) + i] = mb_mapping->tab_registers[0x0D40 + i];
                buf_st.unlock();
                meas_time.lock();
                for(int i=0; i < 4; i++)
                    mb_mapping->tab_registers[BUF_SREZ+7 + 10 * (mb_mapping->tab_registers[REG_SREZ_Q]) + i] = mb_mapping->tab_registers[0x0C80 + i];
                meas_time.unlock();

                mb_mapping->tab_registers[REG_SREZ_Q]++;
            }
        } else {
            mb_mapping->tab_registers[BUF_SREZ] |= 0x0200;
        }
        // Если есть срез, то есть непрочитанные данные
        if(mb_mapping->tab_registers[REG_SREZ_Q]){
            mb_mapping->tab_registers[BUF_SREZ] |= 0x0100;
        } else {
            mb_mapping->tab_registers[BUF_SREZ] &= 0xFEFF;
        }
        buf_sr.unlock();
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

// Поток сервера
int serv(void* thrData) {
    end_serv.lock();
    threadData *data = (threadData*) thrData;
    modbus_mapping_t* mb_mapping = data->mapping;
    int use_backend = data->use_backend;
    modbus_t *ctx = data->ctx;
    end_serv.unlock();
    int rc;
    int i;
    int j;
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
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == BUF_TU) {
            switch (query[header_length + 7]) {
                case 1: // БВ вкл/откл
                    buf_st.lock();
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x0001;
                        buf_st.unlock();
                    } else if (query[header_length+9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFFE;
                        buf_st.unlock();
                        // Изменение буфера накоп. информации
                        meas_time.lock();
                        for (i=0; i < 4; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = mb_mapping->tab_registers[0x0C80 + i];
                        mb_mapping->tab_registers[0x0CC4]++;
                        srand(mb_mapping->tab_registers[0x0C81] / 0x100);
                        mb_mapping->tab_registers[0x0CC6] = rand() / 0.0065536;
                        mb_mapping->tab_registers[0x0CC7] = (rand() % 10) / 80;
                        mb_mapping->tab_registers[0x0CCA] = mb_mapping->tab_registers[0x0C84];
                        mb_mapping->tab_registers[0x0CCB] = mb_mapping->tab_registers[0x0C85];
                        meas_time.unlock();
                    }
                    break;
                case 2: // ОР вкл/откл
                    buf_st.lock();
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x0004;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFFB;
                    }
                    buf_st.unlock();
                    break;
                case 3: // ВЭ вкл/откл
                    buf_st.lock();
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x0010;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D40] &= 0xFFEF;
                    }
                    buf_st.unlock();
                    break;
                case 4: // АПВ БВ ввести/вывести
                    buf_st.lock();
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D41] |= 0x0020;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] &= 0xFFDF;
                    }
                    buf_st.unlock();
                    break;
                case 5: // Вкл уставку 2 / Вкл уставку 1
                    buf_st.lock();
                    if (query[header_length + 9] == 0x40) {
                        mb_mapping->tab_registers[0x0D40] |= 0x2000;
                    } else if (query[header_length + 9] == 0x20) {
                        mb_mapping->tab_registers[0x0D41] |= 0x1000;
                    }
                    buf_st.unlock();
                    break;
                case 6: // Квитировать
                    if (query[header_length + 9] == 0x40) {
                        buf_st.lock();
                        mb_mapping->tab_registers[0x0D40] &= 0xF4FF;
                        mb_mapping->tab_registers[0x0D41] &= 0xFF3F;
                        mb_mapping->tab_registers[0x0D42] = 0;
                        mb_mapping->tab_registers[0x0D43] = 0;
                        mb_mapping->tab_registers[0x0D44] = 0;
                        buf_st.unlock();
                    }
                    break;
                case 7: // Сброс накопительной инф-ии
                    if (query[header_length + 9] == 0x40) {
                        meas_time.lock();
                        for (i = 0; i < 12; i++)
                            mb_mapping->tab_registers[0x0CC0 + i] = 0;
                        meas_time.unlock();
                    }
                    break;
                case 8: // Резерв
                    break;
            }

            buf_sr.lock();
            if (mb_mapping->tab_registers[REG_SREZ_Q]<16){
                // Изменение буфера ТС из-за статусов - создание среза
                if (query[header_length+7] != 7  && query[header_length+7] != 8){
                    buf_st.lock();
                    for(i=0; i < 6; i++)
                        mb_mapping->tab_registers[BUF_SREZ+1 + 10 * (mb_mapping->tab_registers[REG_SREZ_Q]) + i] = mb_mapping->tab_registers[0x0D40 + i];
                    buf_st.unlock();
                    meas_time.lock();
                    for(i=0; i < 4; i++)
                        mb_mapping->tab_registers[BUF_SREZ+7 + 10 * (mb_mapping->tab_registers[REG_SREZ_Q]) + i] = mb_mapping->tab_registers[0x0C80 + i];
                    meas_time.unlock();

                    mb_mapping->tab_registers[REG_SREZ_Q]++;
                }
            } else {
                mb_mapping->tab_registers[BUF_SREZ] |= 0x0200;
            }
            buf_sr.unlock();
            // Бит контроля приказа взведен
            buf_st.lock();
            mb_mapping->tab_registers[0x0D41] |= 0x8000;
            buf_st.unlock();
        }

        buf_sr.lock();
        // Запись в буфер квитирования
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == BUF_KVITIR) {
            if (query[header_length+6] % 2){
                if (mb_mapping->tab_registers[REG_SREZ_Q]) {
                    for (i = 0; i < mb_mapping->tab_registers[REG_SREZ_Q]; i++)
                        for (j = 0; j < 10; j++)
                            mb_mapping->tab_registers[BUF_SREZ+1 + 10 * i + j] = mb_mapping->tab_registers[BUF_SREZ+1 + 10 * (i+1) + j];
                    mb_mapping->tab_registers[REG_SREZ_Q]--;
                }
            }
            if ((query[header_length+6] >> 1) % 2){
                mb_mapping->tab_registers[BUF_SREZ] &= 0xFDFF;
            }
        }

        // Если есть срез, то есть непрочитанные данные
        if(mb_mapping->tab_registers[REG_SREZ_Q]){
            mb_mapping->tab_registers[BUF_SREZ] |= 0x0100;
        } else {
            mb_mapping->tab_registers[BUF_SREZ] &= 0xFEFF;
        }
        buf_sr.unlock();


        // Обновление буфера измерений из-за коррекции времени
        if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1) == BUF_COR_TIME) {
            meas_time.lock();
            for (i = 0; i < 4; i++)
                mb_mapping->tab_registers[0x0C80 + i] = mb_mapping->tab_registers[BUF_COR_TIME + i];
            meas_time.unlock();
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
    std::vector<modbus_t*> ctx;
    modbus_t *ctxs;
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

    // Память для мапа
    mb_mapping = modbus_mapping_new(0,0,0x0D61+1,0);
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

    mb_mapping->tab_registers[REG_END_SERV] = 0x1;

    std::vector<threadData> data;

    if (use_backend == TCP) {
        ctxs = modbus_new_tcp("127.0.0.1", 1502);
    } else {
        ctxs = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
        modbus_set_slave(ctxs, SERVER_ID);
    }

    mapp_wctx thr_sens{};
    thr_sens.mapping=mb_mapping;
    thr_sens.ctx=ctxs;

    // Запуск потоков
    std::thread thr1(times, mb_mapping);
    std::thread thr2(ctrl_order, mb_mapping);
    std::thread thr3(sim_sensor, mb_mapping, ctxs);
    std::thread thr4(curr_meas, mb_mapping);


    std::vector<std::thread> threads;
    end_serv.lock();
    int f_end_serv = mb_mapping->tab_registers[REG_END_SERV];
    end_serv.unlock();
    if (use_backend == TCP)
        s = modbus_tcp_listen(ctxs, 1);
    do {
        if (use_backend == TCP) {
            ctx.emplace_back(modbus_new_tcp("127.0.0.1", 1502));
            modbus_tcp_accept(ctx.back(), &s);
        } else {
            ctx.emplace_back(modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1));
            modbus_set_slave(ctx.back(), SERVER_ID);
            rc = modbus_connect(ctx.back());
            if (rc == -1) {
                fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
                modbus_free(ctx.back());
                return -1;
            }
        }

        if (f_end_serv){
            end_serv.lock();
            data.emplace_back(use_backend,mb_mapping,ctx.back());
            end_serv.unlock();
            threads.emplace_back(serv, &data.back());
        }

        end_serv.lock();
        f_end_serv = mb_mapping->tab_registers[REG_END_SERV];
        end_serv.unlock();
    } while (f_end_serv);


    for (auto &thread : threads) thread.join();

    thr1.join();
    thr2.join();
    thr3.join();
    thr4.join();

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
    for (int i = 0; i < ctx.size(); i++) {
        modbus_close(ctx[i]);
        modbus_free(ctx[i]);
    }
    return 0;
}