#include <Arduino.h>

bool my_debug = false;

uint8_t i2c_bus_num = 0;

char KEY_SPEED_INCR = 'i';  //increase morse speed
char KEY_SPEED_DECR = 'd'; // decrease morse speed
char KEY_FLIP_SPEAKER_FLAG = 's'; // flip the use_speaker flag

uint8_t SPEED_IDX_MINIMUM = 0;
uint8_t SPEED_IDX_MAXIMUM = 8;

#define TEXT_MESSAGE        253 // to not use 0 
#define SPEED_CHG           254

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
