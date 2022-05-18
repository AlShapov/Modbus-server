#define SERVER_ID         17

const uint16_t UT_BITS_ADDRESS = 0x130;
const uint16_t UT_BITS_NB = 0x25;
const uint8_t UT_BITS_TAB[] = { 0xC6, 0x6B, 0xB2, 0x0E, 0x1B };

const uint16_t UT_INPUT_BITS_ADDRESS = 0x1C4;
const uint16_t UT_INPUT_BITS_NB = 0x16;
const uint8_t UT_INPUT_BITS_TAB[] = { 0xAC, 0xDB, 0x35 };

const uint16_t UT_REGISTERS_NB = 0x8;
const uint16_t UT_ADDRESS_BUFF_TAB[] = { 0x0D40, 0x0C80,  0x0CC0, 0x0D60,  0x024C};
const int NB_REGISTERS_BUFF[] = { 6, 8, 12, 2, 4};
const uint16_t UT_REGISTERS_TAB[][12] = { {0x11, 0x12,0x22, 0x23,0x31, 0x32},
                                          {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48},
                                          {0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
                                           0x57, 0x58, 0x59, 0x510, 0x511, 0x512},
                                          {0x1, 0x2},
                                          {0x61, 0x62,0x63, 0x64}};


const uint16_t UT_INPUT_REGISTERS_ADDRESS = 0x108;
const uint16_t UT_INPUT_REGISTERS_NB = 0x1;
const uint16_t UT_INPUT_REGISTERS_TAB[] = { 0x000A };