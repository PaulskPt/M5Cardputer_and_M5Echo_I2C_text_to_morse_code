#include <Arduino.h>

bool my_debug = false;

uint8_t i2c_bus_num = 0;

/*
char KEY_SPEED_INCR = 'i';        //increase morse speed
char KEY_SPEED_DECR = 'd';        // decrease morse speed
char SPEAKER_ON_OFF = 's';        // flip the use_speaker flag
char MORSE_GO = 'g';              // command to start the sending of morse code
char MORSE_END = 'e';             // command to stop the sending of morse code
char KEY_RESET = 'r';             // command to reset the slave device
char VOLUME = 'v';                // command to increase the volume of the slave device
*/

enum Command {
  SHOW_COMMANDS = 'c',
  SPEAKER_ON_OFF = 's',
  MORSE_GO = 'g',
  MORSE_END = 'e',
  KEY_RESET = 'r',
  KEY_SPEED_DECR = 'd',
  KEY_SPEED_INCR = 'i',
  VOLUME_CHG = 'v'
};

char cmd_ltrs[] = {'c', 'd', 'e', 'g', 'i', 'r', 's', 'v'};

uint8_t speed_default = 4;

uint8_t SPEED_IDX_MINIMUM = 0;
uint8_t SPEED_IDX_MAXIMUM = 8;

const uint8_t cmd_idx_arr_elem_size = 15;
static constexpr const char cmd_idx_arr[][cmd_idx_arr_elem_size] = {"CMD_DO_NOTHING", "CMD_RESET", 
    "CMD_MORSE_GO", "CMD_MORSE_END", "CMD_SPEED_CHG", "TEXT_MESSAGE", "CMD_VOLUME_CHG"};

#define CMD_DO_NOTHING      200  // = 0xC8 = Dummy command
#define CMD_RESET           201  // = 0xC9 = Reset (ESP.restart())
#define CMD_MORSE_GO        202  // = 0xCA = Start sending morse
#define CMD_MORSE_END       203  // = 0xCB = Stop sending morse
#define CMD_SPEED_CHG       204  // = 0xCC = Morse speed change
#define TEXT_MESSAGE        205  // = 0xCD = Text message
#define CMD_VOLUME_CHG      206  // = 0xCE = Set volume of M5Echo

#define NO_DATA             250  // = 0xFA = indicates the command has no data (e.g.: CMD_MORSE_GO), 
                                 // however a CMD_SPEED_CHG has data: an index value.
                                 // and a TEXT_MESSAGE has data.
#define CMD_IDX_TODO        255  // 0xFF (see sketch master, function send_cmd())

struct I2C_TEXT_MESSAGE {
  uint8_t  address;        // 1 byte
  uint16_t packetNr;       // 2 bytes
  uint8_t  msgType;        // 1 byte
  char     text[40-4];     // 36 bytes
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
//                                  ads pnr  msgType       data... NULL terminator
struct I2C_TEXT_MESSAGE text_msg = {0, 0, 0, TEXT_MESSAGE, 0,      0};

// Example of howto prepare a morse speed change message struct
// speed_msg.type = CMD_SPEED_CHG;
// speed_msg.speed_idx = 5;  // index to slave device's tone_time_lst (0-8). 3 = default
//                                ads pnr  msgType        cmd or 1 byte data  NULL terminator
struct I2C_CMD_MESSAGE cmd_msg = {0, 0, 0, CMD_SPEED_CHG, 5,                  0};


