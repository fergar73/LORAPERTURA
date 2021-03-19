/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI
 * Copyright (c) 2020 Fernando García Martín
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 *
 * This uses OTAA (Over-the-air activation), where where a DevEUI and
 * application key is configured, which are used in an over-the-air
 * activation procedure where a DevAddr and session keys are
 * assigned/generated for use with all further communication.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!
 *
 * To use this sketch, first register your application and device with
 * the things network, to set or generate a DevAddr, NwkSKey and
 * AppSKey. Each device should have their own unique values for these
 * fields.
 *
 * Do not forget to define the radio type correctly in
 * arduino-lmic/project_config/lmic_project_config.h or from your BOARDS.txt.
 *
 *******************************************************************************/

 // References:
 // [feather] adafruit-feather-m0-radio-with-lora-module.pdf

#include <lmic.h>             // Libreria para la implementación del protocolo Lorawan
#include <hal/hal.h>          // Hardware Abstraction Layer for LMIC
#include <SPI.h>              // Bus SPI - utilizado para la comunicación con el RFM95
#include <Wire.h>             // Bus I2C - utilizado para comunicación con el BME280
#include "LowPower.h"         // Librería para la gestión de bajo consumo del arduino
#include <forcedClimate.h>    // Librería para acceder al sensor BME280 con bajo consumo de memoria
#include <YetAnotherPcInt.h>  // Permite elegir el puerto para la gestión de la interrupción de puerta abierta.
#include <CayenneLPP.h>       // Construcción del payload en formato Cayenne

// Define el SpreadFactor fijo que se utilizará en la transmisión Lora
#define SpreadFactor DR_SF7
// Indicar el tiempo que tarda el mensaje en el aire según el Spread Factor indicado
// se puede utilizar para ello la utilidad: https://avbentem.github.io/airtime-calculator/ttn
const unsigned long airTimeMs = 61.7;  // Expresado en milisegundos


unsigned int frecuenciaEnvioMsg;  // Tiempo en segundos entre cada envio de mensaje, calculado en función del airTimeMs 
unsigned int tiempoDormido = 0;   // Control del tiempo que el procesador esta en modo LowPower

boolean modoAhorroEnergia = true; // Para activar o desactivar el modo LowPower  

// Definición de Pin A2 para Interrupción Sensor Puerta Abierta
#define pinEventoPuertaAbierta A2

// Variables para el control de la interrupción de Puerta Abierta 
volatile boolean eventoPuertaAbierta = false;   // Cuando se abre la puerta salta la interrupción y estará a true. Inicializada a false
boolean ev_antes_envio = false;                 // Contiene el valor de la variable eventoPuertaAbierta antes del envío.
boolean ev_despues_envio = false;               // Contiene el valor de la variable eventoPuertaAbierta despues del envío.

// Sensor de luminosidad - TEMT6000  
#define LIGHTSENSORPIN A0     //Conectado al pin Analogico A0 light sensor reading 
float lux = 0.0;              // Variable para enviar la luminosidad en lux

// Sensor BME280 conectado al puerto I2C en la dirección 0x76 para obtener datos de temperatura, presión y humedad
ForcedClimate climateSensor = ForcedClimate();

// Variable en formato Cayenne para el envío del payload a TTN. Se indica tamaño de cada campo del payload.
// LPP_ANALOG_INPUT_SIZE 4        -- 2 bytes  -- "analog_in_1"
// LPP_DIGITAL_OUTPUT_SIZE 3      -- 1 byte   -- "digital_out_2" y "digital_out_3"
// LPP_TEMPERATURE_SIZE 4         -- 2 bytes  -- "temperature_4"
// LPP_BAROMETRIC_PRESSURE_SIZE 4 -- 2 bytes  -- "barometric_pressure_5"
// LPP_RELATIVE_HUMIDITY_SIZE 3   -- 1 byte   -- "relative_humidity_6"
// LPP_LUMINOSITY_SIZE 4          -- 2 bytes  -- "luminosity_7"
CayenneLPP lpp(25);

// For normal use, we require that you edit the sketch to replace FILLMEIN
// with values assigned by the TTN console. However, for regression tests,
// we want to be able to compile these scripts. The regression tests define
// COMPILE_REGRESSION_TEST, and in that case we define FILLMEIN to a non-
// working but innocuous value.
//

#ifdef COMPILE_REGRESSION_TEST
# define FILLMEIN 0
#else
# warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
# define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif


// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={ FILLMEIN };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]={ FILLMEIN };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
static const u1_t PROGMEM APPKEY[16] = { FILLMEIN };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

static osjob_t sendjob;

// Pin mapping
// Adapted for Feather M0 per p.10 of [feather]
const lmic_pinmap lmic_pins = {
    .nss = 10,                       // chip select on feather (rf95module) CS
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,                       // reset pin
    .dio = {2, 7, LMIC_UNUSED_PIN}, // assumes external jumpers [feather_lora_jumper]
                                    // DIO1 is on JP1-1: is io1 - we connect to GPO6
                                    // DIO1 is on JP5-3: is D2 - we connect to GPO5
};

int contador = 0;
boolean completado = false;

unsigned long inicio;   // Para controlar los tiempos de espera sin utilizar función delay.
unsigned long actual;   // Para controlar los tiempos de espera sin utilizar función delay.
unsigned long fin;      // Para controlar los tiempos de espera sin utilizar función delay.


// Funcion que devuelve el voltaje de las baterías
// Fuente Juan Félix Mateos - Talleres nodo v2.2 TTN MAD

long readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) |
  _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  long result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Variable completado se marca a cierto cuando se ha completado la transacción de envío.
            // Nos sirve para dar paso al próximo ciclo de envío con la seguridad que la transacción que estaba siendo gestionada ha terminado.
            completado = true;  
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

void do_send(osjob_t* j){

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
    // Prepare upstream data transmission at the next possible time.
        lpp.reset();
        lpp.addAnalogInput(1, readVcc() / 1000.F);                        // Enviamos Voltaje actual 
        climateSensor.takeForcedMeasurement();                            // Obtenemos los datos de temperatura, presión y humedad del BME280
        lpp.addTemperature(4, climateSensor.getTemperatureCelcius());     // Enviamos la medición de temperatura
        lpp.addBarometricPressure(5, climateSensor.getPressure());        // Enviamos la medición de presión
        lpp.addRelativeHumidity(6, climateSensor.getRelativeHumidity());  // Enviamos la medición de humedad
        lux = ((((analogRead(LIGHTSENSORPIN) * 3.3 / 1024) / 10000.0) * 1000000) * 2.0 );  // Obtenemos la medición de luminosidad de TEMT6000 y la pasamos a lux
        lpp.addLuminosity(7,lux);                                         // Enviamos la medición de luminosidad        
        ev_antes_envio = eventoPuertaAbierta;                             // Guardamos el valor del eventoPuertaAbierta antes del envío
        lpp.addDigitalOutput(2, eventoPuertaAbierta);                     // Enviamos valor de eventoPuertaAbierta para saber si ha saltado la interrupción de Puerta Abierta
        lpp.addDigitalOutput(3, digitalRead(pinEventoPuertaAbierta));     // Estado de la puerta 1 => Cerrada || 0 => Abierta
        LMIC.pendTxConf = eventoPuertaAbierta;                            // Si se ha producido el evento de puerta abierta activamos el envío del mensaje con ACK
        LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), LMIC.pendTxConf);  //  Se hace el envío del mensaje a TTN
        ev_despues_envio = eventoPuertaAbierta;                           // Guardamos el valor del eventoPuertaAbierta despues del envío
        Serial.print(F("Paquete enviado a TTN "));
        Serial.print("ev_antes_envio ");Serial.print(ev_antes_envio);
        Serial.print(" ev_despues_envio ");Serial.print(ev_despues_envio);
        Serial.print(" eventoPuertaAbierta ");Serial.println(eventoPuertaAbierta);
        if (ev_antes_envio && ev_despues_envio && eventoPuertaAbierta) {
          eventoPuertaAbierta = false;            // Se ha confirmado el envio a TTN con ACK del eventoPuertaAbierta. Se inicializa a falso para el siguiente ciclo de envío.
          Serial.print("EventoPuertaReseteado ");Serial.println(eventoPuertaAbierta);
        }  
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

// Función de la Interrupción de Puerta Abierta.
void abriendoPuerta()
{
  eventoPuertaAbierta = true;        // Al saltar la interrupción de Puerta Abierta, se establece a cierto/true la variable eventoPuertaAbierta
  Serial.println(F("Evento puerta"));
}

// Funcion similar al delay pero que tiene en cuenta la interrupción de puerta abierta para gestionarla sin esperas.
// Si eventoPuertaAbierta es true, la función no quedará en espera.
void esperaConInterrupciones(unsigned long milisegundosEspera)
{
    actual = millis();
    fin = actual + milisegundosEspera;
    while ( (actual <= fin) & (not eventoPuertaAbierta)) {
      actual = millis();
    }
}

void setup() {
    // Encendemos pin para marcar inicio del setup
    pinMode(13,OUTPUT);
    digitalWrite(13,HIGH);

    // Depuración y traza de la programación a traves del puerto Serie a 115200 baudios
    while (!Serial); // wait for Serial to be initialized
    Serial.begin(115200);

    // Revisar la utilidad de este delay
    esperaConInterrupciones(50);    
    Serial.println(F("Setup-Inicio Lorapertura"));
    
    // Asociamos el pinEventoPuertaAbierta a la Interrupción.
    pinMode(pinEventoPuertaAbierta, INPUT_PULLUP);
    PcInt::attachInterrupt(pinEventoPuertaAbierta, abriendoPuerta, FALLING);
    Serial.print(F("Interrupcion preparada.."));

    // Asociamos el pin de entrada analogico al sensor TEMT6000
    pinMode(LIGHTSENSORPIN,  INPUT);  
    Serial.print(F("Sensor Lux asociado.."));
    
    // Arrancamos comunicación con el sensor BME280    
    climateSensor.begin();  
    Serial.print(F("Sensor BME280 listo.."));
    
// Esperamos un medio segundo antes de iniciarlizar la comunicación Lora
    esperaConInterrupciones(500);

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); //Para mejorar la recepción de los downlinks
 
    #if defined(CFG_eu868)
    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    // TTN defines an additional channel at 869.525Mhz using SF9 for class B
    // devices' ping slots. LMIC does not have an easy way to define set this
    // frequency and support for class B is spotty and untested, so this
    // frequency is not configured here.
    #elif defined(CFG_us915)
    // NA-US channels 0-71 are configured automatically
    // but only one group of 8 should (a subband) should be active
    // TTN recommends the second sub band, 1 in a zero based count.
    // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
    LMIC_selectSubBand(1);
    #endif

    // 
    LMIC_setAdrMode(0);
    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(SpreadFactor,14);

    Serial.println(F("LMIC Lorawan Ready"));

    // Se calcula la frecuencia de envio de mensajes de acuerdo a la politica de acceso justo establecida en The Things Network
    frecuenciaEnvioMsg = 86400 / (30000 / airTimeMs);
    Serial.println(frecuenciaEnvioMsg);

    // Se avisa con parpadeo del led del fin del setup
    for (int led=0; led <= 3; led++){
          digitalWrite(13,HIGH);
          esperaConInterrupciones(500);
          digitalWrite(13,LOW);
          esperaConInterrupciones(500);
   } 
    Serial.println(F("Setup-Fin Lorapertura"));
}

void loop() {
  tiempoDormido = 0;
  Serial.print(contador);Serial.println(F(" Empezamos ciclo loop"));
  if (modoAhorroEnergia) {
    Serial.println(F(" Modo dormido"));
    do {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);       // Modo arduino dormido por 8 segundos  -- SLEEP_8S
      tiempoDormido = tiempoDormido + 8;    
    } while ((tiempoDormido < frecuenciaEnvioMsg) & (not eventoPuertaAbierta));   // En caso de puerta abierta se avanza para el envio del mensaje
  }
  else {
    Serial.println(F(" Modo espera"));
    esperaConInterrupciones(long(frecuenciaEnvioMsg) * 1000);
  }
  Serial.print(contador);Serial.println(F(" Send to TTN"));
  do_send(&sendjob);      // Toma de medidas y envío del mensaje a TTN en el próximo ciclo
  Serial.print(contador);Serial.println(F(" While not completado"));
  do {
    os_runloop_once();    // Gestión del envío y recepción de mensajes enviados por la librería LMIC. Variable completado indica que podemos avanzar al siguiente ciclo de envío.      
  } while (not completado);
  completado = false;           // Actualizar completado a falso para el próximo ciclo de envío
  ev_antes_envio = false;       // Se inicializa el control de envío de puerta abierta antes.
  ev_despues_envio = false;     // Se inicializa el control de envío de puerta abierta despues.
  // En algun caso para asegurar la recepción de mensajes certificados se hacen reintentos automáticos incrementando el  SF 
  // Se sobreescribe el SpreadFactor para asegurar el envío en el SF definido.
  LMIC_setDrTxpow(SpreadFactor,14);    
  Serial.print(contador);Serial.println(F(" Terminamos ciclo loop"));
  contador = contador + 1;      
}        
