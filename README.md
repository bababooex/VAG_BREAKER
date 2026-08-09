# **VAG_BREAKER**
Experimental rewrite of already existing rolling code implementation on flipper zero. Developed as helpful tool to analyse and research rolling codes with cheap device like arduino, avoiding high cost of Flipperzero / HackRF, intended for educational and security purposes only. It doesnt allow user to transmit by default, this can be enabled in example code directly, preventing accidental desync or misuse right away. This was made to warn and teach about security vulnerabilities in my home country, where most popular car brand is Skoda!

> [!WARNING]
> Although the name sounds ilegally, it means breaking rolling code, not into someones car. Also I have not tested encode on real car, but it is being decoded with flipper zero correctly with counter + 1. Test this only on systems and vehicles you own or have explicit, written permission to test. Capturing, decoding, or transmitting keyfob and vehicle-access signals without authorization may be illegal in your jurisdiction (e.g. computer misuse, unauthorized access, or radio regulations). You are solely responsible for ensuring your use complies with all applicable laws. The authors and contributors assume no liability for misuse or damage arising from use of this software.

## **Supported VAG variants**
| Type | Decoder | Encoder | Crypto used | Car examples  |
| :---:| :---:   | :---:   |    :---:    |     :---:     | 
|  1   |   ✅    |   ❌   | AUT64       | VW Passat     | 
|  2   |   ✅    |   ✅   | XTEA        | Skoda rapid   |
|  3   |   ✅    |   ✅   | AUT64       | Audi          |
|  4   |   ✅    |   ✅   | AUT64       | Skoda octavia | 

All use same 434.42 MHz. Examples are from my testing, can be wrong. 
The x only means that it is encoding incorrectly, most Passats decode as type 1 but encode as type 2, probably missing key for encode type 1.
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
- Screens switch from main -> advanced 1 -> advanced 2 and back to main, main is default screen and has most important info
  
This table explains different data:

| Screen type | Purpose and data meaning |
|---|---|
| Main screen | Shows decoded type name, button, counter and key used to decrypt, if vag type detected is 2, it shows X - no AUT64 keys used |
| Advanced screen 1 | Shows serial number, dispatch byte and type byte, these are more advanced data derived from what is shown on last screen |
| Advanced screen 2 | Shows full key1 and key2 data directly, for full debug via LCD |

### Navigation
Default example uses 4 buttons
This table explains their purpose, last three are just for that keyfob pressing feel:

| Button name | Purpose and usage |
|---|---|
| RST_BTN | Handles main navigation. Short press changes screen, long restarts capture  |
| LOCK_BTN | Encodes lock button with next rolling code and counter |
| UNLOCK_BTN | Encodes unlock button with next rolling code and counter |
| TRUNK_BTN | Encodes boot button with next rolling code and counter |

## **Images**

## **Crypto research**
This explains more how does underlying logic work
- **Lock It and Still Lose It — On the (In)Security of Automotive Remote Keyless Entry Systems**
  Flavio D. Garcia, David Oswald, Timo Kasper, Pierre Pavlidès
  *USENIX Security 2016, pp. 929–944*
  DOI: [10.5555/3241094.3241166](https://doi.org/10.5555/3241094.3241166)
  https://www.usenix.org/system/files/conference/usenixsecurity16/sec16_paper_garcia.pdf
  
## **Credits**
- Ported to arduino by me **@bababooex**
- Additional help from AI, specifically DeepSeek
### **Inspiration and original implementations**
Originally developed by [ProtoPirate](https://protopirate.net/ProtoPirate/ProtoPirate) crew, for testing code I used [FlipperARF](https://github.com/D4C1-Labs/Flipper-ARF) with my flipper zero. But most of my rewrites are based on rust code from [KAT](https://github.com/KaraZajac/KAT), because it is closest to arduino and doesnt confuse me with dynamic allocation like flipper code.
So thanks to
- **KaraZajac** for her software
- **Proto pirate** and **FlipperARF** guys for their work

