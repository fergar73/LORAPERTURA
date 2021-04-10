### Build on Arduino IDE

You can install the Arduino IDE by downloading it from [arduino.cc](https://www.arduino.cc). This project has been developed using 1.8.13 release. 

#### Installing dependencies

This project relies on several third party dependencies that must be installed in order to be able to build the binary. Below the list of libraries used:

<lmic.h>             // Libreria para la implementación del protocolo Lorawan v2.3.2
https://github.com/mcci-catena/arduino-lmic
<LowPower.h>         // Librería para la gestión de bajo consumo del arduino v1.6.0
https://github.com/rocketscream/Low-Power
<forcedClimate.h>    // Librería para acceder al sensor BME280 con bajo consumo de memoria v3.0.0
https://github.com/JVKran/Forced-BME280
<YetAnotherPcInt.h>  // Permite elegir el puerto para la gestión de la interrupción de puerta abierta. v2.1.0
https://github.com/paulo-raca/YetAnotherArduinoPcIntLibrary 
<CayenneLPP.h>       // Construcción del payload en formato Cayenne v1.0.1
https://github.com/ElectronicCats/CayenneLPP

Once you have cloned this project to a local directory, you can open it from the Arduino IDE
Build and upload the project

#### Select board and upload the binary 

Two options selecting the board depending on the bootloader of the ttnmad node:
If you load the Minicore bootloader as indicated in the doc as optional thenselect ATmega328 board
otherwise you have to choose the "Arduino Pro or Pro Mini" board 


Then select the port where the board is connected to the computer in Tools > Ports

And finally click on the rounded arrow button on the top to upload the project to the board or go to Program > Upload (Ctl+U)
