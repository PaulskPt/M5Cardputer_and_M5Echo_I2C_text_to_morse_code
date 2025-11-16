#include <Arduino.h>
#include <unordered_map>
#include <vector>

bool my_debug = false;

uint8_t i2c_bus_num = 0;

char KEY_SPEED_INCR = 'i';        //increase morse speed
char KEY_SPEED_DECR = 'd';        // decrease morse speed
char KEY_FLIP_SPEAKER_FLAG = 's'; // flip the use_speaker flag
char MORSE_GO = 'g';              // command to start the sending of morse code
char MORSE_END = 'e';             // command to stop the sending of morse code
char KEY_RESET = 'r';             // command to reset the slave device
char VOLUME = 'v';                // command to increase the volume of the slave device

#define VOLUME_DEFAULT 0
#define VOLUME_MAX 10

int speaker_volume = 0; // set in setup() by call to echoSPKR::getVolume()

uint8_t speed_default = 4;
uint8_t SPEED_IDX_MINIMUM = 0;
uint8_t SPEED_IDX_MAXIMUM = 8;

const uint8_t cmd_idx_arr_elem_size = 15;
static constexpr const char cmd_idx_arr[][cmd_idx_arr_elem_size] = {"CMD_DO_NOTHING", "CMD_RESET", "CMD_MORSE_GO", 
                                                  "CMD_MORSE_END",  "CMD_SPEED_CHG", "TEXT_MESSAGE", "CMD_VOLUME_CHG"};

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

// Define two beep tones
beep tone_dot = 
{
  .freq    = 1200,
  .time_ms = 100,
  .maxval  = 10000,
  .modal   = true
};

beep tone_dash = 
{
  .freq    = 1200,
  .time_ms = 300,
  .maxval  = 10000,
  .modal   = true
};

// First tone for M5 Echo button press
beep btn_tone1 = 
{
  .freq    = 1200,
  .time_ms = 100,
  .maxval  = 10000,
  .modal   = false
};

// Second tone for M5 Echo button press
beep btn_tone2 = 
{
  .freq    = 1400,
  .time_ms = 100,
  .maxval  = 10000,
  .modal   = false
};

// My experiments revealed that the higher the element .maxval is the higher the volume of the audio 
// that is produced by the speaker of the M5Atom Echo.
// 10_000 dec = 0x2710
uint8_t maxval_idx = 5;
#define MAXVAL_MINIMUM  0
#define MAXVAL_DEFAULT  5
#define MAXVAL_MAXIMUM 10 // Which will result in 10 * 1000 = 10_000 (see function setVolume)

// This map is used to lookup the (practical) speeds of words per minute in morse code,
// that was estimated by measurements during tests. The key is an index, in the sketch
// used by the variable name new_speed_idx
std::unordered_map<int, int> idx_vs_morse_speed = {
  {0, {10}},
  {1, {11}},
  {2, {13}},
  {3, {15}},
  {4, {17}},
  {5, {18}},
  {6, {26}},
  {7, {35}},
  {8, {54}}
};

// Define the morse code, dot-dash sequences for 
// the characters that are in the text string to send in morse code
// Example: in the array {1,2}, the 1 represents a dot, the 2 a dash.
std::unordered_map<char, std::vector<int>> morse_txt_dict = {
  //                     decimal:
  {'\"', {1,2,1,1,2,1}}, // 34 <">
  {',', {2,2,1,1,2,2}},  // 44
  {'.', {1,2,1,2,1,2}},  // 46
  {'/', {2,1,1,2,1}},    // 47
  {'0', {2,2,2,2,2}},    // 48       = 30 HEX
  {'1', {1,2,2,2,2}},    // 49
  {'2', {1,1,2,2,2}},    // 50
  {'3', {1,1,1,2,2}},    // 51
  {'4', {1,1,1,1,2}},    // 52
  {'5', {1,1,1,1,1}},    // 53
  {'6', {2,1,1,1,1}},    // 54
  {'7', {2,2,1,1,1}},    // 55
  {'8', {2,2,2,1,1}},    // 56
  {'9', {2,2,2,2,1}},    // 57
  {':', {2,2,2,1,1,1}},  // 58
  {'=', {2,1,1,1,1,2}},  // 61  used as break of line character written like '//'
  {'?', {1,1,2,2,1,1}},  // 63    for character not found
  {'a', {1,2}},          // 97
  {'b', {2,1,1,}},       // 98
  {'c', {2,1,2,1}},      // 99
  {'d', {2,1,1}},        // 100
  {'e', {1}},            // 101
  {'f', {1,1,2,1}},      // 102
  {'g', {2,2,1}},        // 103
  {'h', {1,1,1,1}},      // 104
  {'i', {1,1}},          // 105
  {'j', {1,2,2,2}},      // 106
  {'k', {2,1,2}},        // 107
  {'l', {1,2,1,1}},      // 108
  {'m', {2,2}},          // 109
  {'n', {2,1}},          // 110
  {'o', {2,2,2}},        // 111
  {'p', {1,2,2,1}},      // 112
  {'q', {2,2,1,2}},      // 113
  {'r', {1,2,1}},        // 114
  {'s', {1,1,1}},        // 115
  {'t', {2}},            // 116
  {'u', {1,1,2}},        // 117
  {'v', {1,1,1,2}},      // 118
  {'w', {1,2,2}},        // 119
  {'x', {2,1,1,2}},      // 120
  {'y', {2,1,2,2}},      // 121
  {'z', {2,2,1,1}},      // 122
};

