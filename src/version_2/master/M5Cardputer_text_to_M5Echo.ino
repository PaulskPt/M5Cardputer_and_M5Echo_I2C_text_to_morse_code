/*
*******************************************************************************
* Copyright (c) 2025 by @PaulskPt (Github)
* License: MIT
*
* Arduino sketch: M5Cardputer_text_to_M5Echo.ino 
* 
* Purpose: to input text from the keyboard of the M5Cardputer,
* then send the text to a M5Echo, which in turn will send 
* the received text in morse code to its loudspeaker. 
*
* 2025-03-29
*
* Example for input text:
* https://github.com/m5stack/M5Cardputer/blob/master/examples/Basic/keyboard/inputText/inputText.ino
*
* For an Arduino example of sending text via I2C, see:
* https://forum.arduino.cc/t/send-text-string-over-i2c-between-two-arduinos/326052/5
*
* IDE: Arduino v2.3.5. Inside the Arduino IDE > Tools > Board chosen: M5Cardputer
*
* Updates 2025-04-02:
* Send command messages, e.g.: 
* a) to start or stop sending texts: free texts sent from this device (master) to the slave device,
* or in the slave device pre-programmed fixed speed test morse text 'paris ';
* b) to send command messages to increase or decrease the speed of the morse code send by the slave device;
* c) to send a command message CMD_RESET, to remotely order the slave device to execute a software reset (ESP.restart()).
*
* Note: the acompanion Arduino sketch for the M5tack M5Atom Echo is:
*       G:\Dropbox\Documentos\Arduino\M5Atom_Echo_I2C_text_rx_to_morse\M5Atom_Echo_I2C_text_rx_to_morse.ino
*
* M5Stack dualbutton unit example: https://github.com/m5stack/M5Stack/tree/master/examples/Unit/DUAL_BUTTON
*
* Update: 2025-11-16: added functionality to use GROVE PORT-A of the M5Cardputer for both I2C communication
          as to read status of the buttons of a M5Stack Dualbutton unit. For this the function setPins() was created.
* Update: 2025-11-17: in handle_kbd_input(): 
*                     a) if display is sleeping, wake it up on keypress, do not process the key further;
*                     b) added debounce filtering.
*/

/**
 * @file inputText.ino
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief M5Cardputer input text test
 * @version 0.1
 * @date 2023-10-13
 *
 *
 * @Hardwares: M5Cardputer
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 */

 // See also: https://forum.arduino.cc/t/using-twowire-i2c2-with-esp32-two-pressure-sensors/1061318/10 

#include <Arduino.h>
#include "pins_arduino.h"
#include "Wire.h"  // Leave this include line at this position, before the ones below! Otherwise compile error will occur!
#include <M5Cardputer.h>
#include <M5GFX.h>
#include "puter_echo.h"
#include <iostream> // needed for std::cout
#include <algorithm> // needed for if (!std::find(std::begin(cmd_ltrs), std::end(cmd_ltrs), data[2]) != std::end(cmd_ltrs)) 
#include <M5Unified.h>  // or M5Cardputer.h depending on your setup

/* For the StampS3 builtin RGB LED see the discussion:
   https://www.reddit.com/r/CardPuter/comments/1b45ohu/did_you_know_the_cardputer_stamps3_has_an_rgb_led/
   And the example at: https://github.com/m5stack/STAMP-S3/blob/main/examples/Led/Led.ino
*/
#include <FastLED.h>

#define PIN_BUTTON 0
#define PIN_LED    21
#define NUM_LEDS   1

CRGB leds[NUM_LEDS];
uint8_t led_ih             = 0;
uint8_t led_status         = 0;
String led_status_string[] = {"Rainbow", "Red", "Green", "Blue", "White", "Off"};
unsigned long rainbowStart = 0;
#define LED_RAINBOW 0
#define LED_RED     1
#define LED_GREEN   2
#define LED_YELLOW  3
#define LED_BLUE    4
#define LED_CYAN    5
#define LED_WHITE   6
#define LED_OFF     7

// #define USE_M5STACK_EXAMPLE  // see handle_rx()

//#ifdef MY_DEBUG
//#undef MY_DEBUG
//#endif

#ifndef MY_DEBUG
#define MY_DEBUG
#endif

// Define print macros
#ifdef MY_DEBUG
  #define DEBUG_PRINT(x)       Serial.print(x)
  #define DEBUG_PRINTLN(x)     Serial.println(x)
  #define DEBUG_PRINTF(...)    serialPrintf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#ifndef ARDUINO_M5STACK_CARDPUTER
#define ARDUINO_M5STACK_CARDPUTER
#endif

#ifndef USE_DUALBUTTON
#define USE_DUALBUTTON
#endif

#define I2C_DEV_ADDR    0x55

uint32_t TXpacketNr = 14385; // = 0x3830 (for test in send_speed_chg() )
uint32_t RXpacketNr = 0;

#ifdef USE_DUALBUTTON
int btn_blue_last_value = 0, btn_red_last_value = 0;
int btn_blue_curr_value = 0, btn_red_curr_value = 0;
bool dualbtn_red_pressed = false; // yellow wire 
bool dualbtn_blue_pressed = false; // white wire
uint8_t BLUE_BTN = G1;
uint8_t RED_BTN  = G2;
unsigned long lastRedEvent = 0;
unsigned long lastBlueEvent = 0;
const unsigned long debounceDelay = 50; // ms
// Global queued command (Echo message codes)
uint8_t queuedCmd = CMD_DO_NOTHING;
#endif

unsigned long feedbackShownAt = 0;
const unsigned long feedbackDuration = 2000; // 2 seconds
bool feedbackActive = false;

unsigned long lastActivity = 0;
const unsigned long idleTimeout = 30000; // 30 seconds, adjust as needed
bool displaySleeping = false;

bool pinsSetForI2C = false;

// Copied from m5stack_cardputer/pins_arduino.h 
/*
static const uint8_t SDA = 13;
static const uint8_t SCL = 15;
// TF card:
static const uint8_t SS    = 12;
static const uint8_t MOSI  = 14;
static const uint8_t MISO  = 39;
static const uint8_t SCK   = 40;
// GROVE 
static const uint8_t G0  = 0; // According to the schematic this pin is SDA
static const uint8_t G1  = 1; // same                                   SCL

*/
// end of copied from m5stack_cardputer
// according to \m5stack_cardputer\pins_arduino.h SDA = 13 and SCL = 15
#define GROVE_SDA G2 // G1
#define GROVE_SCL G1 // G0

#ifdef USE_GROVE
#undef USE_GROVE
#endif

#ifdef USE_GROVE
#define GROVE_PIN1 G2 // 26 // Define the first  pin of Port A (Yellow) SDA
#define GROVE_PIN2 G1 // 32 // Define the second pin of Port A (White ) SCL
#endif

bool I2C_device_found = false;

M5Canvas canvas(&M5Cardputer.Display);

String data = "> ";
size_t rx_bufferSize = 128;
uint8_t speed_idx = speed_default;
uint8_t *rx_buffer;
uint8_t rx_buffer_howMany = 0;
bool use_speaker = false;
bool receiveFlag = false;
bool ack_rcvd = false;
bool show_commands_flag = false;
bool ctrl_pressed = false;
bool morse_go_flag = true;
bool morse_end_flag = false;
bool key_reset_flag = false;
bool decrease_pressed = false;
bool increase_pressed = false;
bool volume_echo_flag = false;

unsigned long start_t = millis();
unsigned long curr_t = 0;
unsigned long timeout_limit_t = 2000; // timeout = 2 seconds
unsigned long elapsed_t = 0;

// Default version for most use cases (160-byte buffer)
void serialPrintf(const char* format, ...) {
  char buffer[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
}

// Extended version where you can specify buffer size
// (= Function Overloading!)
void serialPrintf(size_t bufferLen, const char* format, ...) {
  char buffer[bufferLen];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, bufferLen, format, args);
  va_end(args);
  Serial.print(buffer);
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

void onRequest() {
  Wire.print(TXpacketNr++);
  Wire.print(" Packets.");
  Serial.println("onRequest");
}

void onReceive(int howMany) {
  //Wire.readBytes(buffer, howMany);
  rx_buffer_howMany = static_cast<int8_t>(howMany);
  receiveFlag = true;
}

#ifdef USE_GROVE
void ack_cb() {
  ;
}
#endif

void beep() {
  if (use_speaker) {
    //M5Cardputer.Speaker.tone(1200, 100);
    //delay(100);
    M5Cardputer.Speaker.tone(1400, 100);
    delay(100);
  }
}

bool setPins(bool for_i2c = true) {
  bool result = false;

  if (for_i2c) {
    if (pinsSetForI2C) {
      return true;  // Already in I2C mode
    }

    // Configure pins for I2C
    result = Wire.setPins(GROVE_SDA, GROVE_SCL);
    if (result) {
      Wire.begin();  // Re-init bus to ensure clean state
      delay(20);     // Short stabilization delay
      pinsSetForI2C = true;
    }
  } else {
#ifdef USE_DUALBUTTON
    if (!pinsSetForI2C) {
      return true;  // Already in button mode
    }

    // Defensive: ensure bus is idle before switching
    Wire.end();  // Release I2C bus drivers

    pinMode(RED_BTN, INPUT_PULLUP);
    pinMode(BLUE_BTN, INPUT_PULLUP);
    pinsSetForI2C = false;
    result = true;
#else
    // Fallback: keep I2C pins configured
    result = Wire.setPins(GROVE_SDA, GROVE_SCL);
#endif
  }

  DEBUG_PRINT(F("setPins(): result = "));
  DEBUG_PRINTLN(result ? F("true") : F("false"));
  DEBUG_PRINT(F("Mode = "));
  DEBUG_PRINTLN(pinsSetForI2C ? F("I2C") : F("Buttons"));

  return result;
}



void wait_for_keypress(bool useTimer = false) {
  unsigned long start = millis();
  const unsigned long timeout = 5000; // 5 seconds, adjust as needed

  while (true) {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
      if (M5Cardputer.Keyboard.isPressed()) {
        return; // exit on keypress
      }
    }

    if (useTimer && (millis() - start > timeout)) {
      // exit after timeout if timer mode is enabled
      return;
    }
  }
}


void disp_feedback(const char* msg, uint16_t color = GREEN) {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 28,
                               M5Cardputer.Display.width(), 28, BLACK);

  M5Cardputer.Display.setTextColor(color);
  M5Cardputer.Display.setCursor(4, M5Cardputer.Display.height() - 24);
  M5Cardputer.Display.print(msg);

  // Reset immediately to white so main screen stays consistent
  M5Cardputer.Display.setTextColor(WHITE);

  feedbackShownAt = millis();
  feedbackActive = true;
}

unsigned long ledOnAt = 0;

void blinkFeedback(uint8_t cmd) {
  switch (cmd) {
    case CMD_MORSE_GO:   led_status = LED_GREEN; break;
    case CMD_MORSE_END:  led_status = LED_RED; break;
    case CMD_RESET:      
      led_status = LED_RAINBOW;
      rainbowStart = millis();
      break;
    case CMD_VOLUME_CHG: led_status = LED_BLUE; break;
    case CMD_SPEED_CHG:  led_status = LED_CYAN; break;
    default:             led_status = LED_OFF; break;
  }

  set_led();             // LED ON
  ledOnAt = millis();    // record when LED turned on
}

// Unified helper for sending Echo commands
void processCommand(uint8_t cmd) {
  if (cmd == CMD_DO_NOTHING) return;

  setPins(true);
  send_cmd(cmd);

  // Visual feedback
  switch (cmd) {
    case CMD_MORSE_GO:   disp_feedback("Morse GO", GREEN); break;
    case CMD_MORSE_END:  disp_feedback("Morse END", RED); break;
    case CMD_RESET:      disp_feedback("Reset", YELLOW); break;
    case CMD_VOLUME_CHG: disp_feedback("Volume", CYAN); break;
    default:             disp_feedback("Command sent", WHITE); break;
  }
  blinkFeedback(cmd);
}


#ifdef USE_DUALBUTTON
bool ck_dualbutton() {
  static constexpr const char *txts[] PROGMEM = {
    "dualbtn",  // 0
    "blue",     // 1
    "red",      // 2
    "pressed",   // 3
    "released"   // 4
  };
  unsigned long now = millis();
  bool btn_pressed = false;

  // --- RED button ---
  if (digitalRead(RED_BTN) == LOW && !dualbtn_red_pressed) {
    lastActivity = millis(); // reset idle timer
    if (now - lastRedEvent > debounceDelay) {
      dualbtn_red_pressed = true;
      btn_pressed = true;
      lastRedEvent = now;
      //Serial.println(F("dualbtn red pressed"));
      Serial.printf("\nAt %lu: ", now);
      Serial.printf("%s %s %s\n", 
      txts[0], 
      txts[2], 
      txts[3]);
      queuedCmd = CMD_MORSE_GO;
    }
  }
  if (digitalRead(RED_BTN) == HIGH && dualbtn_red_pressed) {
    if (now - lastRedEvent > debounceDelay) {
      dualbtn_red_pressed = false;
      lastRedEvent = now;
      //Serial.println(F("dualbtn red released"));
      Serial.printf("At %lu: ", now);
      Serial.printf("%s %s %s\n", 
      txts[0], 
      txts[2], 
      txts[4]);
      if (queuedCmd == CMD_MORSE_GO) {
        processCommand(queuedCmd);
        queuedCmd = CMD_DO_NOTHING;
      }
    }
  }

  // --- BLUE button ---
  if (digitalRead(BLUE_BTN) == LOW && !dualbtn_blue_pressed) {
    lastActivity = millis(); // reset idle timer
    if (now - lastBlueEvent > debounceDelay) {
      dualbtn_blue_pressed = true;
      lastBlueEvent = now;
      btn_pressed = true;
      //Serial.println(F("dualbtn blue pressed"));
      Serial.printf("\nAt %lu: ", now);
      Serial.printf("%s %s %s\n", 
      txts[0], 
      txts[1], 
      txts[3]);
      queuedCmd = CMD_MORSE_END;
    }
  }
  if (digitalRead(BLUE_BTN) == HIGH && dualbtn_blue_pressed) {
    if (now - lastBlueEvent > debounceDelay) {
      dualbtn_blue_pressed = false;
      lastBlueEvent = now;
      //Serial.println(F("dualbtn blue released"));
      Serial.printf("At %lu: ", now);
      Serial.printf("%s %s %s\n", 
      txts[0], 
      txts[1], 
      txts[4]);
      if (queuedCmd == CMD_MORSE_END) {
        processCommand(queuedCmd);
        queuedCmd = CMD_DO_NOTHING;
      }
    }
  }
  return btn_pressed;
}
#endif

float dispTextSizeSmall = 0.5;
float dispTextSize      = 1.0;

void disp_main_screen() {
  static constexpr const char *txts[] PROGMEM = {"Input: ", "<Key> +", "<Enter>"};
  M5Cardputer.Display.clear();
  M5Cardputer.Display.setRotation(1);

  // Always reset font and size for main screen
  M5Cardputer.Display.setTextFont(&fonts::FreeMonoBold18pt7b);
  M5Cardputer.Display.setTextSize(dispTextSize);
  M5Cardputer.Display.setTextColor(WHITE);   // reset to default white

  canvas.setTextFont(&fonts::FreeMonoBold18pt7b);
  canvas.setTextSize(dispTextSize);
  canvas.createSprite(M5Cardputer.Display.width() - 4,
                      M5Cardputer.Display.height() - 4);
  canvas.setTextScroll(true);

  M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(),
                               M5Cardputer.Display.height() - 28, GREEN);

  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 4,
                               M5Cardputer.Display.width(), 4, GREEN);

  canvas.createSprite(M5Cardputer.Display.width() - 8,
                      M5Cardputer.Display.height() - 36);
  canvas.pushSprite(4, 4);

  // Draw text with correct size and color
  M5Cardputer.Display.drawString(txts[0], 4,  4);
  M5Cardputer.Display.drawString(txts[1], 4, 34);
  M5Cardputer.Display.drawString(txts[2], 4, 64);

  data = "> ";
  M5Cardputer.Display.drawString(data, 4, M5Cardputer.Display.height() - 24);
}


void disp_commands(bool waitForKeypress = false) {
  M5Cardputer.Display.clear();
  //canvas.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  canvas.setTextFont(&fonts::FreeMonoBold18pt7b);
  canvas.setTextSize(dispTextSizeSmall);
  canvas.createSprite(M5Cardputer.Display.width() - 4,
                      M5Cardputer.Display.height() - 4);
  canvas.setTextScroll(true);
  canvas.println("--- COMMANDS: CTRL+ ---");
  canvas.println(" i  increase speed");
  canvas.println(" d  decrease speed");
  //canvas.pushSprite(4, 4);
  //wait_for_keypress();
  canvas.println(" r  reset device");
  canvas.println(" s  sound on/off");
  canvas.println(" g  morse go");
  canvas.println(" e  morse end");
  canvas.pushSprite(4, 4);
  wait_for_keypress(waitForKeypress);
  //M5Cardputer.Display.clear();
}

void send_text_msg() {
    static constexpr const char txt0[] PROGMEM = "send_text_msg(): ";
    static constexpr const char *txts[] PROGMEM = {
      "sending ",       // 0
      "text ",          // 1
      "message",        // 2
      "bytes ",         // 3
      "sent",           // 4
      "Number of ",     // 5
      "Going to ",      // 6
      "send "           // 7
    };
  TXpacketNr++;
  Serial.print(txt0);
  Serial.print(F("New TXpacketNr = "));
  Serial.println(TXpacketNr);

  DEBUG_PRINT(txt0);
  DEBUG_PRINT(F("String data  = \""));
  DEBUG_PRINT(data);
  DEBUG_PRINTLN(F("\""));

  uint8_t le_data = data.length();
  uint8_t le_message;
  uint8_t le_packet;
  uint8_t i;
  uint8_t j;
  uint8_t *message;
  le_packet = 0;

  message = (uint8_t *)malloc( 4 + le_data + 1);
  message[0] = I2C_DEV_ADDR;
  message[1] = (TXpacketNr & 0xFF00) >> 8; // put MSB of uint16_t
  message[2] = TXpacketNr & 0xFF; // put LSB of uint16_t
  message[3] = TEXT_MESSAGE;
  le_packet += 4;

  for (j = 0; data.c_str()[j] != '\0'; j++) {
    message[4 + j] = data[j];
    le_packet++;
  }
  //message[4 + le_data + 1] = '\0'; // end of string marker
  message[le_packet] = '\0'; // end of string marker
  le_packet++; // include the '\0' byte


  //Serial.printf("le_data = %d\n", le_data);
  //Serial.print(txt0);
  //Serial.printf("le_packet = %d\n", le_packet); // this le_packet, in this moment, excludes the '\0' byte
  for (j = 0; j < le_packet; j++) {
    DEBUG_PRINT(txt0);
    DEBUG_PRINTF("message[%d] = '0x%02x' ('%c')\n", j, message[j], message[j]);
  }

  le_message=sizeof(message)/sizeof(message[0]);
  
  String s;
  if (message[3] == TEXT_MESSAGE)
    s = "TEXT_MESSAGE";
  else if (message[3] == CMD_SPEED_CHG)
    s = "CMD_SPEED_CHG";

  //  Note:
  //  %.*s = %.*s 
  // is a format specifier for printing a substring, 
  // where .* lets you specify the length dynamically.
  Serial.print(txt0);
  Serial.println("Message contents:");
  Serial.printf("\tI2C_DEV_ADDR = 0x%02x, \
               \n\tTXpacketNr   = %u, \
               \n\tmsgType      = %s, \
               \n\ttext         = \"%.*s\" (+ \'\\0\')\n", \
    message[0], 
    (message[1] << 8) | message[2], 
    s,
    le_data, 
    (char *)&message[4]
  );

  Serial.print(txt0);
  Serial.print(F("length of packet to send = "));
  Serial.println(le_packet);
  Serial.print(txt0);
  Serial.println(F("message in bytes: "));
  //uint8_t le3 = sizeof(message);
  for (i = 0; i < le_packet; i++) {  // 0..8
    Serial.printf("0x%02x ", message[i]);
  }
  Serial.println();
  
  size_t bytes_sent = 0;
  setPins(); // set I2C pins
  Wire.beginTransmission(I2C_DEV_ADDR);
  bytes_sent = Wire.write((uint8_t *)message, le_packet);
  Wire.endTransmission(true);
  delay(500);

  Serial.print(txt0);
  if (bytes_sent > 0) {
    Serial.printf("%s%s %s. %s%s%s: %d\n",
      txts[1], txts[2], txts[4],
      txts[5], txts[3], txts[4],
      bytes_sent);
  } else {
    Serial.printf("%s%s%s", txts[0], txts[1], txts[2]);
    Serial.println(F(" to slave failed."));
  }
  //free(message);
 }

void send_cmd(uint8_t cmd_idx) {
  static constexpr const char txt0[] PROGMEM = "send_cmd(): ";
  TXpacketNr++;

  uint8_t message[6];  // Fixed-size buffer on stack
  message[0] = I2C_DEV_ADDR;
  message[1] = (TXpacketNr >> 8) & 0xFF;
  message[2] = TXpacketNr & 0xFF;
  message[3] = cmd_idx;

  uint8_t tmp_idx = cmd_idx - CMD_DO_NOTHING;
  uint8_t le_cmd_arr = sizeof(cmd_idx_arr) / sizeof(cmd_idx_arr[0]);

  String s1, s2;
  if (tmp_idx < le_cmd_arr) {
    s1 = cmd_idx_arr[tmp_idx];
    if ((cmd_idx == CMD_DO_NOTHING) || (cmd_idx == CMD_RESET) ||
        (cmd_idx == CMD_MORSE_GO) || (cmd_idx == CMD_MORSE_END) ||
        (cmd_idx == CMD_VOLUME_CHG)) {
      message[4] = NO_DATA;
      s2 = "NO_DATA";
    } else {
      message[4] = CMD_IDX_TODO;
      s2 = "CMD_IDX_TODO";
    }
  } else {
    Serial.print(txt0);
    Serial.print(F("Error: tmp_idx out of bounds "));
    Serial.printf("(%d)\n", tmp_idx);
    return;
  }

  message[5] = '\0';  // Optional terminator for debug only
  uint8_t le_message = sizeof(message);

  Serial.print(txt0);
  Serial.printf("I2C_DEV_ADDR=0x%02x, TXpacketNr=%u, cmd=\"%s\", data=\"%s\"\n",
                message[0], ((message[1] << 8) | message[2]),
                s1.c_str(), s2.c_str());

  Serial.print(txt0);
  Serial.print(F("Going to send command message: "));
  for (uint8_t i = 0; i < le_message; i++) {
    Serial.printf("0x%02x%s", message[i], (i < le_message - 1) ? ", " : "\n");
  }

  // setPins already called in function processCommand()
  //setPins(true);  // Ensure I2C mode
  Wire.beginTransmission(I2C_DEV_ADDR);
  size_t bytes_written = Wire.write(message, le_message);
  uint8_t tx_result = Wire.endTransmission(true);

  Serial.print(txt0);
  if (tx_result == 0 && bytes_written == le_message) {
    Serial.print(F("command message sent. Number of bytes queued: "));
    Serial.printf("%d\n", bytes_written);
  } else {
    Serial.print(F("I2C transmission failed (code "));
    Serial.printf("%d)\n", tx_result);
  }
}

void send_speed_chg(int8_t speed_chg_idx) {
  static constexpr const char txt0[] PROGMEM = "send_speed_chg(): ";
  static constexpr const char *txts[] PROGMEM = {
    "sending ",        // 0
    "speed change ",   // 1
    "message",         // 2
    "bytes ",          // 3
    "sent",            // 4
    "Number of ",      // 5
    "Going to ",       // 6
    "send ",           // 7
    "NULL terminator"  // 8
  };

  TXpacketNr++;
  uint8_t message[6];  // Fixed-size buffer on stack

  message[0] = I2C_DEV_ADDR;
  message[1] = (TXpacketNr >> 8) & 0xFF;   // MSB
  message[2] = TXpacketNr & 0xFF;          // LSB
  message[3] = CMD_SPEED_CHG;
  message[4] = speed_chg_idx;
  message[5] = '\0';                       // optional terminator

  uint8_t le_message = sizeof(message);
  Serial.print(txt0);
  Serial.printf("I2C_DEV_ADDR=0x%02x, TXpacketNr=%u, msgType=CMD_SPEED_CHG, data=%d, %s\n",
                message[0], (message[1] << 8) | message[2], message[4], txts[8]);

  Serial.print(txt0);
  Serial.print(F("length of message = "));
  Serial.printf("%d\n", le_message);
  Serial.print(txt0);
  Serial.printf("%s%s%s%s: ", txts[6], txts[7], txts[1], txts[2]);
  for (uint8_t i = 0; i < le_message; i++) {
    Serial.printf("0x%02x%s", message[i], (i < le_message - 1) ? ", " : "\n");
  }

  setPins(); // set I2C pins
  Wire.beginTransmission(I2C_DEV_ADDR);
  size_t bytes_written = Wire.write(message, le_message);
  uint8_t tx_result = Wire.endTransmission(true);

  Serial.print(txt0);
  if (tx_result == 0 && bytes_written == le_message) {
    Serial.print(F("speed change message sent. Number of bytes queued: "));
    Serial.printf("%d\n", bytes_written);
  } else {
    Serial.print(F("I2C transmission failed (code "));
    Serial.printf("%d)\n", tx_result);
  }
}

void handle_rx() {
  uint8_t rx_buffer_idx = 0;
  int value;
#ifdef USE_M5STACK_EXAMPLE
  //Read 40 bytes from the slave
  uint8_t bytesReceived = Wire.requestFrom(I2C_DEV_ADDR, rx_buffer_len);
  Serial.printf("requestFrom: %u\n", bytesReceived);
  if((bool)bytesReceived) {  //If received more than zero bytes
    uint8_t temp[bytesReceived];
    Wire.readBytes(temp, bytesReceived);
    log_print_buf(temp, bytesReceived);
  }
#else
  rx_buffer[0] = '\0';  // empty rx_buffer
  
  while(Wire.available()) {
    value = Wire.read();
    if (value >= 0) {
      rx_buffer[rx_buffer_idx]= static_cast<uint8_t>(value);
      rx_buffer_idx += 1;
    }
    else
      break; // Value < 0 . Usually it will be -1 if no more data available
  }
  if (rx_buffer_idx > 0) {
    if (rx_buffer[0] == 1) {
      ack_rcvd = true;
      // Serial.printf("\'%s\'\n",rx_buffer);
      std::cout << "ACK Received from slave: " << std::endl;
      // I2C_ack(); // Send an acknowledge text to the I2C Slave device
    }
  }
#endif
}

void disp_text(char *txt) {
  if (txt != NULL) {
    M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 28,
    M5Cardputer.Display.width(), 25,
    BLACK);
    M5Cardputer.Display.drawString(txt, 4, M5Cardputer.Display.height() - 24);
  }
 }

bool send(String txt) {
  int le = txt.length();
  if (le > 0) {
    setPins(); // set I2C pins
    Wire.beginTransmission(I2C_DEV_ADDR);  // Start transmission to the slave device
    for (int i = 0; txt[i] != '\0'; i++) {
        Wire.write(txt[i]);             // Sends value byte
    }
    Wire.write('\0'); // Write end-of-string null marker
    uint8_t error = Wire.endTransmission(true);  // Stop transmitting
    Serial.printf("endTransmission: %u\n", error);
  }
}

void cleanup() {
  //freeRxBuffer();
  ack_rcvd = false;
  ctrl_pressed = false;
  decrease_pressed = false;
  increase_pressed = false;
  morse_go_flag = false;
  morse_end_flag = false;
  key_reset_flag = false;
  volume_echo_flag = false;
  receiveFlag = false;
  show_commands_flag = false;
  use_speaker = false;
  speed_idx = speed_default;
}

bool isDataOnlySpaces() {
  return std::all_of(data.begin(), data.end(), [](char c) { return c == ' '; });
}

// Called by handle_kbd_input
void pr_fillin(char ltr_fillin) {
  Serial.print(F("<ctrl> + \'"));
  Serial.print(ltr_fillin);
  Serial.println(F("\' pressed"));
  // Serial.printf("%s%s + \'%c\' %s\n", txts[0], txts[2], ltr_fillin, txts[1]);
}


void handle_kbd_input() {
  static constexpr const char txt0[] PROGMEM = "handle_kbd_input(): ";
  static char lastKey = '\0';
  static unsigned long lastKeyTime = 0;
  const unsigned long debounceDelay = 50; // ms

  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {
    lastActivity = millis(); // reset idle timer
    if (M5Cardputer.Keyboard.isPressed()) {
      beep();
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      if (displaySleeping) {
        M5Cardputer.Display.wakeup();
        displaySleeping = false;
        Serial.println("Display woke up on keypress.");
        return; // consume this keypress
      }
      led_status = LED_YELLOW;
      set_led();

      std::string wordStr(status.word.begin(), status.word.end());
      DEBUG_PRINTF("%sstatus.word = \"%s\"\n", txt0, wordStr.c_str());

      // --- Auto-debounce layer ---
      for (auto i : status.word) {
        unsigned long now = millis();

        // Case 1: timing-based bounce suppression
        if (i == lastKey && (now - lastKeyTime) < debounceDelay) {
          Serial.printf("%sBounce ignored (timing): '%c'\n", txt0, i);
          continue;
        }

        // Case 2: reject consecutive spaces
        if (i == ' ' && data.length() > 0 && data.charAt(data.length() - 1) == ' ') {
          Serial.printf("%sIgnored double space\n", txt0);
          continue;
        }

        // Accept key
        data += i;
        lastKey = i;
        lastKeyTime = now;
      }


      if (data != "> ") {
        if (data.length() >= 2) {
          // Check if data[2] is in cmd_ltrs
          if (std::find(std::begin(cmd_ltrs), std::end(cmd_ltrs), data[2]) == std::end(cmd_ltrs)) {  
            // character typed is not found in the list cmd_ltrs, so: print it.  
            Serial.print(txt0);
            Serial.print(F("data = \""));
            Serial.print(data);
            Serial.println("\"");
          } else {
            // the character typed is found in the list cmd_ltrs.
            Serial.print(txt0);
            pr_fillin(data[2]);
          }
        }
      }

      if (status.ctrl) {
        ctrl_pressed = true;
        Serial.print(txt0);
        Serial.println(F("<ctrl> pressed"));
        if (data.length() > 2)
          data = "> ";  // wipe out what has been typed before
      } else {
        if (status.del) {
          data.remove(data.length() - 1);
        }

        if (status.enter) {
          data.remove(0, 2);
          if (isDataOnlySpaces() == false) { 
            if (data.length() > 0) {
              canvas.println(data);
              canvas.pushSprite(4, 4);
              send_text_msg();  // send the text to the M5Echo
            }
          }
          data = "> ";
        }

        M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 28,
                                     M5Cardputer.Display.width(), 25,
                                     BLACK);

        M5Cardputer.Display.drawString(data, 4,
                                       M5Cardputer.Display.height() - 24);
      }

      bool brk2 = false; 
      for (auto i : status.word) {
        if (ctrl_pressed) {
          switch (i) {
            case SHOW_COMMANDS:
              show_commands_flag = true;
              disp_commands();
              disp_main_screen();
              brk2 = true;
              break;
            case SPEAKER_ON_OFF:
              use_speaker = !use_speaker;
              data = "> ";
              ctrl_pressed = false;
              brk2 = true;
              break;
            case MORSE_GO:
              morse_go_flag = true;
              blinkFeedback(CMD_MORSE_GO);
              break;
            case MORSE_END:
              morse_end_flag = true;
              blinkFeedback(CMD_MORSE_END);
              break;
            case KEY_RESET:
              key_reset_flag = true;
              blinkFeedback(CMD_RESET);
              break;
            case VOLUME_CHG:
              volume_echo_flag = true;
              blinkFeedback(CMD_VOLUME_CHG);
              break;
            case KEY_SPEED_DECR:
              decrease_pressed = true;
              speed_idx -= 1;
              blinkFeedback(CMD_SPEED_CHG);
              DEBUG_PRINT(txt0);
              DEBUG_PRINT(F("speed_idx < SPEED_IDX_MINIMUM ? "));
              DEBUG_PRINTF("%s\n", (speed_idx < SPEED_IDX_MINIMUM) ? "true" : "false");
              if (speed_idx < SPEED_IDX_MINIMUM)
                speed_idx = SPEED_IDX_MINIMUM;
              brk2 = true;
              break;
            case KEY_SPEED_INCR:
              increase_pressed = true;
              speed_idx += 1;
              blinkFeedback(CMD_SPEED_CHG);
              DEBUG_PRINT(txt0);
              DEBUG_PRINT(F("speed_idx >= SPEED_IDX_MAXIMUM ? "));
              DEBUG_PRINTF("%s\n", (speed_idx >= SPEED_IDX_MAXIMUM) ? "true" : "false");
              if (speed_idx >= SPEED_IDX_MAXIMUM)
                speed_idx = SPEED_IDX_MAXIMUM;
              brk2 = true;
              break;
          }
        }
        if (brk2) break;
      }

      if (show_commands_flag || key_reset_flag || morse_go_flag ||
          morse_end_flag || decrease_pressed || increase_pressed || volume_echo_flag) {
        data = "> ";
        ctrl_pressed = false;
        if (show_commands_flag) show_commands_flag = false;
        delay(500); // debounce delay for control keys
      }
      led_status = LED_OFF;
      set_led();
    }
  }
}


void set_led() {
  switch (led_status) {
    case LED_RAINBOW:
        leds[0] = CHSV(led_ih, 255, 255);
        break;
    case LED_RED:
        leds[0] = CRGB::Red;
        break;
    case LED_GREEN:
        leds[0] = CRGB::Green;
        break;
    case LED_YELLOW:
        leds[0] = CRGB::Yellow;
        break;
    case LED_BLUE:
        leds[0] = CRGB::Blue;
        break;
    case LED_CYAN:
        leds[0] = CRGB::Cyan;
        break;
    case LED_WHITE:
        leds[0] = CRGB::White;
        break;
    case LED_OFF:
        leds[0] = CRGB::Black;
        break;
    default:
        leds[0] = CRGB::Black;
        break;
  }
  FastLED.show();
  led_ih++;
  delay(15);
}

void setup() {
  static constexpr const char txt0[] PROGMEM = "setup(): ";
  static constexpr const char txt1[] PROGMEM = "I2C Pins set ";
  static constexpr const char txt2[] PROGMEM = "Going to restart in 5 seconds...";
  static constexpr const char *txts[] PROGMEM = {
    "M5Stack Cardputer v1.0.",        // 0
    "I2C test",                       // 1
    "to send ",                       // 2
    "text for ",                      // 3
    "morse code ",                    // 4
    "and ",                           // 5
    "speed change commands",          // 6
    "to a M5Stack Atom Echo device.", // 7
    "successfull.",                   // 8
    "failed."                         // 9
  };
  auto cfg = M5.config();
  // See: https://github.com/geo-tp/M5-Card-Computer-I2C-Scanner/blob/main/src/main.cpp
  M5Cardputer.begin(cfg, true);
  //M5.begin(cfg); // Initialize the M5
  M5.Power.begin(); // Initialize the M5 power
  
  //using namespace m5;

  /// for external I2C device (Port.A)
  //I2C_Class &Ex_I2C = m5::Ex_I2C;

  Serial.begin(115200); 

  Serial.setDebugOutput(true);
  delay(5000); // Give user time to clear the Serial Monitor output window!
  Serial.println();
  Serial.print(txt0);
  Serial.printf("%s\n%s\n%s%s%s\n%s%s%s\n%s\n", 
    txts[0], 
    txts[1], 
    txts[2], txts[3], txts[4], 
    txts[5], txts[2], txts[6],
    txts[7] );

#ifdef USE_GROVE
  pinMode(GROVE_PIN1, OUTPUT);
  pinMode(GROVE_PIN2, INPUT_PULLUP); // Set the second pin of GROVE B as input (White)

  attachInterrupt(digitalPinToInterrupt(GROVE_PIN2), ack_cb, FALLING);
#endif

  if (!create_rx_buffer()) {
    Serial.print(txt0);
    Serial.println(txt2);
      delay(5000);
      ESP.restart();
  }

  bool result;
  
  auto board = M5Cardputer.Display.getBoard();

  Serial.print(txt0);
  Serial.print(F("Board type: "));
  Serial.println(board);
  
  // Setup the StampS3 LED
  //pinMode(PIN_BUTTON, INPUT);
  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);
  led_status = LED_OFF;
  set_led();
  // M5Cardputer, Port A. See: https://github.com/m5stack/M5Unified/README.md  ESP32S3 GPIO list
  //result = Wire.setPins(2, 1); //GROVE_SDA, GROVE_SCL);
#ifdef USE_DUALBUTTON
  result = setPins(false); // set back to default pins for dual button
  dualbtn_red_pressed = false;
  dualbtn_blue_pressed = false;
#else 
  result setPins(); // set I2C pins
#endif
  Serial.print(txt0);
  Serial.print(txt1);
  if (result == true) 
    Serial.println(txts[8]);
  else 
    Serial.println(txts[9]);
  
  size_t bufSz = Wire.setBufferSize(sizeof(I2C_TEXT_MESSAGE));
  Serial.print(txt0);
  Serial.print(F("setting I2C buffersize to "));
  Serial.print(sizeof(I2C_TEXT_MESSAGE));
  Serial.print(" ");
  if (bufSz == 0) {
    Serial.println(txts[9]);
  } else {
    Serial.println(txts[8]);
  }
  Wire.onReceive(onReceive); 
  Wire.onRequest(onRequest);
  //Wire.begin(GROVE_SDA, GROVE_SCL);
  //result = Wire.begin((uint8_t)I2C_DEV_ADDR);
  //result = Wire.begin();
  //result = Wire.begin(I2C_DEV_ADDR, GROVE_SDA, GROVE_SCL);

  result = Wire.begin();

  if (result == false) {
    Serial.print(txt0);
    Serial.print(F("Can't start I2C on bus nr: "));
    Serial.printf("%d.\n", i2c_bus_num);
    Serial.println(F("Going to restart in 5 seconds..."));
    delay(5000);
    ESP.restart();
  } else {
    Serial.print(txt0);
    Serial.print(F("Successfully connected onto I2C bus nr: "));
    Serial.printf("%d.\n", i2c_bus_num);
  }
  disp_commands(true);
  disp_main_screen();
}

// For a very util source for the Cardputer see:
// https://cardputer.free.nf/class_keyboard___class.html

void loop() {
  bool dualbtn_pressed = false;
  cleanup(); // reset all global flags and settings
  while (true) {
    handle_kbd_input();

    if (key_reset_flag) {
      send_cmd(CMD_RESET);
      key_reset_flag = false;
    }

    if (morse_go_flag) {
      send_cmd(CMD_MORSE_GO);
      morse_go_flag = false;
    }

    if (morse_end_flag) {
      send_cmd(CMD_MORSE_END);
      morse_end_flag = false;
    }

    if (decrease_pressed || increase_pressed) {
      send_speed_chg(speed_idx);
      decrease_pressed = false;
      increase_pressed = false;
    }

    if (volume_echo_flag) {
      send_cmd(CMD_VOLUME_CHG);
      volume_echo_flag = false;
    }
    // Handle feedback timeout
    if (feedbackActive && (millis() - feedbackShownAt > feedbackDuration)) {
      disp_main_screen();   // redraw main screen
      feedbackActive = false;
    }

    // --- Display idle timeout check ---
    if (!displaySleeping && (millis() - lastActivity > idleTimeout)) {
      M5Cardputer.Display.sleep();
      displaySleeping = true;
      Serial.println(F("Display went to sleep due to inactivity."));
    }

#ifdef USE_DUALBUTTON
    setPins(false); // set back to default pins for dual button
    if (!displaySleeping) {
      dualbtn_pressed = ck_dualbutton(); // normal handling when awake
    }
#endif

    // --- Display wakeup check ---
    if (displaySleeping) {
      //dualbtn_pressed = ck_dualbutton(); // check dual button presses
      M5Cardputer.update();  // <-- refresh hardware state

#ifdef USE_DUALBUTTON
      bool wake_btn_pressed = ck_dualbutton(); // check buttons while sleeping
#endif
      if (
#ifdef USE_DUALBUTTON
        wake_btn_pressed ||
#endif
      (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
      ) {
        M5Cardputer.Display.wakeup();
        disp_main_screen(); // redraw main UI
        displaySleeping = false;
        lastActivity = millis();
        Serial.println(F("Display woke up on keypress."));
      }
    }
    // --- LED animation ---
    if (led_status == LED_RAINBOW) {
      set_led();  // advance hue each iteration
      if (millis() - rainbowStart > 5000) { // stop after 5 seconds
        led_status = LED_OFF;
        set_led();
        Serial.println(F("Rainbow ended"));
      }
    }

    // --- LED pulse off check ---
    if (led_status != LED_OFF && led_status != LED_RAINBOW) {
      if (millis() - ledOnAt > 100) { // 100 ms pulse
        led_status = LED_OFF;
        set_led();
      }
    }
  }
}
