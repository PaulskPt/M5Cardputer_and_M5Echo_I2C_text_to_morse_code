/*
*******************************************************************************
* Initial copyright (c) 2021 by M5Stack
* 初始版權 (c) 2021 歸 M5Stack 所有
*                  Equipped with Atom-Lite sample source code
*                          配套  Atom-Lite 示例源代码
* Visit for more information: https://docs.m5stack.com/en/atom/atom_spk
* 获取更多资料请访问：https://docs.m5stack.com/zh_CN/atom/atom_spk
*
* Product:  SPK.
* Date: 2021/9/1
*
* Ported to a M5Stack Atom Echo by @PaulskPt (Github).
* Date: 2024/10/7
* Using example C++ code originally designed for the M5Stack AtomSPK
* ported this to the M5Stack Atom Echo
* Changed AtomSPK.h and AtomSPK.cpp into AtomEchoSPKR.h and AtomEchoSPKR.cpp
*
* New description: Speaker playing morse code example using ATOMECHOSPKR class
*
* Update: 2025-03-29 M5Echo_I2C_text_rx_to_morse.ino
*    Goal: to receive a text string via I2C,
*    from another device, in my case a M5Stack Cardputer,
*
*    At the end counting the number of words sent and displaying the speed
*    of the morse code sent.
*
* IDE: Arduino v2.3.5. Inside the Arduino IDE > Tools > Board chosen: M5Atom (M5Stack).
***************************************************************************************
*
* +--------------------------------------------------------------------+
* | Note: You can interrupt the sending of morse code                  |
* | by pressing one of the two buttons on the M5Stack Dualbutton unit. |
* | The sketch then will print (in case the blue button was pressed):  |
* | send_morse(): button blue pressed. Exiting function                |
* +--------------------------------------------------------------------+
*
* NOTE: Do not use this sketch when oscilloscope probes are connected in parallel to the I2C wires 
*       and the oscilloscope is switched OFF. The probes will drain the signal resulting in the
*       I2C signal arriving in the Atom Echo will be unreliable! If you experience this effect
*       disconnect the probes or switch the oscilloscope ON!
*/

#include <Arduino.h>
#include <Wire.h>
// #include "C:/Users/PaulS2/AppData/Local/Arduino15/packages/m5stack/hardware/esp32/2.1.4/libraries/Wire/src/Wire.h"
#include <M5Atom.h>
#include <FastLED.h>
#include "AtomEchoSPKR.h"
#include <time.h>
#include "puter_echo.h"

#include <unordered_map>
#include <vector>
#include <iostream>
#include <string.h>
#include <cstring>  // For memcpy

extern TwoWire Wire;

#define I2C_DEV_ADDR    0x55

#define NUM_LEDS 1
#define ATOMECHO_LED_PIN 27

#define GROVE_SDA 26 //
#define GROVE_SCL 32 //

// In function send_morse() print unit space indicator "|1|" 
// between the dots and dashes.
// If you don't want the unit space indicator printed,
// comment-out the next line.
#define SHOW_UNITS  

CRGB leds[NUM_LEDS];

enum my_colors {RED=0, GREEN, BLUE, WHITE, BLACK};

unsigned long start_t = millis();

ATOMECHOSPKR echoSPKR;  // Create an instance of the ATOMECHOSPKR class

int debounce_delay = 100; // mSec
size_t rx_bufferSize = 128;
size_t cw_bufferSize = 128;
uint8_t *rx_buffer;
uint8_t *cw_buffer;
int8_t rx_buffer_howMany = 0;
int rx_buffer_idx = 0;
int cw_buffer_idx = 0;
bool I2C_active = false;
bool receiveFlag = false;
bool rcvd_cmd_flag = false;
bool rcvd_set_speed_flag = false;
bool rcvd_message_flag = false;
bool cmd_morse_go_flag = false;
bool cmd_morse_end_flag = false;
bool cmd_reset_flag = false;
bool cmd_volume_echo_flag = false;
uint32_t TXpacketNr = 0;
uint32_t RXpacketNr = 0;
const int tone_time_lst[] = {200, 180, 160, 140, 120, 100,  80,  60,  40};
int le_tone_time_lst = sizeof(tone_time_lst)/sizeof(tone_time_lst[0]);
uint8_t new_speed_idx = speed_default; // default morse code speed index (rcvd fm M5Cardputer)
uint16_t dly1 = tone_time_lst[speed_default]/2;  // unit delay
uint16_t dly3 = dly1 * 3; // character delay
uint16_t dly7 = dly1 * 7; // word delay

void go_restart() {
  Serial.println(F("Going to restart in 5 seconds..."));
  delay(5000);  // wait 5 seconds
  ESP.restart();
}

bool create_rx_buffer() {
  static constexpr const char txt0[] PROGMEM = "create_rx_buffer(): ";
  bool ret = false;
  if (rx_buffer == NULL) {
    rx_buffer = (uint8_t *)malloc(rx_bufferSize);
    if (rx_buffer == NULL) {
      Serial.print(txt0);
      Serial.print(F("Can't allocate memory for I2C_"));
      Serial.printf("%d rx_buffer\n", i2c_bus_num);
    }
    else {
      ret = true;
      rx_buffer[0] = '\0';
    }
  }
  return ret;
}

bool create_cw_buffer() {
  static constexpr const char txt0[] PROGMEM = "create_cw_buffer(): ";
  bool ret = false;
  if (cw_buffer == NULL) {
    cw_buffer = (uint8_t *)malloc(cw_bufferSize);
    if (cw_buffer == NULL) {
      Serial.print(txt0);
      Serial.println(F("Can't allocate memory for cw_buffer"));
    }
    else {
      ret = true;
      cw_buffer[0] = '\0';
    }
  }
  return ret;
}

void freeRxBuffer() {
  if (rx_buffer != NULL) {
    free(rx_buffer);
    rx_buffer = NULL;
    rx_buffer_idx = 0;
  }
}

void freeCWBuffer() {
  if (cw_buffer != NULL) {
    free(cw_buffer);
    cw_buffer = NULL;
    cw_buffer_idx = 0;
  }
}

void dot_dash_time() {
  static constexpr const char txt0[] PROGMEM = "dot_dash_time(): ";
  static constexpr const char txt1[] PROGMEM = "set to: ";
  size_t length = strlen(txt0); // Get the length of txt0
  char* spaces = new char[length + 1]; // +1 for null terminator
  memset(spaces, ' ', length); // No 'std::' prefix needed
  spaces[length] = '\0'; // Add null terminator
  Serial.print(txt0);
  Serial.print(F("tone_dot.time_ms  "));
  Serial.print(txt1);
  Serial.printf("%3d", tone_dot.time_ms);
  Serial.println(F(" mSeconds"));
  Serial.print(spaces);
  Serial.print(F("tone_dash.time_ms "));
  Serial.print(txt1);
  Serial.printf("%3d", tone_dash.time_ms);
  Serial.println(F(" mSeconds"));

  delete[] spaces; // Free memory when done
}

void show_delays() {
  Serial.println(F("Values for dly1, dly3 and dly7:"));
  Serial.printf("dly1: %d, dly3: %d, dly7: %d mSeconds\n", dly1, dly3, dly7);
}

void set_volume() {
  static constexpr const char txt0[] PROGMEM = "set_volume(): ";
  static constexpr const char txt1[] PROGMEM = "volume (0-10) ";
  volume_idx++;  // increase volume index
  Serial.print(txt0);
  Serial.print(txt1);
  if (volume_idx > VOLUME_MAX) {
    volume_idx = 0;
    Serial.print(F("maximum reached. Reset to minimum: "));
  } else {
    Serial.print(F("increased to: "));
  }
  Serial.println(volume_idx);
  echoSPKR.setVolume(volume_idx); // call the setVolume function of the speaker driver
}

void set_speed() {

  if (rcvd_set_speed_flag == false)
    return;
  
  rcvd_set_speed_flag = false; // reset flag

  static constexpr const char txt0[] PROGMEM = "set_speed(): ";
  static constexpr const char txt1[] PROGMEM = "speed index changed to: ";  // was: tone_time_list_idx changed to
  
  int tone_dly1 = tone_time_lst[speed_default];  // set default
  int unit_dly1 = tone_dly1 / 2;                 // temporary was: dly1_lst[speed_default];       // same

  if (new_speed_idx < 0 || new_speed_idx >= le_tone_time_lst)
    new_speed_idx = speed_default; // set to default
  
  //tone_dly1 = tone_dot.time_ms;
  //unit_dly1 = dly1;

  if (my_debug) {
    Serial.print(txt0);
    Serial.printf("new_speed_idx = %d, le_tone_time_lst-1 = %d, tone_time_lst[%d] = %d\n", 
      new_speed_idx, le_tone_time_lst-1, new_speed_idx, tone_time_lst[new_speed_idx]);
  }

  Serial.print(txt0);
  Serial.print(txt1);
  Serial.printf("%d\n", new_speed_idx);
  
  tone_dly1 = tone_time_lst[new_speed_idx];
  unit_dly1 = tone_dly1 / 2;  // was temporarily: dly1_lst[new_speed_idx];

  tone_dot.time_ms = tone_dly1;
  tone_dash.time_ms = 3 * tone_dot.time_ms; // keep ratio 3 : 1

  dly1 = unit_dly1;
  dly3 = dly1 * 3;
  dly7 = dly1 * 7;
  
  dot_dash_time();

  Serial.print(txt0);
  show_delays();
}

// Example see:
// C:\Users\PaulS2\AppData\Local\Arduino15\packages\m5stack\
//      hardware\esp32\2.1.4\libraries\Wire\examples\WireSlave\WireSlave.ino
void handle_rx(int8_t nrBytes) {
  static constexpr const char txt0[] PROGMEM = "handle_rx(): ";
  
  constexpr size_t cmd_idx_arr_le = sizeof(cmd_idx_arr) / sizeof(cmd_idx_arr[0]);
  uint8_t cmdIdx;
  uint8_t i;
  uint8_t dest_address;
  uint8_t rx_msg_len;
  uint32_t PacketNr_tmp;
  int value;
  bool result;
  bool stop = false;
  char c;
  
  //freeRxBuffer();
  rx_buffer = NULL;  // was: rx_buffer[0] = '\0';
  rx_buffer_idx = 0;

  result = create_rx_buffer();
  if (result == false)
    return;
  
  //int nrBytes = Wire.available();
  if (nrBytes == 0) 
    return;
  else {
    while(Wire.available()) {
      value = Wire.read();      // Actually receive the I2C data
      if (value >= 0) {
        rx_buffer[rx_buffer_idx] = static_cast<uint8_t>(value);
        rx_buffer_idx += 1;
      }
    }
    if (rx_buffer_idx == 0) 
      return;   // exit when nothing received
  }

  Serial.print(txt0);
  Serial.print(F("received nr of bytes: "));
  Serial.println(rx_buffer_idx);

  dest_address = rx_buffer[0];
  if (dest_address != I2C_DEV_ADDR)
    return;
  else {
    Serial.print(txt0);
    Serial.print(F("rcvd correct destination address: 0x"));
    Serial.println(dest_address, HEX);
    RXpacketNr = (rx_buffer[1] << 8) | rx_buffer[2]; // add MSB then LSB of 16 bits packetNr
    Serial.print(txt0);
    Serial.printf("RXpacketNr: %u\n", RXpacketNr);
    Serial.print(txt0);
    Serial.print(F("type of command: "));
    // Serial.printf("rx_buffer[3] = %d = ", rx_buffer[3]);
    uint8_t cmd_idx = rx_buffer[3] - CMD_DO_NOTHING;
    Serial.println(cmd_idx_arr[cmd_idx]);
    switch (rx_buffer[3]) {
      case CMD_DO_NOTHING:
        stop = true;
        break;
      case CMD_RESET:
        go_restart();
        break;
      case CMD_MORSE_GO:
        cmd_morse_go_flag = true;
        cmd_morse_end_flag = false;
        break;
      case CMD_MORSE_END:
        cmd_morse_go_flag = false;
        cmd_morse_end_flag = true;
        stop = true;
        break;
      case CMD_SPEED_CHG:
        rcvd_set_speed_flag = true;
        break;
      case TEXT_MESSAGE:
        rcvd_message_flag = true;
        break;
      case CMD_VOLUME_CHG:
        cmd_volume_echo_flag = true;
        break;
      default:
        Serial.println(F("UNKNOWN CMD. Exiting function.."));
        stop = true;
        break;
    }
    if (stop == true)
      return;
  
    if (rcvd_set_speed_flag) {        
      new_speed_idx = rx_buffer[4];
      Serial.print(txt0);
      Serial.print(F("New speed index: "));
      Serial.printf("%d", new_speed_idx);
      Serial.println(F(" received from master device"));

      if (new_speed_idx < SPEED_IDX_MINIMUM || new_speed_idx > SPEED_IDX_MAXIMUM) {
        return;
      } else {  
        set_speed();
      }
    } else if (rcvd_message_flag) {
      rx_msg_len = rx_buffer_idx - 4;
      Serial.print(txt0);
      Serial.print(F("message received: "));
      Serial.printf("\"%.*s\"", rx_msg_len, (char *)&rx_buffer[4]);
      Serial.print(F(", length: "));
      Serial.println(rx_msg_len);
      if (!cpyRXtoCW()) // Copy the rx_buffer to the cw_buffer
        return;
    }
  }
}

uint8_t calcCWbufferLen() {
  uint8_t idx = 0;
  uint8_t i;
  for (i = 0; cw_buffer[i] != '\0'; i++) {
    idx++;
  }
  if (cw_buffer[i] == '\0')
    idx++;
  return idx;
}

/* 
*  Copy the contents of the rx_buffer to the cw_buffer.
*  Reason to do this: when executing the function send_morse(),
*  and through a call to poll_I2C() the rx_buffer is re-filled,
*  the old contents of the rx_buffer is lost. This is the reason
*  to create a separate cw_buffer that retains its contents until
*  new text arrives to fill the cw_buffer with.
*/
bool cpyRXtoCW() {
  static constexpr const char txt0[] PROGMEM = "cpyRXtoCW(): ";
  bool ret = true;
  if (rcvd_message_flag) {
    if (cw_buffer == NULL) {
      if (!create_cw_buffer()) {
        return false;
      }
    }
    cw_buffer_idx = rx_buffer_idx; // copy index value
    if (cw_buffer_idx > 0) {
      if (rx_buffer && cw_buffer) {
        memcpy(cw_buffer, rx_buffer, cw_bufferSize); // Copies rx_buffer to cw_buffer
        size_t cw_buffer_idx = calcCWbufferLen();
        Serial.print(txt0);
        Serial.println(F("RX_buffer copied to CW_buffer"));
        Serial.print(txt0);
        Serial.print(F("contents of cw_buffer = "));
        Serial.printf("\"%.*s\"\n", cw_buffer_idx, (char *)&cw_buffer[4]);

        Serial.print(txt0);
        Serial.print(F("size cw_buffer = "));
        Serial.println(cw_buffer_idx);
      }
      else
        ret = false;
    }
  }
  return ret;
}

/*
void onRequest() {
  Wire.print(TXpacketNr++);
  Wire.print(" Packets.");
  Serial.println("onRequest");
}
*/

void onReceive(int howMany) {
  //Wire.readBytes(buffer, howMany);
  RXpacketNr++;
  rx_buffer_howMany = howMany;
  receiveFlag = true;
}

void LedColor(my_colors color)
{
  switch (color)
  {
    case RED:
      leds[0] = CRGB::Red; // Set the LED to red
      break;
    case GREEN:
      leds[0] = CRGB::Green; // Set the LED to green
      break;
    case BLUE:
      leds[0] = CRGB::Blue; // Set the LED to blue
      break;
    case WHITE:
      leds[0] = CRGB::White; // Set the LED to white
      break;
  case BLACK:
      leds[0] = CRGB::Black; // Set the LED to black
      break;
  default:
      leds[0] = CRGB::Black; // Set the LED to black
      break;
  };
    FastLED.show(); // Update the LED
}

void btn_beep() {
  LedColor(GREEN);
  echoSPKR.playBeep(btn_tone1);
  //echoSPKR.playBeep(btn_tone1.freq, btn_tone1.time_ms, btn_tone1.maxval, btn_tone1.modal);
  delay(100);
  echoSPKR.playBeep(btn_tone2);
  //echoSPKR.playBeep(btn_tone2.freq, btn_tone2.time_ms, btn_tone2.maxval, btn_tone2.modal);
  delay(100);
  LedColor(RED);
}

void send_morse() {
  // Define variables:
  static constexpr const char txt0[] PROGMEM = "send_morse(): ";
  std::vector<int> lst; // create an empty list
  int i;
  int j;
  int le;
  int le2;
  int n;
  int n2;
  int cw_buffer_char_cnt;
  char c;
  char c2;
  size_t writeSize;
  bool morse_default = false; // morse_default, when true, we use the standard "paris " test to send.
  bool my_morse_debug = true;
  bool word_end = false;
  bool stop = false;
  int unit_count = 0;
  int word_count = 0;
  unsigned long morse_start_t;
  unsigned long curr_t = 0;
  unsigned long interval_t = 60 * 1000;
  unsigned long elapsed_t = 0;
  int btn_red = 0;
  int btn_blu = 0;
  int text_begin_idx = 0; // If morse_default then this value = 0.
  //                         For a received text the text string starts at byte 4 of cw_buffer. 
  /*
  if (rx_buffer_idx == 0) {
    Serial.print(txt0);
    Serial.println(F("Received text is empty. Exiting this function"));  
    return;
  }
  */
  Serial.print(txt0);
  Serial.println(F("Starting..."));
  Serial.print(F("tone dot (and dash) frequency = "));
  Serial.printf("%d Hz\n", tone_dot.freq);
  Serial.printf("tone_dot.modal  = %s\n", (tone_dot.modal  == 1) ? "true": "false");
  Serial.printf("tone_dash.modal = %s\n", (tone_dash.modal == 1) ? "true": "false");
  dot_dash_time();

  if (!cmd_morse_go_flag && receiveFlag && cw_buffer_idx > 0) {
    Serial.print(txt0);
    Serial.print(F("cw_buffer_idx = "));
    Serial.println(cw_buffer_idx);
    morse_default = false;
    text_begin_idx = 4;
    cw_buffer_char_cnt = 0;
    Serial.print(F("Text to send:\nin morse code = "));
    Serial.printf("\"%.*s\"", cw_buffer_idx-1, (char *)&cw_buffer[text_begin_idx]);
    Serial.print(F(", length = "));
    Serial.printf("%d\n", cw_buffer_idx - text_begin_idx -1);
    if (my_debug) {
      Serial.print(F("in bytes = "));
      for (i = 4; i < cw_buffer_idx-1; i++) {
        Serial.printf("0x%02x ", cw_buffer[i]);
      }
      Serial.println();
    }
    if (!my_debug)
      Serial.println(F("Sending..."));
  }
  else {  // if cmd_morse_go_flag is active
    // use default "paris"
    morse_default = true;
    text_begin_idx = 0;
    const char tmp[] = "paris ";
    cw_buffer_idx = strlen(tmp); // was: tmp.length()+1; // +1 for null terminator
    Serial.print(txt0);
    Serial.print(F("going to send \""));
    Serial.print(tmp);
    Serial.print(F("\", length = "));
    Serial.println(cw_buffer_idx);
    if (cw_buffer == NULL)
      if (!create_cw_buffer())
        return;
    for (i = 0; i < cw_buffer_idx; i++) {
      cw_buffer[i] = static_cast<uint8_t>(tmp[i]);
    }
    cw_buffer[i] = 0; // was: '\0' null terminator
    Serial.print(txt0);
    Serial.print(F("contents cw_buffer = "));
    Serial.printf("\"%.*s\"\n", cw_buffer_idx, (char *)&cw_buffer[0]);
  }
  if (my_debug) {
    Serial.println(F("Going to send the text now..."));
    Serial.println(F("In the lists like: [2,1], 2 represents a dash, 1 respresents a dot"));
  }
  show_delays();
  morse_start_t = millis();
  while (true) {
    curr_t = millis();
    elapsed_t = curr_t - morse_start_t;
    if (elapsed_t >= interval_t) {
      stop = true;
      Serial.print("\nOne minute limit reached.\n");
      break;
    }
    if (cw_buffer == NULL)
      return;
    Serial.printf("%2d) ", word_count+1);
    for (i = text_begin_idx; i < cw_buffer_idx; i++) {
      if (M5.Btn.wasPressed()) {
        Serial.println();
        Serial.print(txt0);
        Serial.println(F("Button was pressed. Exiting function."));
        btn_beep();
        delay(100);  // button debounce delay
        freeCWBuffer();
        return;
      }
      else {
        int8_t nrBytes = poll_I2C(); // check for rcvd msg
        if (nrBytes == 6) // This could be a CMD type of message
          handle_rx(nrBytes);
        if (cmd_morse_end_flag == true) {
          Serial.println();
          Serial.print(txt0);
          Serial.println(F("CMD_MORSE_END received. Exiting function."));
          btn_beep();
          delay(100);  // button debounce delay
          freeCWBuffer();
          cmd_morse_go_flag = false;
          cmd_morse_end_flag = false;
          return;
        }
      }
      if (i+1 < cw_buffer_idx && static_cast<int>(cw_buffer[i+1]) == 32) 
        word_end = true;
      c = cw_buffer[i];
      n = static_cast<int>(c); // Calculate the ASCII value
      if (n == 0)  // string terminator. Exit for loop.
        break;  
      if (n == 32) {
        if (my_debug)
          Serial.println(F("word space delay"));
        if (my_morse_debug)
          Serial.print("|  7  |\n");
        delay(dly7);
        unit_count += 7;
      } else {
        if (n >= 65 and n <= 90)  // Convert upper case to lower case
          n2 = n + 32;    // python example: ord('A') = 65. ord('a') = 97
        else if (n > 122)  // value higher than character 'z'
          n2 = 63;         // replace by a question mark '?'
        else
          n2 = n;
        c2 = static_cast<char>(n2);  // convert back to ASCII
        if (my_debug) {
          Serial.print(txt0);
          Serial.print(F("searching in morse_tx_dict for character: \'"));
          Serial.print(c2);
          Serial.printf("\', with value %d\n", c2);
        }
        if (morse_txt_dict.find(c2) == morse_txt_dict.end()) {
          //Serial.printf("character \'%s\' not found in morse_txt_dict\n", n2);
          c2 = '?';  // use ? as replacement for character received and not found in dictionary.
        } else {
          // Extract the list and assign it to a variable
          lst = morse_txt_dict[c2];
          if (my_debug) {
            Serial.print(F("Sending character "));
            Serial.print(c2);
            Serial.print(F(" = "));
          }
          // Calculate the length of the extracted list
          size_t le2 = lst.size();
          if (le2 > 0) {
            if (my_debug) {
              Serial.print("[");
              for (int n = 0; n < le2; n++) {
                Serial.print(lst[n]);
                if (n < le2 -1) {
                  Serial.print(", ");
                }
              }
              Serial.println("]");
            }
            for (j = 0; j < le2; j++) {
              if (lst[j] == 2) {
                if (my_debug)
                  Serial.println(F("Sending a dash"));
                if (my_morse_debug)
                  Serial.print("---");
                //Serial.printf("tone_dash.modal = %d\n", tone_dash.modal);
                unit_count += 3;
                writeSize = echoSPKR.playBeep(tone_dash);
                //writeSize = echoSPKR.playBeep(tone_dash.freq, tone_dash.time_ms, tone_dash.maxval, tone_dash.modal);
              } else if (lst[j] == 1) {
                if (my_debug)
                  Serial.println(F("Sending a dot"));
                if (my_morse_debug)
                  Serial.print(".");
                //Serial.printf("tone_dot.modal = %d\n", tone_dot.modal);
                unit_count += 1;
                writeSize = echoSPKR.playBeep(tone_dot);
                //echoSPKR.playBeep(tone_dot.freq, tone_dot.time_ms, tone_dot.maxval, tone_dot.modal);
              }
              // if (writeSize > 0) {
              //   Serial.print(F("writeSize = "));
              //   Serial.println(writeSize);
              // }
              // for example: after 'e' or 't' no character space delay
              // and not after the last dot or dash of the character
              if (le2 > 1 && j < le2-1) {
                if (my_debug)
                  Serial.println(F("unit space delay"));
                if (my_morse_debug)
#ifdef SHOW_UNITS
                  Serial.print("|1|");
#else
                  Serial.print(" ");
#endif
                delay(dly1); // was: (dly1)
                unit_count += 1;
              }
            }
            if (i == le -1) {
              if (my_debug)
                Serial.println(F("word space delay"));
              if (my_morse_debug)
                Serial.print("|   7   |");
              delay(dly7);
              unit_count += 7;
            } else {
              if (!word_end) {
                if (my_debug)
                  Serial.println(F("character space delay"));
                if (my_morse_debug)
                  Serial.print("| 3 |");
                delay(dly3);
                unit_count += 3;
              }
              else
                word_end = false;
            }
          }
        }
        M5.update();
      }
    } // end-of-for-loop
    Serial.println(); // New line after the last character (morse code representation)
    if (my_debug) {
      Serial.println(F("All characters sent"));
      Serial.print(F("Units sent = "));
      Serial.printf("%d\n", unit_count);
    }
    word_count += 1;
  }
  Serial.print(F("Number of units sent = "));
  Serial.printf("%d, ", unit_count);
  Serial.print(F("this is "));
  Serial.printf("%d ", static_cast<int>(unit_count/word_count)); 
  Serial.println(F("units per word."));
  Serial.print(F("Number of words sent: "));
  Serial.print(word_count);
  Serial.println(".");
  Serial.print(F("This equals to a morse code speed of "));
  Serial.printf("%d ", word_count);
  Serial.println(F("words/minute."));
  freeCWBuffer();
}

void setup() {
  static constexpr const char txt0[] PROGMEM = "setup(): ";
  static constexpr const char txt1[] PROGMEM = "I2C Pins set ";
  static constexpr const char *txts[] PROGMEM = {
    "M5Stack M5Atom Echo",              // 0
    "I2C test",                         // 1
    "to receive ",                      // 2
    "text for ",                        // 3
    "morse code ",                      // 4
    "and ",                             // 5
    "speed change commands",            // 6
    "from a M5Stack Cardputer device.", // 7
    "successfull.",                     // 8
    "failed."                           // 9
  };
  // Note that Serial.begin() is started by M5.begin()
  // Note if, in M5.begin() the second boolean paramer is true then the Wire.begin command will fail.
  M5.begin(true, false, false); // Init M5 Atom Echo  (SerialEnable, I2CEnable, DisplayEnable)

  Serial.begin(115200);
  delay(1000);

  //Serial.println(F("M5Stack M5Atom Echo new \"ATOMECHOSPKR class\" text rx to morse test"));
  Serial.printf("\n%s\n%s\n%s%s%s\n%s%s%s\n%s\n\n", 
    txts[0], 
    txts[1], 
    txts[2], txts[3], txts[4], 
    txts[5], txts[2], txts[6],
    txts[7] );

  Serial.print(txt0);
  Serial.print(F("morse default speed index = "));
  Serial.println(new_speed_idx);
  show_delays();
  Serial.println();

  delay(3000); // wait before doing other things

  FastLED.addLeds<NEOPIXEL, ATOMECHO_LED_PIN>(leds, NUM_LEDS); // Initialize FastLED
  FastLED.setBrightness(50); // Set brightness
  LedColor(RED);

  if (!create_rx_buffer()) {
    Serial.print(txt0);
    go_restart();
  }

  if (!create_cw_buffer()) {
    Serial.print(txt0);
    go_restart();
  }

  // See: https://docs.m5stack.com/en/arduino/m5unified/m5unified_appendix
  //auto board = m5::board_t::board_M5Atom  // M5.getBoard();   // doesn't work for the M5Echo
  auto board = "M5Atom Echo";
  
  Serial.print(txt0);
  Serial.print(F("Board type: "));
  Serial.println(board);

  bool result = Wire.setPins(GROVE_SDA, GROVE_SCL);
  Serial.print(txt0);
  Serial.print(txt1);
  if (result == true) 
    Serial.println(txts[8]);
  else 
    Serial.println(txts[9]);
  
  size_t bufSz = Wire.setBufferSize(sizeof(I2C_TEXT_MESSAGE));
  Serial.print(F("setting I2C buffersize to "));
  Serial.print(sizeof(I2C_TEXT_MESSAGE));
  Serial.print(" ");
  if (bufSz == 0) {
    Serial.println(txts[9]);
  } else {
    Serial.println(txts[8]);
  }
  Wire.onReceive(onReceive);
  // Wire.onRequest(onRequest);

  result = Wire.begin((uint8_t)I2C_DEV_ADDR);
  if (result == false) {
    Serial.print(txt0);
    Serial.print(F("Can't start I2C on bus nr: "));
    Serial.printf("%d.\n", i2c_bus_num);
    go_restart();
  } else {
    Serial.print(txt0);
    Serial.print(F("Successfully connected onto I2C bus nr: "));
    Serial.printf("%d.\n", i2c_bus_num);
    I2C_active = true;
  }

  esp_err_t err = ESP_OK;

  try {
    //err = echoSPKR.begin(88200); // standard rate = 44100
    err = echoSPKR.begin(); // standard rate = 44100
  }
  catch (esp_err_t err) {
    Serial.print(txt0);
    Serial.print(F("While starting echoSPKR, error: "));
    Serial.print(err);
    Serial.println(F("occurred."));
    go_restart();
  }
  // set_volume();  // set speaker volume to default <<== Is not necessary! echoSPKR.begin() does this already!

  // delay(100);
}

void cleanup() {
  freeRxBuffer();
  freeCWBuffer();
  receiveFlag = false;
  rcvd_cmd_flag = false;
  cmd_morse_go_flag = false;
  cmd_morse_end_flag = false;
  cmd_reset_flag = false;
  rcvd_message_flag = false;
  cmd_volume_echo_flag = false;
  rcvd_set_speed_flag = false;
}

int8_t poll_I2C() {
  static constexpr const char txt0[] PROGMEM = "poll_I2C(): ";
  int8_t nrBytes = -1;
  // Poll I2C
  nrBytes = static_cast<int8_t>(Wire.available());
  if (nrBytes > 0) {
    Serial.println();
    Serial.print(txt0);
    Serial.print(F("Nr of bytes available in msg via Wire (I2C) "));
    Serial.println(nrBytes);
  }
  return nrBytes;
}

void loop() {
  static constexpr const char txt0[] PROGMEM = "loop(): ";
  
  cleanup();

  bool start = true;

  delay(5000); // Wait for user to clear the serial monitor window
  Serial.print('\n');
  Serial.print(txt0);
  Serial.println(F("entering the while loop..."));
  Serial.print(txt0);
  Serial.print(F("ESP.getFreeHeap = "));
  Serial.println(ESP.getFreeHeap());

  while (true) {
  
    if (start && receiveFlag) {
      start = false;
      if (!I2C_active)
        send_morse();
    }

    int8_t nrBytes = poll_I2C();
    if (nrBytes > 0) {
      receiveFlag = true;
      handle_rx(nrBytes);
      //cleanup();
    }

    if (cmd_morse_go_flag) {
      send_morse();
      cmd_morse_go_flag = false; // reset flag
      cmd_morse_end_flag = false; // same
    }

    if (rcvd_message_flag == true) {
      Serial.print(txt0);
      Serial.print(F("receiveFlag = "));
      Serial.printf("%s\n", (receiveFlag == true) ? "true" : "false");
      ///handle_rx();
      send_morse();
      if (!start)
        cleanup();
    }

    if (rcvd_set_speed_flag == true)
      set_speed();

    if (cmd_volume_echo_flag) {
      cmd_volume_echo_flag = false;
      set_volume();
    }
    
    if (M5.Btn.wasPressed()) {
      Serial.print(txt0);
      Serial.println(F("Button was pressed"));
      btn_beep();
      // Cleanup and reset all flags.
      cleanup();
      delay(1000);
      start_t = millis();
    }
    
    M5.update();
    LedColor(RED);
  }
}

