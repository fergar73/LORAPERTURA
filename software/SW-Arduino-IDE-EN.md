## Build on Arduino IDE
You can install the Arduino IDE by downloading it from [arduino.cc](https://www.arduino.cc). This project has been developed using 1.8.13 release. 

### Installing dependencies
This project relies on several third party dependencies that must be installed in order to be able to build the binary. Below the list of libraries used:

- <lmic.h>             // For the Lorawan comunication - Release v2.3.2
https://github.com/mcci-catena/arduino-lmic
- <LowPower.h>         // To allow the low power arduino energy  consumption Release v1.6.0
https://github.com/rocketscream/Low-Power
- <forcedClimate.h>    // Getting BME280 data Release v3.0.0
https://github.com/JVKran/Forced-BME280
- <YetAnotherPcInt.h>  // To choose the port for the door control interruption Release v2.1.0
https://github.com/paulo-raca/YetAnotherArduinoPcIntLibrary 
- <CayenneLPP.h>       // Cayenne Library for the payload format Release v1.0.1
https://github.com/ElectronicCats/CayenneLPP

Once you have installed and downloaded the repository in the software directory, the code is available to compile it and upload it to the lorapertura device.
Remember that you must fill in the specific data of your device in FILLMEIN depending on the configuration (OTAA or ABP) of your device in [The Things Network](https://www.thethingsnetwork.org) 

Code to replace FILLMEIN in ABP device
```c
// LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t NWKSKEY[16] = { FILLMEIN } ;`

// LoRaWAN AppSKey, application session key
static const u1_t PROGMEM APPSKEY[16] = { FILLMEIN } ;

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
// The library converts the address to network byte order as needed.
static const u4_t DEVADDR = FILLMEIN ; // <-- Change this address for every node!
```

### Select board and upload the binary 
Two options selecting the board depending on the bootloader of the ttnmad node:
If you load the Minicore bootloader as indicated in the doc as optional then select ATmega328 board
![minicore](/images/minicore-board.png)
otherwise you have to choose the "Arduino Pro or Pro Mini" board.
![arduino-mini](/images/arduino-mini.png)
Then select the port where the board is connected to the computer in Tools > Ports
And finally click on the rounded arrow button on the top to upload the project to the board or go to Program > Upload (Ctl+U)
