# **VAG_BREAKER**
Experimental rewrite of already existing rolling code implementation on flipper zero. Developed as helpful tool to analyse and research rolling codes with cheap device like arduino, avoiding high cost of Flipperzero / HackRF, intended for educational and security purposes only. It doesnt allow user to transmit by default, this can be enabled in example code directly. This prevents accidental desync or misuse right away. This was made to warn and teach about security vulnerabilities in my home country, where most popular car brand is Skoda!

## **Supported VAG variants**
| Type | Decoder | Encoder | Crypto used | Car examples  |
| :---:| :---:   | :---:   |    :---:    |     :---:     | 
|  1   |   ✅    |   ❌   | AUT64       | VW Passat     | 
|  2   |   ✅    |   ✅   | XTEA        | Skoda rapid   |
|  3   |   ✅    |   ✅   | AUT64       | Audi          |
|  4   |   ✅    |   ✅   | AUT64       | Skoda octavia | 

All use same 434.42 MHz. Examples are from my testing, can be wrong. The x only means that it is encoding incorrectly, most Passats decode as type 1 but encode as type 2.
## **How to put everything together?**

### Required hw for default example
- Arduino (nano)
- CC1101 module
- 4 buttons
- 16x2 lcd display
- 4 channel level shifter
### Connections
- Buttons connected to common ground and default pins in the code:  RST_BTN 5, LOCK_BTN 6, TRUNK_BTN 4, UNLOCK_BTN 7
- LCD connected to analog pins acting as digital: rs = A5, en = A4, d4 = A3, d5 = A2, d6 = A1, d7 = A0
- CC1101 SPI pins except MISO with GDO0 go through 3V3 level shifter, MISO is ok directly to pin 12, provide power to 3V3 side with arduino 3V3 pin or use external ams1117 3.3 like I did.
Then hope the example code will not lag on cc1101 config or be stuck on rssi and you are done!

## **Usage and navigation**


### Screen info 
Program gives you visual clues on LCD itself, you can enable debug in code to diagnose over serial. 
- Firstly it configs cc1101 to raw ask capture, after everything is ok, it waits for signal
- After one of VAG types is detected it tries to decode and then prints info about capture
This table explains different data: 
| Screen type | Purpose and data meaning |
|---|---|
| Main screen | Shows decoded type name, button, counter and key used to decrypt, if vag type detected is 2, it shows X - no AUT64 keys used |
| Advanced screen 1 | Shows serial number, dispatch byte and type byte, these are more advanced data derived from what is shown on last screen  |
| Advanced screen 2 | Shows full key1 and key2 data directly, for full debug via LCD |

### Navigation
Default example uses 4 buttons
This table explains their purpose, last three are just for that keyfob pressing feel:
| Button name | Purpose and usage |
|---|---|
| RST_BTN | Handles main navigation. If you capture correct data, you either switch 3 screens with short presses or one long press restarts capture  |
| LOCK_BTN | Encodes lock button with next rolling code and counter |
| UNLOCK_BTN | Encodes unlock button with next rolling code and counter |
| TRUNK_BTN | Encodes boot button with next rolling code and counter |

## **Images**

## **Credits**
### **Inspiration and original implementations**
- RocketGod
- MMX
- Leeroy
- gullradriel
- Skorp - Thanks, I sneaked a lot from Weather App!
- Vadim's Radio Driver

### **Protocol Magic**

- L0rdDiakon
- YougZ
- RocketGod
- MMX
- DoobTheGoober
- Skorp
- Slackware
- Trikk
- Wootini
- Li0ard
- Leeroy
- Ash

### **Reverse Engineering Support**

- DoobTheGoober
- MMX
- NeedNotApply
- RocketGod
- Slackware
- Trikk
- Li0ard
