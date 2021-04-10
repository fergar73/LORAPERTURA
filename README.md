### LORAPERTURA
El dispositivo lorapertura permite saber cuando una puerta está abierta o cerrada e informar de su estado a [The Things Network](https://www.thethingsnetwork.org/). Ofrece información de temperatura, presión y humedad del sitio donde esté instalado así como la luminosidad del espacio.

The lorapertura device lets you know when a door is open or closed and report its status to [The Things Network](https://www.thethingsnetwork.org/). It offers information on temperature, pressure and humidity of the place where it is installed as well as the luminosity of the space. 

#### Hardware
- Nodo TTN Madrid: View this repo  [documentation]( https://github.com/IoTopenTech/Nodo_TTN_MAD_V2) for full details and follow the [guide](https://github.com/IoTopenTech/Nodo_TTN_MAD_V2/blob/master/Montaje%20nodo%20TTN%20MAD%20v2_2%20basico.pdf) to build it from scratch.
- Sensor magnético puerta abierta
- Sensor Bosch BME280
- Sensor TEMT6000 

#### Software
Se proporcionan dos "sketches" de arduino en el directorio del software dependiendo de la forma preferida de conectar el dispositivo (OTAA O ABP) en the things network.

Se ha utilizado el Arduino IDE para este proyecto, los detalles se indican en [SW-Arduino-IDE](https://github.com/fergar73/lorapertura/SW-Arduino-IDE.md)

Two arduino sketches are provided in the software directory depending on the preferred way to authenticate the device (OTAA OR ABP) in the things network.

#### 3D - Caja Dispositivo / Device Enclosure
Se propone una caja de interior junto con su tapa para fijar el dispositivo a la pared en la carpeta caja-3d. Este dispositivo se ha realizado utilizando freecad

![caja-lorapertura](./images/caja-lorapertura.png)

An indoor box is proposed together with its cover to fix the device to the wall in the box-3d folder.

#### Contribuciones
Se pueden realizar contribuciones al proyecto, puedes hacer un fork en local y plantear los pull-request con las mejoras realizadas.

Contributions can be made to the project, you can fork locally and raise the pull-request with the improvements made.

#### Licencia

 lorapertura © 2021 by fergar73 is licensed under CC BY-NC-SA 4.0 - Attribution-NonCommercial-ShareAlike 4.0 International License. To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
 
#### Agradecimientos
Este proyecto ha sido posible gracias a la extraordinaria labor que se está realizando desde la comunidad [The Things Network Madrid](https://www.thethingsnetwork.org/community/madrid/)
 
