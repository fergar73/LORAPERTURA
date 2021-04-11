## Programación con Arduino IDE
Puedes instalar el arduino IDE disponible en [arduino.cc](https://www.arduino.cc). Este proyecto ha sido desarrollado con la versión 1.8.13.

### Dependencias
Este proyecto requiere de la instalación en el arduino IDE de las siguientes librerías. Son necesarias para la compilación y correcto funcionamiento del dispositivo. 

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

Una vez tengas instalado y descargado el repositorio en el directorio software está el código disponible para compilarlo y subirlo al dispositivo lorapertura.
Recuerda que debes rellenar los datos concreto de tu dispositivo en FILLMEIN dependiendo de la configuración (OTAA o ABP) de tu dispositivo en [The Things Network](https://www.thethingsnetwork.org)

Lugar del código activación ABP donde sustituir el FILLMEIN 
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

### Selección de la placa y carga del programa en el dispositivo

Hay 2 opciones de seleccionar la placa dependiendo del "bootloader" que tenga cargado el nodo TTNMAD.
Si tiene cargado el Minicore bootloader que se indica en la documentación como paso opcional, hay que indicar la placa ATmega328 
![minicore](images/minicore-board.png)
si no es el caso hay que seleccionar la placa "Arduino Pro or Pro Mini" con procesador ATmega328P (3.3V,8MHz).
![arduino-mini](images/arduino-mini.png)
Seleciona el puerto que esté conectado a la placa y mediante el arduino IDE pulsa la opción de subir el código (Ctl+U)