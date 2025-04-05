M5Cardputer_and_M5Echo_I2C_text_to_morse_code

INTRO - A BIT OF HISTORY:

In 1962 I was teached the morse code at the signals school of the Royal Dutch Navy in Amsterdam, The Netherlands. During nine months I learned to send and to receive (and decode in my head) morse code and type the received text on a typewriter ("in the blind", that is: not looking on your hands while typing). Sending the morse code using a heavy duty copper "straight key" was done by the so-called "count method" (in Dutch: "tel methode"). The teacher had a wooden stick in one of his hands. He hit the stick on a wooden block mounted on the table in front of him, in the rythm that we, students "sang" the counting.
And we, the student wireless radio operators, in English also known as "sparks" (named after one of the first type of wireless radio transmitter [wiki](https://en.wikipedia.org/wiki/Spark-gap_transmitter)), had to speak loudly the counting of the 
dots, the dashes, the spaces between them and the spaces between the characters of a word and the spaces between the words.
It went like this: (7 counts in front, to start with) 1234567, "1 and 123 and 2 and 223" for ". --- ---" (= letter W).
As we grew in handling the heavy morse key and speed increased, we arrived at a speed at which the counting was not possible
anymore. From then on we sang the dots and dashes like: "dit dah dah" (= letter W). And in a classroom with 24 students,
one heard 24 brass straight keys pounding in the same rythm. 
Now, 63 years later, the profession of wireless operator doesn't exist anymore. Since the arrival of satellite communication,
the sparks disappeared from the military navy ships as well as from the merchant navy ships. However, morse code is still in use by radio-hobbyists, known as: "hamradio operators". I am one of them. For more info see: [morse code](https://en.wikipedia.org/wiki/Morse_code).
Since a decade I also experiment with microcontrollers. Recently, I came to the idea to create a small project in which a microcontroller generates morse code. 

PURPOSE:

Use an M5Stack M5Cardputer to create and send I2C messages (packets) to an M5Stack M5Echo.
The M5Echo after interprating the received message, in the case of reception of a text message,
will translate the received text into morse code and make the code audible through the builtin loudspeaker.
This will continue during one minute.

VERSIONS:

There are two version of this software.
```
  a) an Arduino (C++) sketch for the master device (in my case the M5Cardputer);
  b) an Arduino (C++) sketch for the slave device (in my case the M5Echo).

```
In the ```src``` folder are two subfolders: "master", containing the software to be flashed to the master device,
"slave", containing the software to be flashed to the slave device.

Hardware used:

```
  1) M5Stack M5Cardputer;
  2) M5Stack M5Echo;
  3) eventually a grove hub with at least 3 connectors;
  4) at least one grove wire to connect port A of the M5Cardputer with Port A of the M5Echo.
```

I2C Communication:

In this moment the I2C communication is one-way: from the I2C master to the I2C slave, Using the hexadecimal address 0x55
towards the slave device. (And inherent to the I2C protocol the I2C module (Wire.h / Wire.cpp), the slave will send ACK
impulses back to the master).

There exist three types of messages (msgType) initiated by the master device:

```
   a) command message (CMD_MESSAGE);
   b) text message (TEXT_MESSAGE);
   c) morse speed change message (SPEED_CHG).

``` 
Upon reception the I2C slave device, the M5Echo, will be check for correct addressing, msgType and contents. In the case of a text message, the M5Echo will translate the received text into morse code, then produces the
morse code audio through its loudspeaker. In this moment there are four types of commands for the M5Echo:

```
  d) CMD_DO_NOTHING; (in this moment serving as a base value (200) to calculate an index to the list of commands);
  e) CMD_RESET (if necessary this command can be used to instruct the slave to execute a software reset (ESP.restart())).
  f) CMD_MORSE_GO (to instruct the M5Echo to start sending a default text "paris ", in morse code used to measure the speed of the morse code);
  g) CMD_MORSE_END (to instruct the M5Echo to end an ongoing sending of morse code. This can be in the case of the default text "paris " or
     another text received from the master device).
```

OTHER COMMANDS:

  Beside the command message for the slave device, there is are two commands used for the master device.
  
  All commands are of the following key combination: "\<ctrl\> + \<key\>"  :
  
  LIST OF KEY COMMANDS
  
```
  +--- commands: ctrl+ ---+
  | c | show commands     |  (show this list)
  +---+-------------------+
  | i | increase speed    |  (morse speed)
  +---+-------------------+
  | d | decrease speed    |  (same)
  +---+-------------------+
  | r | reset device      |  (command to do a software reset on the slave device)
  +---+-------------------+
  | s | sound on/off      |  (speaker of the M5Cardputer (which gives a "beep" upon a keypress))
  +---+-------------------+
  | g | morse go          |  (start sending default text "paris " in morse code)
  +---+-------------------+
  | e | mose end          |  (stop any ongoing sending of morse code)
  +---+-------------------+

```
DEFINITIONS:
  
Upon start (or reset) the Arduino sketch running on either the master or the slave device
will load certain definitions from the #include file: "puter_echo.h" which is present in the 
subfolers "master" and "slave".

MESSAGE CONSTRUCTION : 

h) in the case of a text message:

```
<7-bit I2C address> <32-bit packetNr MSB> <32-bit packetNr LSB> <MsgType> <Text> <NULL-terminator>

```

i) in the case of a command message:

```
<7-bit I2C address> <32-bit packetNr MSB> <32-bit packetNr LSB> <MsgType> <Cmd> <NULL-terminator>

```

SETTINGS:

In the file "puter_echo.h", at the top there is a setting for the boolean flag "my_debug". It is set to "false".
If you change this flag into "true", during runtime there will be printed more information to the Serial Monitor output.



THE MASTER DEVICE

After a reset ("Btn Rst" on the back of the M5Cardputer device, the device will show in the display
the text

```
 "Input:
  <Key> +
  <Enter>"
```
and on the bottom line there will appear the prompt: "> ".
Now the sketch is waiting for your input:
In case of a text: type the text. End it with a space character and confirm your entry by hitting the \<Enter\> key.
In case of a command: first press the "ctrl" button (down-left) followed by a letter, one of the letters shown in
the list of COMMANDS above ({c, i, d, r, s, g, e}). After pressing these two keys, one after the other, the sketch of the Cardputer
will interprete and handle the command given. If it is a command for the M5Echo the M5Cardputer will immediately
send a command type of message, containing the command you entered, to the M5Echo. My experience is that this goes
very rapid. On the oscillographs one can see that an I2C command type of message only contains 6 bytes.
Sending this command message packet takes only 13 x 50 = 650 microseconds! (see the oscillographs)

After a reset the following text will be shown on the Serial Monitor output of the M5Cardputer:

```
setup(): M5Stack Cardputer v1.0.
I2C test
to send text for morse code 
and to send speed change commands
to a M5Stack Atom Echo device.
setup(): Board type: 14
setup(): I2C Pins set successfull.
setup(): setting I2C buffersize to 42 successfull.
setup(): Successfully connected onto I2C bus nr: 0.

```

THE SLAVE DEVICE

After a reset the following text will be shown on the Serial Monitor output of the M5Echo:

```
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 188777542, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:1184
load:0x40078000,len:13260
load:0x40080400,len:3028
entry 0x400805e4
M5Ato
M5Stack M5Atom Echo
I2C test
to receive text for morse code 
and to receive speed change commands
from a M5Stack Cardputer device.

setup(): Board type: M5Atom Echo
setup(): I2C Pins set successfull.
setting I2C buffersize to 42 successfull.
setup(): Successfully connected onto I2C bus nr: 0.
E (4102) I2S: i2s_driver_uninstall(2048): I2S port 0 has not installed

loop(): entering the while loop...
loop(): ESP.getFreeHeap = 302588

```

The sketch uses two buffers of 128 bytes each in memory.
The first buffer is "rx_buffer". This buffer receives the data from the I2C connection.
The second buffer is "cw_buffer". After every reception of a message packet via I2C, this buffer 
will receive a copy of the contents of the rx_buffer. Why? I have chosen for this option because,
when the M5Echo is sending the morse code text, at every loop of inside the function send_morse(),
the I2C will be polled. If there are bytes available for reception, the rx_buffer will be filled
with the new message packet. This new message can be a CMD_MESSAGE msgType. In that case the 
sketch (after the I2C polling returning into the send_morse() function, will execute the command
received, for instance: CMD_MORSE_END. In this case the ongoing sending of morse code will be
ended). When othere types of messages are received they will be ignored in that moment. The
sending of the morse code will continue, using the contents of the cw_buffer (because the rx_buffer meanwhile
has been filled with data from a new message).

The sketch for the slave contains a table for the conversion from ASCII to code to send morse dots and dashes.

```
std::unordered_map<char, std::vector<int>> morse_txt_dict = {
  //                     decimal:
  {'\"', {1,2,1,1,2,1}}, // 34 <">
  {',', {2,2,1,1,2,2}},  // 44
  {'.', {1,2,1,2,1,2}},  // 46
  {'/', {2,1,1,2,1}},    // 47
  {'0', {2,2,2,2,2}},    // 48       = 30 HEX
  {'1', {1,2,2,2,2}},    // 49
  {'2', {1,1,2,2,2}},    // 50
  [...]
  {'v', {1,1,1,2}},      // 118
  {'w', {1,2,2}},        // 119
  {'x', {2,1,1,2}},      // 120
  {'y', {2,1,2,2}},      // 121
  {'z', {2,2,1,1}},      // 122
};

```

In this table the array after each character value, for exaple in:

```
     {'1', {1,2,2,2,2}}, a "1" represents a morse code "dot" and a "2" represents a "dash". 

```

Here an example of Serial Monitor output when the default text "paris " is being sent in morse code audio
to the speaker of the M5Echo:

```
handle_rx(): RXpacketNr: 14387
handle_rx(): type of message: CMD_MESSAGE
handle_rx(): type of command: CMD_MORSE_GO ('paris')
send_morse(): Starting...
tone dot (and dash) frequency = 1200 Hz
tone_dot.modal  = true
tone_dash.modal = true
dot_dash_time(): tone_dot.time_ms  set to: 100 mSeconds
                 tone_dash.time_ms set to: 300 mSeconds
send_morse(): going to send 'paris ', length = 6
send_morse(): contents cw_buffer = "paris "
Values for dly1, dly3 and dly7:
dly1: 50, dly3: 150, dly7: 350 mSeconds
 1) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 2) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 3) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 4) .|1|---|1|---|1|.| 3 |
send_morse(): Button was pressed. Exiting function.
loop(): Button was pressed

```
Note that in the lines 1)...3) the decimal "." point represents a morse code "dot".
A "-" tack character represents a morse code "dash".
The markings "|1|", "| 3 |" and "|   7   |" represent time spacings:
- |1| representing dot-dash space (in my sketch referred to "unit_delay" or (variable) dly1 );
- | 3 | representing character space ("character_delay", or (variable) dly3);
- |   7   |" representing a word space ("character_delay", or (variable) dly7).

End of texts: it is necessary to remember that when preparing (typing) a text on the master device, 
always end the text with a "space character" (|__|) (key on the right, below the Enter key), 
like in the default "paris ". The sketch will also add an end-of-message/text marker: '\0'. 
The sketch of the slave uses various for (...) loops which check for the '\0' NULL terminator.

In the case, during the sending of the default text "paris ", when a command message containing the command
"CMD_MORSE_END", the follwing will be visual in the Serial Monitor output:

```
send_morse(): going to send 'paris ', length = 6
send_morse(): contents cw_buffer = "paris "
Values for dly1, dly3 and dly7:
dly1: 50, dly3: 150, dly7: 350 mSeconds
 1) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 2) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 3) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |.|1|.|1|.|  7  |

 4) .|1|---|1|---|1|.| 3 |.|1|---| 3 |.|1|---|1|.| 3 |.|1|.| 3 |
poll_I2C(): Nr of bytes available in msg via Wire (I2C) 6
handle_rx(): received nr of bytes: 6
handle_rx(): rcvd correct destination address: 0x55
handle_rx(): RXpacketNr: 14387
handle_rx(): type of message: CMD_MESSAGE
handle_rx(): type of command: CMD_MORSE_END

send_morse(): CMD morse end recived. Exiting function.
```


FLASHING NOTE

If you use the Arduino IDE v2 take note of my recent experience. A few days ago, when starting up the Arduino IDE v2.3.4,
it presented me a notice that there was an upgrade for the IDE to version v2.3.5 available. I accepted to install the upgrade.
After realisation of this upgrade, I tried to flash the M5Echo with the slave sketch. The flashing went OK, however after reset 
the M5Echo kept looping a software reset. "Panic...". In the crash message I saw the word "DIO". In the IDE I checked the option:
"Arduino > Tools > Flash Mode". I saw that this option was default set for "QIO 80MHz". I changed this option to "DIO 80MHz".
After I flashed the sketch again to the M5Echo, the sketch ran flawlessly.

DESCRIPTION DEFAULT MORSE SPEED TEST ("paris ")

During one minute the word "paris " repeatedly will be send in morse code.
At the end of the test, the number of times the word "paris " was sent will be printed.
This value represents the speed in morse code (words per minute).

In the sketch there are three global variables that determine the speed and the duration of 
the morse code being send. The variable ```dly1``` (unit delay) is created
and set in line 96. The values of ```dly3``` (character delay) and ```dly7``` (word delay)
are derived from variable dly1 as: dly3 = 3 * dly1 and dly7 = 7 * dly1. 
The value of dly1 determines the speed the morse code will be send. 
The morse speed to send can be varied between 10 and 54 words per minute. 
In lines 105-107 of the Arduino sketch is a table that shows 
the relation between the value of dly1 and the morse speed being send.
For example: the default value of dly1 is 50 (milliSeconds). With this value 
the morse speed will be 19 words per minute (wpm). The value of dly1 is derived from the
setting of the global variable: tone_dot.time_ms as: ```dly1 = tone_dot.time_ms / 2```.
And the global variable: ```tone_dash.time_ms``` is derived from the value of ```tone_dot.time_ms``` as:
```tone_dash.time_ms = 3 * tone_dot.time_ms```. (see the function: ```set_speed()```)


```

Docs:

```
Text files containing Arduino Monitor Output texts.


Images: 

Images, are in the folder: ```images```.
This folder contains images of the hardware setup and images of I2C traffic "catched" with an oscilloscope.
I added comments/explanation to some of the oscilloscope graphs.


Links to product pages of the hardware used:

- M5Stack M5Cardputer [info](https://shop.m5stack.com/products/atom-echo-smart-speaker-dev-kit?variant=34577853415588);
- M5Stack M5Echo [info](https://shop.m5stack.com/products/atom-echo-smart-speaker-dev-kit?variant=34577853415588) or:
- M5Stack M5Echo (seller in Portugal) [info](https://mauser.pt/catalog/product_info.php?products_id=096-8697);
- M5Stack Grove hub [info](https://shop.m5stack.com/products/mini-hub-module)

Links to product accessories of the hardwar used:
- Seeed studio Grove I2C Hub [info](https://www.seeedstudio.com/Grove-I2C-Hub.html);
- Seeed studio Grove Universal 4 Pin Buckled 20cm cable (5 Pcs Pack)
  [info](https://www.seeedstudio.com/Grove-Universal-4-Pin-Buckled-20cm-Cable-5-PCs-pack.html);
- Seeed studio Grove 4 pin Mal jumper to Grove 4 pin Conversion cable (5 Pcs Pack)
  [info](https://www.seeedstudio.com/Grove-4-pin-Male-Jumper-to-Grove-4-pin-Conversion-Cable-5-PCs-per-Pack.html)


Known Issues:

The current software on the master and the slave devices runs OK. ToDo: investigate how the audio impulses can be changed in such a way that the
resulting audio of the morse code does appear less "staccato" (rigid).
In the morse code test version I experienced that setting ```tone_dot.time_ms``` to uneven decimal values, for example: 90, 110, 130,
caused the tone for the dash to sound garbled. Also the tone of the ```push button``` on the M5Echo sounded garbled alike. That is why I
created an global array of integer values: ```const int tone_time_lst[] = {10, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200};``` which 
will be used in function ```set_speed()``` to change the value of  ```tone_dot.time_ms```, 
from which are derived the values of other global variables (see the explanation about morse speed test above).


