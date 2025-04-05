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



// #define USE_M5STACK_EXAMPLE  // see handle_rx()

#ifndef ARDUINO_M5STACK_CARDPUTER
#define ARDUINO_M5STACK_CARDPUTER
#endif

#define I2C_DEV_ADDR    0x55

uint32_t TXpacketNr = 14385; // = 0x3830 (for test in send_speed_chg() )
uint32_t RXpacketNr = 0;


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
#define GROVE_PIN1 26 // Define the first  pin of Port A (Yellow) SDA
#define GROVE_PIN2 32 // Define the second pin of Port A (White ) SCL
#endif

bool I2C_device_found = false;

M5Canvas canvas(&M5Cardputer.Display);

String data = "> ";
size_t rx_bufferSize = 128;
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
int8_t speed_idx = 3;  // default morse speed for M5Echo

unsigned long start_t = millis();
unsigned long curr_t = 0;
unsigned long timeout_limit_t = 2000; // timeout = 2 seconds
unsigned long elapsed_t = 0;

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
  
  // M5Cardputer, Port A. See: https://github.com/m5stack/M5Unified/README.md  ESP32S3 GPIO list
  result = Wire.setPins(2, 1); //GROVE_SDA, GROVE_SCL);
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
    // I2C_ScannerWire();
  }
  disp_main_screen();
}

void wait_for_keypress() {
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      if (M5Cardputer.Keyboard.isPressed())
        return;
    }
  }
}

float dispTextSizeSmall = 0.5;
float dispTextSize      = 1.0;

void disp_main_screen() {
  static constexpr const char *txts[] PROGMEM = {"Input: ", "<Key> +", "<Enter>"};
  M5Cardputer.Display.clear();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(dispTextSize);

  //canvas.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  canvas.setTextFont(&fonts::FreeMonoBold18pt7b);
  canvas.setTextSize(dispTextSize);
  canvas.createSprite(M5Cardputer.Display.width() - 4,
                      M5Cardputer.Display.height() - 4);
  canvas.setTextScroll(true);
  M5Cardputer.Display.drawRect(0, 0, M5Cardputer.Display.width(),
                              M5Cardputer.Display.height() - 28, GREEN);
  //M5Cardputer.Display.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  M5Cardputer.Display.setTextFont(&fonts::FreeMonoBold18pt7b);

  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 4,
                              M5Cardputer.Display.width(), 4, GREEN);

  //canvas.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  canvas.setTextFont(&fonts::FreeMonoBold18pt7b);
  //canvas.setTextSize(0.5);
  canvas.createSprite(M5Cardputer.Display.width() - 8,
                      M5Cardputer.Display.height() - 36);
  //canvas.setTextScroll(true);
  //canvas.println(txt1);
  canvas.pushSprite(4, 4);
  M5Cardputer.Display.drawString(txts[0], 4,  4);
  M5Cardputer.Display.drawString(txts[1], 4, 34);
  M5Cardputer.Display.drawString(txts[2], 4, 64);

  data = "> ";
  M5Cardputer.Display.drawString(data, 4, M5Cardputer.Display.height() - 24);

}

void disp_commands() {
  M5Cardputer.Display.clear();
  //canvas.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  canvas.setTextFont(&fonts::FreeMonoBold18pt7b);
  canvas.setTextSize(dispTextSizeSmall);
  canvas.createSprite(M5Cardputer.Display.width() - 4,
                      M5Cardputer.Display.height() - 4);
  canvas.setTextScroll(true);
  canvas.println("--- COMMANDS: CTRL+ ---");
  canvas.println(" i  incr speed");
  canvas.println(" d  decr speed");
  //canvas.pushSprite(4, 4);
  //wait_for_keypress();
  canvas.println(" r  reset device");
  canvas.println(" s  snd on/off");
  canvas.println(" g  morse go");
  canvas.println(" e  morse end");
  canvas.pushSprite(4, 4);
  wait_for_keypress();
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
  message[4 + le_data +1] = '\0'; // end of string marker
  le_packet++;

  le_message=sizeof(message)/sizeof(message[0]);
  
  String s;
  if (message[3] == TEXT_MESSAGE)
    s = "TEXT_MESSAGE";
  else if (message[3] == SPEED_CHG)
    s = "SPEED_CHG";

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
  for (i = 0; i < le_packet; i++) {
    Serial.printf("0x%02x ", message[i]);
  }
  Serial.println();
  
  size_t bytes_sent = 0;
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

 void send_cmd(int8_t cmd_idx) {
  static constexpr const char txt0[] PROGMEM = "send_cmd(): ";
  static constexpr const char *txts[] PROGMEM = {
    "sending ",       // 0
    "command ",       // 1
    "message",        // 2
    "bytes ",         // 3
    "sent",           // 4
    "Number of ",     // 5
    "Going to ",      // 6
    "send "           // 7
  };
  TXpacketNr++;
  uint8_t le_cmd_msg;
  uint8_t le_message;
  uint8_t *message;
  uint8_t message_bufferSize = 6;

  message = NULL;
  message = (uint8_t *)malloc(message_bufferSize);
  if (message == NULL) {
    Serial.print(txt0);
    Serial.println(F("Can't allocate memory for cmd msg"));
    return;
  }

  message[0] = I2C_DEV_ADDR;
  message[1] = (TXpacketNr & 0xFF00) >> 8; // put MSB of uint16_t
  message[2] = TXpacketNr & 0xFF; // put LSB of uint16_t
  message[3] = CMD_MESSAGE;
  message[4] = cmd_idx;
  message[5] = '\0';
  le_message=6; // was: =sizeof(message)/sizeof(message[0]) + 1;

  String s1;
  switch (message[3]) {
    case CMD_MESSAGE:
      s1 = "CMD_MESSAGE";
      break;
    case TEXT_MESSAGE:
      s1 = "TEXT_MESSAGE";
      break;
    case SPEED_CHG:
      s1 = "SPEED_CHG";
      break;
  }

  String s2;
  switch (message[4]) {
    case CMD_DO_NOTHING:
      s2 = "CMD_DO_NOTHING";
      break;
    case CMD_RESET:
      s2 = "CMD_RESET";
      break;
    case CMD_MORSE_GO:
      s2 = "CMD_MORSE_GO";
      break;
    case CMD_MORSE_END:
      s2 = "CMD_MORSE_END";
      break;
  }
  
  Serial.print(txt0);
  Serial.printf("I2C_DEV_ADDR = 0x%02x, TXpacketNr = %u, msgType = %s, cmd = %s\n", 
              message[0], 
              (message[1] << 8) | message[2], 
              s1,
              s2);

  Serial.print(txt0);
  Serial.print(F("length of message = "));
  Serial.printf("%d\n", le_message);

  Serial.print(txt0);
  Serial.printf("%s%s%s%s: ", txts[6], txts[7], txts[1], txts[2]);
  uint8_t i; 
  for (i = 0; message[i] != '\0'; i++) {
    Serial.printf("0x%02x", message[i]);
    if (i < le_message-1)
      Serial.print(", ");
  }
  Serial.printf("0x%02x\n", message[i]);  // Print the NULL terminator
  size_t bytes_sent = 0;
  Wire.beginTransmission(I2C_DEV_ADDR);
  bytes_sent = Wire.write((uint8_t *)message, le_message);
  Wire.endTransmission(true);
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
  free(message);
}

void send_speed_chg(int8_t speed_idx) {
  static constexpr const char txt0[] PROGMEM = "send_speed_chg(): ";
  static constexpr const char *txts[] PROGMEM = {
    "sending ",       // 0
    "speed change ",  // 1
    "message",        // 2
    "bytes ",         // 3
    "sent",           // 4
    "Number of ",     // 5
    "Going to ",      // 6
    "send "           // 7
  };
  TXpacketNr++;
  uint8_t le_speed_msg;
  uint8_t le_message;
  uint8_t *message;
  uint8_t message_bufferSize = 6;

  message = NULL;
  message = (uint8_t *)malloc(message_bufferSize);
  if (message == NULL) {
    Serial.print(txt0);
    Serial.println(F("Can't allocate memory for speed chg msg"));
    return;
  }

  message[0] = I2C_DEV_ADDR;
  message[1] = (TXpacketNr & 0xFF00) >> 8; // put MSB of uint16_t
  message[2] = TXpacketNr & 0xFF; // put LSB of uint16_t
  message[3] = SPEED_CHG;
  message[4] = speed_idx;
  message[5] = '\0';
  le_message=6; // was: =sizeof(message)/sizeof(message[0]) + 1;

  String s;
  if (message[3] == TEXT_MESSAGE)
    s = "TEXT_MESSAGE";
  else if (message[3] == SPEED_CHG)
    s = "SPEED_CHG";

  Serial.print(txt0);
  Serial.printf("I2C_DEV_ADDR = 0x%02x, TXpacketNr = %u, msgType = %s, speed_idx = %d\n", 
              message[0], 
              (message[1] << 8) | message[2], 
              s,
              message[4]);

  Serial.print(txt0);
  Serial.print(F("length of message = "));
  Serial.printf("%d\n", le_message);

  Serial.print(txt0);
  Serial.printf("%s%s%s%s: ", txts[6], txts[7], txts[1], txts[2]);
  uint8_t i;
  for (i = 0; message[i] != '\0'; i++) {
    Serial.printf("0x%02x", message[i]);
    if (i < le_message-1)
      Serial.print(", ");
  }
  Serial.printf("0x%02x\n", message[i]);  // Print the NULL terminator
  size_t bytes_sent = 0;
  Wire.beginTransmission(I2C_DEV_ADDR);
  bytes_sent = Wire.write((uint8_t *)message, le_message);
  Wire.endTransmission(true);
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
  free(message);
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
  receiveFlag = false;
  show_commands_flag = false;
  use_speaker = false;
  speed_idx = 3;
}

bool isDataOnlySpaces() {
  return std::all_of(data.begin(), data.end(), [](char c) { return c == ' '; });
}

void handle_kbd_input() {
  static constexpr const char txt0[] PROGMEM = "handle_kbd_input(): ";
  static constexpr const char *txts[] PROGMEM = {
    "<ctrl> ",   // 0
    "key ",      // 1
    "pressed",   // 2
    "active"     // 3
  };

  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      beep();
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

      for (auto i : status.word) {
        data += i;
      }
      if ( data != "> ") {
        Serial.print(txt0);
        Serial.print(F("data = "));
        Serial.println(data);
      }

      if (status.ctrl) {
        ctrl_pressed = true;
        Serial.print(txt0);
        Serial.printf("%s%s\n", txts[0], txts[2]);
      }
      else {
        if (status.del) {
          data.remove(data.length() - 1);
        }

        if (status.enter) {
          data.remove(0, 2);
          // check if data does contain only spaces
          // If so, do not send.
          if (isDataOnlySpaces() == false) { 
            if (data.length() > 0) {
              canvas.println(data);
              canvas.pushSprite(4, 4);
              send_text_msg();  // send the text to the M5Echo
              //send(data);
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

      for (auto i : status.word) {
        if (ctrl_pressed) {
          if (i == SHOW_COMMANDS) {
            show_commands_flag = true;
            disp_commands();
            disp_main_screen();
            break;
          }
          if (i == KEY_FLIP_SPEAKER) {
              use_speaker = !use_speaker; // flip the flag
              data = "> ";
              ctrl_pressed = false;
              break;
          }
          if (i == MORSE_GO) {
            morse_go_flag = true;
            Serial.print(txt0);
            Serial.printf("%s%s + \'g\' %s\n", txts[0], txts[3], txts[2]);
          }
          if (i == MORSE_END) {
            morse_end_flag = true;
            Serial.print(txt0);
            Serial.printf("%s%s + \'e\' %s\n", txts[0], txts[3], txts[2]);
          }
          if (i == KEY_RESET) {
            key_reset_flag = true;
            Serial.print(txt0);
            Serial.printf("%s%s + \'r\' %s\n", txts[0], txts[3], txts[2]);
          }

          if (i == KEY_SPEED_DECR) {
            Serial.print(txt0);
            Serial.printf("%s%s + \'d\' %s\n", txts[0], txts[3], txts[2]);
            decrease_pressed = true;
            speed_idx -= 1;
            if (my_debug) {
              Serial.print(txt0);
              Serial.print(F("speed_idx < SPEED_IDX_MINIMUM ? "));
              Serial.printf("%s\n", (speed_idx < SPEED_IDX_MINIMUM) ? "true" : "false");
            }
            if (speed_idx < SPEED_IDX_MINIMUM)
              speed_idx = SPEED_IDX_MINIMUM;
            break;
          }
          if (i == KEY_SPEED_INCR) {
            Serial.print(txt0);
            Serial.printf("%s%s + \'i\' %s\n", txts[0], txts[3], txts[2]);
            increase_pressed = true;
            speed_idx += 1;
            if (my_debug) {
              Serial.print(txt0);
              Serial.print(F("speed_idx >= SPEED_IDX_MAXIMUM ? "));
              Serial.printf("%s\n", (speed_idx >= SPEED_IDX_MAXIMUM) ? "true" : "false");
            }
            if (speed_idx >= SPEED_IDX_MAXIMUM)
              speed_idx = SPEED_IDX_MAXIMUM;
            break;
          }
        }
      }  // end-of-for loop
      if (show_commands_flag || key_reset_flag || morse_go_flag || morse_end_flag || decrease_pressed || increase_pressed) {
        data = "> ";
        ctrl_pressed = false; // reset flag
        if (show_commands_flag)
          show_commands_flag = false;
        delay(500); // debounce delay
      }
    }
  }
}


void I2C_ScannerWire() {
  static constexpr const char txt0[] PROGMEM = "I2C_ScannerWire(): ";
  byte error;
  uint8_t address;
  uint8_t nDevices;

  Serial.print(txt0);
  Serial.println(F("Scanning for I2C devices..."));

  nDevices = 0;
  for(address = 0x0; address < 0x7F; address++ )
  {
    Serial.print(txt0);
    Serial.print(F("scanning address: "));
    Serial.printf("0x%02x\n", address);
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print(F("I2C device found at address: "));
      Serial.printf("0x%02x", address);
      Serial.println(" !");
      I2C_device_found = true;
      nDevices++;
      break;
    }
    /*
    else if (error==4)
    {
      Serial.print("Unknown error at address: ");
      Serial.printf("0x%02x\n", address);
    }
    */   
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}


/*
void I2C_ScannerWire1()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    Wire1.beginTransmission(address);
    error = Wire1.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error==4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}
*/

// For a very util source for the Cardputer see:
// https://cardputer.free.nf/class_keyboard___class.html
void loop() {
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
      if (decrease_pressed)
        decrease_pressed = false; // reset
      if (increase_pressed)
        increase_pressed = false; // reset
    }
  }
}

