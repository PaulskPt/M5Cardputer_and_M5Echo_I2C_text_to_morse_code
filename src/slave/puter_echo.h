#include <Arduino.h>

bool my_debug = false;

uint8_t i2c_bus_num = 0;

char KEY_SPEED_INCR = 'i';  //increase morse speed
char KEY_SPEED_DECR = 'd'; // decrease morse speed
char KEY_FLIP_SPEAKER_FLAG = 's'; // flip the use_speaker flag
char MORSE_GO = 'g'; // command to start the sending of morse code
char MORSE_END = 'e'; // command to stop the sending of morse code
char KEY_RESET = 'r'; // command to reset the destination device

uint8_t SPEED_IDX_MINIMUM = 0;
uint8_t SPEED_IDX_MAXIMUM = 8;

#define CMD_MESSAGE         252 // = 0xFC     to not use 0 
#define TEXT_MESSAGE        253 // = 0xFD
#define SPEED_CHG           254 // = 0xFE

#define CMD_DO_NOTHING      200  // = 0xC8 = Dummy command
#define CMD_RESET           201  // = 0xC9 = Reset (ESP.restart())
#define CMD_MORSE_GO        202  // = 0xCA = Start sending morse
#define CMD_MORSE_END       203  // = 0xCB = Stop sending morse

struct I2C_TEXT_MESSAGE {
  uint8_t  address;        // 1 byte
  uint16_t packetNr;       // 2 bytes
  uint8_t  msgType;        // 1 byte
  char     text[40-4];     // 36 bytes
};

struct I2C_SPEED_CHG_MESSAGE {
  uint8_t  address;        // 1 byte
  uint16_t packetNr;       // 2 bytes
  uint8_t  msgType;        // 1 byte
  uint8_t  speed_idx;      // 1 byte
  uint8_t  dummy[40-5];    // not used, but max packet len 40 bytes like the I2C_TEXT_MESSAGE
};

struct I2C_CMD_MESSAGE {
  uint8_t  address;        // 1 byte
  uint16_t packetNr;       // 2 bytes
  uint8_t  msgType;        // 1 byte
  uint8_t  cmd_idx;        // 1 byte
  uint8_t  dummy[40-5];    // not used, but max packet len 40 bytes like the I2C_TEXT_MESSAGE
};

// Prepare a text message struct
// text_msg.type = MESSAGE;
// text_msg.text[0] = '\0';
//                                  ads pnr  msgType       bfr
struct I2C_TEXT_MESSAGE text_msg = {0, 0, 0, TEXT_MESSAGE, '\0'};

// Prepare a morse speed change message struct
// speed_msg.type = SPEED_CHG;
// speed_msg.speed_idx = 5;  // index to slave device's tone_time_lst (0-8). 3 = default
//                                        ads pnr  msgType    spd  bfr
struct I2C_SPEED_CHG_MESSAGE speed_msg = {0, 0, 0, SPEED_CHG, 5,  '\0'};

struct I2C_CMD_MESSAGE cmd_msg = {0, 0, 0, CMD_MESSAGE, 5,  '\0'};

