/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI
 * Copyright (c) 2019 Fernando García Martín
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 *
 * This uses ABP (Activation-by-personalisation), where a DevAddr and
 * Session keys are preconfigured (unlike OTAA, where a DevEUI and
 * application key is configured, while the DevAddr and session keys are
 * assigned/generated in the over-the-air-activation procedure).
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

#include <lmic.h>
#include <hal/hal.h>
#include <Wire.h>
#include <SPI.h>
#include "LowPower.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <YetAnotherPcInt.h>
#include <CayenneLPP.h>
CayenneLPP lpp(25);

// Define el SpreadFactor que se utilizará en la transmisión por Lora
#define SpreadFactor DR_SF9


// Definición de Pin A2 para Interrupción Sensor Puerta Abierta
#define pinEventoPuertaAbierta A2
volatile boolean eventoPuertaAbierta = false;  // Inicializamos la marca de evento de Puerta Abierta a falso

// Sensor de luminosidad - TEMT6000
#define LIGHTSENSORPIN A0 //Conectado al pin Analogico A0 light sensor reading 

// Sensor BME280 conectado al puerto I2C para datos de temperatura, presión y humedad
Adafruit_BME280 bme; // I2C

//
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

// LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t NWKSKEY[16] = { 0xE2, 0x9F, 0xB4, 0xBC, 0x12, 0xC2, 0xBD, 0x13, 0x5E, 0x8A, 0xA4, 0xE4, 0xB0, 0x80, 0x4B, 0xAE } ;

// LoRaWAN AppSKey, application session key
static const u1_t PROGMEM APPSKEY[16] = { 0x42, 0x07, 0xD6, 0xA1, 0xD2, 0x14, 0xC9, 0x33, 0x6C, 0x7C, 0x89, 0x90, 0xC9, 0x10, 0x3B, 0x00 } ;

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
// The library converts the address to network byte order as needed.
static const u4_t DEVADDR = 0x26011AB8 ; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h,
// otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

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

unsigned long inicio;   // Para controlar los tiempos de movimiento sin utilizar funciÃƒÂ³n delay.
unsigned long actual;   // Para controlar los tiempos de movimiento sin utilizar funciÃƒÂ³n delay.
unsigned long fin;      // Para controlar los tiempos de movimiento sin utilizar funciÃƒÂ³n delay.


// Funcion que devuelve el voltaje de las baterías
// Fuente Juan Félix Mateos - Talleres nodo v2.2 TTN MAD
// Hay que eliminar el delay(2) para evitar problema con las interrupciones.

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

  boolean ev_antes_envio = false;
  boolean ev_despues_envio = false;
  
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        /* Tomamos la lectura del sensor BME280 para el envío */
        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                      Adafruit_BME280::SAMPLING_X1, // temperature
                      Adafruit_BME280::SAMPLING_X1, // pressure
                      Adafruit_BME280::SAMPLING_X1, // humidity
                      Adafruit_BME280::FILTER_OFF   );

    // Prepare upstream data transmission at the next possible time.
        lpp.reset();
        lpp.addAnalogInput(1, readVcc() / 1000.F);                      // Enviamos Voltaje actual 
        lpp.addTemperature(4, bme.readTemperature());                   // Enviamos la medición de temperatura
        lpp.addBarometricPressure(5, bme.readPressure() / 100.0F);      // Enviamos la medición de presión
        lpp.addRelativeHumidity(6, bme.readHumidity());                 // Enviamos la medición de humedad
        lpp.addLuminosity(7,analogRead(LIGHTSENSORPIN));                // Enviamos la medición de luminosidad        
        LMIC.pendTxConf = eventoPuertaAbierta;    // Si se ha producido el evento de puerta abierta activamos el envío del mensaje con ACK
//        LMIC.pendTxConf = false;  // Enviamos sin confirmación
        ev_antes_envio = eventoPuertaAbierta;
        lpp.addDigitalOutput(2, eventoPuertaAbierta);                   // Enviamos marca si se ha abierto la puerta        
        lpp.addDigitalOutput(3, digitalRead(pinEventoPuertaAbierta));   // Enviamos el estado de la puerta cerrada || abierta
        LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), LMIC.pendTxConf);  //  Se hace el envío del mensaje a TTN
        ev_despues_envio = eventoPuertaAbierta;
        Serial.println(eventoPuertaAbierta); 
        if (ev_antes_envio && ev_despues_envio && eventoPuertaAbierta) {
          eventoPuertaAbierta = false;            // Se ha enviado paquete con la marca de puerta abierta. Hay que resetear el evento.
        }
        Serial.print(F("Paquete enviado a TTN "));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void abriendoPuerta()
{
  eventoPuertaAbierta = true;
  Serial.println(F("Evento puerta"));
}

void esperaConInterrupciones(unsigned long milisegundosEspera)
{
    actual = millis();
    Serial.print(actual);    Serial.println(F("Inicio"));
    fin = actual + milisegundosEspera;
    while ( (actual <= fin) & (not eventoPuertaAbierta)) {
      actual = millis();
    }
    Serial.print(fin);    Serial.println(F("Fin"));
}

void setup() {

  // Depuración y traza de la programación a traves del puerto Serie a 115200 baudios
    while (!Serial); // wait for Serial to be initialized
    Serial.begin(115200);
    
    // per sample code on RF_95 test  ¿¿??¿¿??
    // No entiendo bien por qué este delay de 100 milisegundos.
    esperaConInterrupciones(100);
    
    Serial.println(F("Setup-Inicio Lorapertura"));

  
    // Asociamos el pinEventoPuertaAbierta a la Interrupción.
    pinMode(13,OUTPUT);
    pinMode(pinEventoPuertaAbierta, INPUT_PULLUP);
    PcInt::attachInterrupt(pinEventoPuertaAbierta, abriendoPuerta, FALLING);
//    attachInterrupt(digitalPinToInterrupt(pinEventoPuertaAbierta), abriendoPuerta, FALLING);

    Serial.println(F("Interrupcion asociada"));

    // Asociamos el pin de entrada analogico al sensor TEMT6000
    pinMode(LIGHTSENSORPIN,  INPUT);  
    
    unsigned status;
    
    status = bme.begin(0x76);  
    if (!status) {
        Serial.println(F("Could not find a valid BME280 sensor"));
        while (1);
    }
    else {
      Serial.println(F("Sensor BME280 ready"));
    }

// Esperamos un segundo antes de iniciarlizar la comunicación Lora
    digitalWrite(13,HIGH);
    esperaConInterrupciones(1000);
    digitalWrite(13,LOW);

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x13, DEVADDR, nwkskey, appskey);
    #else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x13, DEVADDR, NWKSKEY, APPSKEY);
    #endif

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
//    LMIC_setDrTxpow(DR_SF10,14);
    LMIC_setDrTxpow(SpreadFactor,14);

    LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

// Esperamos un 1/2 segundo despues de iniciarlizar la comunicación Lora

    esperaConInterrupciones(250);

    Serial.println(F("LMIC Lorawan Ready"));

// Esperamos un 1/2 segundo para comenzar envío 

    esperaConInterrupciones(250);

    Serial.println(F("Setup-Fin Lorapertura - Arranca loop"));

}

void loop() {
// No hacemos nada durante 2 minutos.
  Serial.print(contador);Serial.println(F(" Empezamos ciclo loop - sleep 120"));
  esperaConInterrupciones(120000);  
//  for (byte i = 0; i < 10; i++) {
//    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
//  }
  Serial.print(contador);Serial.println(F(" Wake up"));
  // Despues del minuto mandamos mensaje para ttnmapper  
  Serial.print(contador);Serial.println(F(" Send to TTN"));
  do_send(&sendjob);
  Serial.print(contador);Serial.println(F(" While not completado"));
  do {
    os_runloop_once();    
    
  } while (not completado);
  Serial.print(contador);Serial.println(F(" Terminamos ciclo loop"));
  contador = contador + 1;      
  completado = false;
}        
