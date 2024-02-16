///////////////////////////////////////////////////////////////////////////////
// growatt2lorawan-v2.ino
// 
// LoRaWAN Node for Growatt PV-Inverter Data Interface
//
// - implements power saving by using the ESP32 deep sleep mode
// - implements fast re-joining after sleep by storing network session data
// - LoRa_Serialization is used for encoding various data types into bytes
// - internal Real-Time Clock (RTC) set from LoRaWAN network time (optional)
//
//
// Based on:
// ---------
// RadioLib LoRaWAN example
// https://github.com/jgromes/RadioLib/tree/master/examples/LoRaWAN/LoRaWAN_End_Device_Reference
//
//
// Library dependencies (tested versions):
// ---------------------------------------
// RadioLib                             6.4.2
// LoRa_Serialization                   3.2.1
// ESP32Time                            2.0.0
//
//
// created: 03/2023
//
//
// MIT License
//
// Copyright (c) 2022 Matthias Prinke
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//
// History:
//
// 20240216 Created from matthias-bs/growatt2lorawan
//
// Notes:
// - After a successful transmission, the controller can go into deep sleep
//   (option SLEEP_EN)
// - Sleep time is defined in SLEEP_INTERVAL
// - If joining the network or transmitting uplink data fails,
//   the controller can go into deep sleep
//   (option FORCE_SLEEP)
// - Timeout is defined in SLEEP_TIMEOUT_INITIAL and SLEEP_TIMEOUT_JOINED
// - The EEPROM(?) is used to store information about the LoRaWAN 
//   network session; this speeds up the connection after a restart
//   significantly
// - settimeofday()/gettimeofday() must be used to access the ESP32's RTC time
// - Arduino ESP32 package has built-in time zone handling, see 
//   https://github.com/SensorsIot/NTP-time-for-ESP8266-and-ESP32/blob/master/NTP_Example/NTP_Example.ino
//
///////////////////////////////////////////////////////////////////////////////

// include the library
#include <RadioLib.h>
#include <Preferences.h>
#include "src/settings.h"
#include "src/payload.h"

//-----------------------------------------------------------------------------
//
// User Configuration
//

// Enable debug mode (debug messages via serial port)
#define _DEBUG_MODE_

// Enable sleep mode - sleep after successful transmission to TTN (recommended!)
//#define SLEEP_EN

// Enable setting RTC from LoRaWAN network time
#define GET_NETWORKTIME

#if defined(GET_NETWORKTIME)
    // Enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
    const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

    // RTC to network time sync interval (in minutes)
    #define CLOCK_SYNC_INTERVAL 24 * 60
#endif

// Number of uplink ports
#define NUM_PORTS 2

// Uplink period multipliers
#define UPLINK_PERIOD_MULTIPLIERS {1, 5}
typedef struct {
    int port;
    int mult;
} Schedule;

const Schedule UplinkSchedule[NUM_PORTS] = {
  // {port, mult}
  {1, 1},
  {2, 2}
};

// If SLEEP_EN is defined, MCU will sleep for SLEEP_INTERVAL seconds after succesful transmission
#define SLEEP_INTERVAL 360

// Long sleep interval, MCU will sleep for SLEEP_INTERVAL_LONG seconds if battery voltage is below BATTERY_WEAK
#define SLEEP_INTERVAL_LONG 900

// Force deep sleep after a certain time, even if transmission was not completed
//#define FORCE_SLEEP

// During initialization (not joined), force deep sleep after SLEEP_TIMEOUT_INITIAL (if enabled)
#define SLEEP_TIMEOUT_INITIAL 1800

// If already joined, force deep sleep after SLEEP_TIMEOUT_JOINED seconds (if enabled)
#define SLEEP_TIMEOUT_JOINED 600

// Additional timeout to be applied after joining if Network Time Request pending
#define SLEEP_TIMEOUT_EXTRA 300

// Debug printing
// To enable debug mode (debug messages via serial port):
// Arduino IDE: Tools->Core Debug Level: "Debug|Verbose"
// or
// set CORE_DEBUG_LEVEL in BresserWeatherSensorTTNCfg.h
#define DEBUG_PORT Serial2
#define DEBUG_PRINTF(...) { log_d(__VA_ARGS__); }

//-----------------------------------------------------------------------------

#if defined(GET_NETWORKTIME)
  #include <ESP32Time.h>
#endif

// LoRa_Serialization
#include <LoraMessage.h>

// Pin mappings for some common ESP32 LoRaWAN boards.
// The ARDUINO_* defines are set by selecting the appropriate board (and borad variant, if applicable) in the Arduino IDE.
// The default SPI port of the specific board will be used.
#if defined(ARDUINO_TTGO_LoRa32_V1)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/ttgo-lora32-v1/pins_arduino.h
    // http://www.lilygo.cn/prod_view.aspx?TypeId=50003&Id=1130&FId=t3:50003:3
    // https://github.com/Xinyuan-LilyGo/TTGO-LoRa-Series
    // https://github.com/LilyGO/TTGO-LORA32/blob/master/schematic1in6.pdf
    #define PIN_LMIC_NSS      LORA_CS
    #define PIN_LMIC_RST      LORA_RST
    #define PIN_LMIC_DIO0     LORA_IRQ
    #define PIN_LMIC_DIO1     33
    #define PIN_LMIC_DIO2     cMyLoRaWAN::lmic_pinmap::LMIC_UNUSED_PIN

#elif defined(ARDUINO_TTGO_LoRa32_V2)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/ttgo-lora32-v2/pins_arduino.h
    #define PIN_LMIC_NSS      LORA_CS
    #define PIN_LMIC_RST      LORA_RST
    #define PIN_LMIC_DIO0     LORA_IRQ
    #define PIN_LMIC_DIO1     33
    #define PIN_LMIC_DIO2     cMyLoRaWAN::lmic_pinmap::LMIC_UNUSED_PIN
    #pragma message("LoRa DIO1 must be wired to GPIO33 manually!")

#elif defined(ARDUINO_TTGO_LoRa32_v21new)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/ttgo-lora32-v21new/pins_arduino.h
    #define PIN_LMIC_NSS      LORA_CS
    #define PIN_LMIC_RST      LORA_RST
    #define PIN_LMIC_DIO0     LORA_IRQ
    #define PIN_LMIC_DIO1     LORA_D1
    #define PIN_LMIC_DIO2     LORA_D2

#elif defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wifi_lora_32_V2)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/heltec_wireless_stick/pins_arduino.h
    // https://github.com/espressif/arduino-esp32/tree/master/variants/heltec_wifi_lora_32_V2/pins_ardiono.h
    #define PIN_LMIC_NSS      SS
    #define PIN_LMIC_RST      RST_LoRa
    #define PIN_LMIC_DIO0     DIO0
    #define PIN_LMIC_DIO1     DIO1
    #define PIN_LMIC_DIO2     DIO2

#elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
    #define PIN_LMIC_NSS      6
    #define PIN_LMIC_RST      9
    #define PIN_LMIC_DIO0     5
    #define PIN_LMIC_DIO1     11
    #define PIN_LMIC_DIO2     cMyLoRaWAN::lmic_pinmap::LMIC_UNUSED_PIN
    #pragma message("ARDUINO_ADAFRUIT_FEATHER_ESP32S2 defined; assuming RFM95W FeatherWing will be used")
    #pragma message("Required wiring: E to IRQ, D to CS, C to RST, A to DI01")
    #pragma message("BLE is not available!")

#elif defined(ARDUINO_FEATHER_ESP32)
    #define PIN_LMIC_NSS      14
    #define PIN_LMIC_RST      27
    #define PIN_LMIC_DIO0     32
    #define PIN_LMIC_DIO1     33
    #define PIN_LMIC_DIO2     cMyLoRaWAN::lmic_pinmap::LMIC_UNUSED_PIN
    #pragma message("NOT TESTED!!!")
    #pragma message("ARDUINO_ADAFRUIT_FEATHER_ESP32 defined; assuming RFM95W FeatherWing will be used")
    #pragma message("Required wiring: A to RST, B to DIO1, D to DIO0, E to CS")

#elif defined(FIREBEETLE_ESP32_COVER_LORA)
    // https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478
    // https://wiki.dfrobot.com/FireBeetle_Covers_LoRa_Radio_868MHz_SKU_TEL0125
    #define PIN_LMIC_NSS      27 // D4
    #define PIN_LMIC_RST      25 // D2
    #define PIN_LMIC_DIO0     26 // D3
    #define PIN_LMIC_DIO1      9 // D5
    #define PIN_LMIC_DIO2     cMyLoRaWAN::lmic_pinmap::LMIC_UNUSED_PIN
    #pragma message("FIREBEETLE_ESP32_COVER_LORA defined; assuming FireBeetle ESP32 with FireBeetle Cover LoRa will be used")
    #pragma message("Required wiring: D2 to RESET, D3 to DIO0, D4 to CS, D5 to DIO1")

#elif defined(LORAWAN_NODE)
    // LoRaWAN_Node board
    // https://github.com/matthias-bs/LoRaWAN_Node
    #pragma message("LORAWAN_NODE defined; assuming LoRaWAN_Node board will be used")
    #define PIN_LMIC_NSS      14
    #define PIN_LMIC_RST      12
    #define PIN_LMIC_DIO0     4
    #define PIN_LMIC_DIO1     16
    #define PIN_LMIC_DIO2     17

#else
    #pragma message("Unknown board; please select one in the Arduino IDE or in settings.h or create your own!")
    
    // definitions for generic CI target ESP32:ESP32:ESP32
    #define PIN_LMIC_NSS      14
    #define PIN_LMIC_RST      12
    #define PIN_LMIC_DIO0     4
    #define PIN_LMIC_DIO1     16
    #define PIN_LMIC_DIO2     17

#endif

/// Modbus interface select: 0 - USB / 1 - RS485
bool modbusRS485;

// Uplink message payload size
// The maximum allowed for all data rates is 51 bytes.
const uint8_t PAYLOAD_SIZE = 51;

/// Uplink payload buffer
static uint8_t loraData[PAYLOAD_SIZE];

/// ESP32 preferences (stored in flash memory)
Preferences preferences;

struct sPrefs {
  uint16_t  sleep_interval;       //!< preferences: sleep interval
  uint16_t  sleep_interval_long;  //!< preferences: sleep interval long
} prefs;

// SX1262 has the following pin order:
// Module(NSS/CS, DIO1, RESET, BUSY)
// SX1262 radio = new Module(8, 14, 12, 13);

// SX1278 has the following pin order:
// Module(NSS/CS, DIO0, RESET, DIO1)
SX1278 radio = new Module(PIN_LMIC_NSS, PIN_LMIC_DIO0, PIN_LMIC_RST, PIN_LMIC_DIO1);

// create the node instance on the EU-868 band
// using the radio module and the encryption key
// make sure you are using the correct band
// based on your geographical location!
LoRaWANNode node(&radio, &EU868);

// for fixed bands with subband selection
// such as US915 and AU915, you must specify
// the subband that matches the Frequency Plan
// that you selected on your LoRaWAN console
/*
  LoRaWANNode node(&radio, &US915, 2);
*/

void setup() {
    pinMode(INTERFACE_SEL, INPUT_PULLUP);
    modbusRS485 = digitalRead(INTERFACE_SEL);
    

    // set baud rate
    if (modbusRS485) {
        Serial.begin(115200);
        log_d("Modbus interface: RS485");
    } else {
        Serial.setDebugOutput(false);
        DEBUG_PORT.begin(115200, SERIAL_8N1, DEBUG_RX, DEBUG_TX);
        DEBUG_PORT.setDebugOutput(true);
        log_d("Modbus interface: USB");
    }

  // initialize radio (SX1262 / SX1278 / ... ) with default settings
  Serial.print(F("[Radio] Initializing ... "));
  int state = radio.begin();
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(true);
  }

  // APPEUI, DEVEUI and APPKEY
  #include "secrets.h"

  #ifndef SECRETS
    #define SECRETS
    // application identifier - pre-LoRaWAN 1.1.0, this was called appEUI
    // when adding new end device in TTN, you will have to enter this number
    // you can pick any number you want, but it has to be unique
    uint64_t joinEUI = 0x12AD1011B0C0FFEE;
  
    // device identifier - this number can be anything
    // when adding new end device in TTN, you can generate this number,
    // or you can set any value you want, provided it is also unique
    uint64_t devEUI = 0x70B3D57ED005E120;
  
    // select some encryption keys which will be used to secure the communication
    // there are two of them - network key and application key
    // because LoRaWAN uses AES-128, the key MUST be 16 bytes (or characters) long
  
    // network key is the ASCII string "topSecretKey1234"
    uint8_t nwkKey[] = { 0x74, 0x6F, 0x70, 0x53, 0x65, 0x63, 0x72, 0x65,
                         0x74, 0x4B, 0x65, 0x79, 0x31, 0x32, 0x33, 0x34 };
  
    // application key is the ASCII string "aDifferentKeyABC"
    uint8_t appKey[] = { 0x61, 0x44, 0x69, 0x66, 0x66, 0x65, 0x72, 0x65,
                         0x6E, 0x74, 0x4B, 0x65, 0x79, 0x41, 0x42, 0x43 };
  
    // prior to LoRaWAN 1.1.0, only a single "nwkKey" is used
    // when connecting to LoRaWAN 1.0 network, "appKey" will be disregarded
    // and can be set to NULL
  #endif

  // now we can start the activation
  // this can take up to 10 seconds, and requires a LoRaWAN gateway in range
  // a specific starting-datarate can be selected in dynamic bands (e.g. EU868):
  /* 
    uint8_t joinDr = 4;
    state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey, joinDr);
  */
  Serial.print(F("[LoRaWAN] Attempting over-the-air activation ... "));
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

  if(state >= RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    delay(2000);	// small delay between joining and uplink
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(true);
  }

  Serial.print("[LoRaWAN] DevAddr: ");
  // TODO enable after RadioLib update
  //Serial.println(node.getDevAddr(), HEX);

  // on EEPROM-enabled boards, after the device has been activated,
  // the session can be restored without rejoining after device power cycle
  // this is intrinsically done when calling `beginOTAA()` with the same keys
  // or if you 'lost' the keys or don't want them included in your sketch
  // you can call `restore()`
  /*
    Serial.print(F("[LoRaWAN] Resuming previous session ... "));
    state = node.restore();
    if(state >= RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while(true);
    }
  */

  // disable the ADR algorithm
  node.setADR(false);

  // set a fixed datarate
  node.setDatarate(5);
  // in order to set the datarate persistent across reboot/deepsleep, use the following:
  /*
    node.setDatarate(5, true);  
  */

  // enable CSMA
  // this tries to minimize packet loss by searching for a free channel
  // before actually sending an uplink 
  node.setCSMA(6, 2, true);

  // enable or disable the dutycycle
  // the second argument specific allowed airtime per hour in milliseconds
  // 1250 = TTN FUP (30 seconds / 24 hours)
  // if not called, this corresponds to setDutyCycle(true, 0)
  // setting this to 0 corresponds to the band's maximum allowed dutycycle by law
  node.setDutyCycle(true, 1250);

  // enable or disable the dwell time limits
  // the second argument specifies the allowed airtime per uplink in milliseconds
  // unless specified, this argument is set to 0
  // setting this to 0 corresponds to the band's maximum allowed dwell time by law
  node.setDwellTime(true, 400);
}

void loop() {
  int state = RADIOLIB_ERR_NONE;

  // set battery fill level - the LoRaWAN network server
  // may periodically request this information
  // 0 = external power source
  // 1 = lowest (empty battery)
  // 254 = highest (full battery)
  // 255 = unable to measure
  // TODO: Add battery voltage measurement
  uint8_t battLevel = 146;
  node.setDeviceStatus(battLevel);

  // retrieve the last uplink frame counter
  uint32_t fcntUp = node.getFcntUp();

  Serial.print(F("[LoRaWAN] Sending uplink packet ... "));
  // String strUp = "Hello!" + String(fcntUp);
  
  // // send a confirmed uplink to port 10 every 64th frame
  // // and also request the LinkCheck and DeviceTime MAC commands
  // if(fcntUp % 64 == 0) {
  //   node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_LINK_CHECK);
  //   node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
  //   state = node.uplink(strUp, 10, true);
  // } else {
  //   state = node.uplink(strUp, 10);
  // }
  uint8_t port = 0;
  LoraEncoder encoder(loraData);
  #ifdef GEN_PAYLOAD
      gen_payload(port, encoder);
  #else
      get_payload(port, encoder);
  #endif
  state = node.uplink(loraData, encoder.getLength(), port);

  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // after uplink, you can call downlink(),
  // to receive any possible reply from the server
  // this function must be called within a few seconds
  // after uplink to receive the downlink!
  Serial.print(F("[LoRaWAN] Waiting for downlink ... "));
  String strDown;

  // you can also retrieve additional information about 
  // uplink or downlink by passing a reference to
  // LoRaWANEvent_t structure
  LoRaWANEvent_t event;
  state = node.downlink(strDown, &event);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));

    // print data of the packet (if there are any)
    Serial.print(F("[LoRaWAN] Data:\t\t"));
    if(strDown.length() > 0) {
      Serial.println(strDown);
    } else {
      Serial.println(F("<MAC commands only>"));
    }

    // print RSSI (Received Signal Strength Indicator)
    Serial.print(F("[LoRaWAN] RSSI:\t\t"));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // print SNR (Signal-to-Noise Ratio)
    Serial.print(F("[LoRaWAN] SNR:\t\t"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    // print frequency error
    Serial.print(F("[LoRaWAN] Frequency error:\t"));
    Serial.print(radio.getFrequencyError());
    Serial.println(F(" Hz"));

    // print extra information about the event
    Serial.println(F("[LoRaWAN] Event information:"));
    Serial.print(F("[LoRaWAN] Direction:\t"));
    if(event.dir == RADIOLIB_LORAWAN_CHANNEL_DIR_UPLINK) {
      Serial.println(F("uplink"));
    } else {
      Serial.println(F("downlink"));
    }
    Serial.print(F("[LoRaWAN] Confirmed:\t"));
    Serial.println(event.confirmed);
    Serial.print(F("[LoRaWAN] Confirming:\t"));
    Serial.println(event.confirming);
    Serial.print(F("[LoRaWAN] Datarate:\t"));
    Serial.println(event.datarate);
    Serial.print(F("[LoRaWAN] Frequency:\t"));
    Serial.print(event.freq, 3);
    Serial.println(F(" MHz"));
    Serial.print(F("[LoRaWAN] Output power:\t"));
    Serial.print(event.power);
    Serial.println(F(" dBm"));
    Serial.print(F("[LoRaWAN] Frame count:\t"));
    Serial.println(event.fcnt);
    Serial.print(F("[LoRaWAN] Port:\t\t"));
    Serial.println(event.port);
    
    Serial.print(radio.getFrequencyError());

    uint8_t margin = 0;
    uint8_t gwCnt = 0;
    if(node.getMacLinkCheckAns(&margin, &gwCnt)) {
      Serial.print(F("[LoRaWAN] LinkCheck margin:\t"));
      Serial.println(margin);
      Serial.print(F("[LoRaWAN] LinkCheck count:\t"));
      Serial.println(gwCnt);
    }

    uint32_t networkTime = 0;
    uint8_t fracSecond = 0;
    if(node.getMacDeviceTimeAns(&networkTime, &fracSecond, true)) {
      Serial.print(F("[LoRaWAN] DeviceTime Unix:\t"));
      Serial.println(networkTime);
      Serial.print(F("[LoRaWAN] LinkCheck second:\t1/"));
      Serial.println(fracSecond);
    }
  
  } else if(state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println(F("timeout!"));
  
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // on EEPROM enabled boards, you can save the current session
  // by calling "saveSession" which allows retrieving the session after reboot or deepsleep
  node.saveSession();

  // wait before sending another packet
  uint32_t minimumDelay = 60000;                  // try to send once every minute
  uint32_t interval = node.timeUntilUplink();     // calculate minimum duty cycle delay (per law!)
	uint32_t delayMs = max(interval, minimumDelay); // cannot send faster than duty cycle allows

  delay(delayMs);
}
