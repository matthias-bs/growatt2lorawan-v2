///////////////////////////////////////////////////////////////////////////////
// config.h
// 
// RadioLib / LoRaWAN specific configuration including radio module wiring
//
// based on https://github.com/radiolib-org/radiolib-persistence
//
// created: 04/2024
//
//
// MIT License
//
// Copyright (c) 2024 Matthias Prinke
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
// 20240721 Copied from BresserWeatherSensorLW project
// 20250307 Added customDelay()
// 20250308 Updated to RadioLib v7.1.2
//          Modified for optional use of LoRaWAN v1.0.4 (requires no nwkKey)
// 20250318 Added pinmap for ARDUINO_TTGO_LORA32_V2
// 20250530 Modified radio chip selection
//          Added pinmap for Lilygo T3S3 SX1262/SX1276/LR1121
//          Removed instances of radio and LoRaWANNode classes
// 20250622 Updated to RadioLib v7.2.0
//
// ToDo:
// - 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>
#include <RadioLib.h>
#include "secrets.h"

// How often to send an uplink - consider legal & FUP constraints - see notes
const uint32_t uplinkIntervalSeconds = 5UL * 60UL;    // minutes x seconds
#define LORAWAN_VERSION_1_1
//#define LORAWAN_VERSION_1_0_4

// JoinEUI (or AppEUI in previous versions of LoRaWAN, e.g. v1.0.4)
// for development purposes you can use all zeros - see RadioLib wiki for details
#ifndef RADIOLIB_LORAWAN_JOIN_EUI  // Replace with your Join EUI
#define RADIOLIB_LORAWAN_JOIN_EUI  0x---------------
#endif

// The Device EUI & two keys can be generated on the TTN console 
#ifndef RADIOLIB_LORAWAN_DEV_EUI   // Replace with your Device EUI
#define RADIOLIB_LORAWAN_DEV_EUI   0x---------------
#endif
#ifndef RADIOLIB_LORAWAN_APP_KEY   // Replace with your App Key 
#define RADIOLIB_LORAWAN_APP_KEY   0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x-- 
#endif
#ifdef LORAWAN_VERSION_1_1
#ifndef RADIOLIB_LORAWAN_NWK_KEY   // Put your Nwk Key here
#define RADIOLIB_LORAWAN_NWK_KEY   0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x-- 
#endif
#endif

// For the curious, the #ifndef blocks allow for automated testing &/or you can
// put your EUI & keys in to your platformio.ini - see wiki for more tips



// Regional choices: EU868, US915, AU915, AS923, IN865, KR920, CN780, CN500
const LoRaWANBand_t Region = EU868;
const uint8_t subBand = 0;  // For US915, change this to 2, otherwise leave on 0


// ============================================================================
// Below is to support the sketch - only make changes if the notes say so ...

// Auto select MCU <-> radio connections
// If you get an error message when compiling, it may be that the 
// pinmap could not be determined - see the notes for more info


// Adafruit
#if defined(ARDUINO_SAMD_FEATHER_M0)
    #pragma message ("Adafruit Feather M0 with RFM95")
    #pragma message ("Link required on board")
    SX1276 radio = new Module(8, 3, 4, 6);

#elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
  // https://wiki.dfrobot.com/FireBeetle_ESP32_IOT_Microcontroller(V3.0)__Supports_Wi-Fi_&_Bluetooth__SKU__DFR0478
  // https://wiki.dfrobot.com/FireBeetle_Covers_LoRa_Radio_868MHz_SKU_TEL0125
  #define PIN_LORA_NSS      27 // D4
  #define PIN_LORA_RST      25 // D2
  #define PIN_LORA_IRQ      26 // D3
  #define PIN_LORA_GPIO      9 // D5
  #define PIN_LORA_DIO2     RADIOLIB_NC
  #pragma message("ARDUINO_DFROBOT_FIREBEETLE_ESP32; assuming this is a FireBeetle ESP32 with FireBeetle Cover LoRa")
  #pragma message("Required wiring: D2 to RESET, D3 to DIO0, D4 to CS, D5 to DIO1")

  #define LORA_CHIP SX1276

// LilyGo 
#elif defined(ARDUINO_TTGO_LORA32_V1)
  #pragma message ("TTGO LoRa32 v1 - no Display")
  // https://github.com/espressif/arduino-esp32/blob/master/variants/ttgo-lora32-v1/pins_arduino.h
  // http://www.lilygo.cn/prod_view.aspx?TypeId=50003&Id=1130&FId=t3:50003:3
  // https://github.com/Xinyuan-LilyGo/TTGO-LoRa-Series
  // https://github.com/LilyGO/TTGO-LORA32/blob/master/schematic1in6.pdf
  #define PIN_LORA_NSS      LORA_CS
  #define PIN_LORA_RST      LORA_RST
  #define PIN_LORA_IRQ      LORA_IRQ
  #define PIN_LORA_GPIO     33
  #define PIN_LORA_DIO2     RADIOLIB_NC
  #define LORA_CHIP SX1276

#elif defined(ARDUINO_TTGO_LORA32_V2)
  // https://github.com/espressif/arduino-esp32/blob/master/variants/ttgo-lora32-v2/pins_arduino.h
  #pragma message ("Using TTGO LoRa32 V2")
  #define PIN_LORA_NSS      LORA_CS
  #define PIN_LORA_RST      LORA_RST
  #define PIN_LORA_IRQ      LORA_IRQ
  #define PIN_LORA_GPIO     RADIOLIB_NC
  #define PIN_LORA_DIO2     RADIOLIB_NC
  #define LORA_CHIP SX1276


#elif defined(ARDUINO_TTGO_LoRa32_v21new) // T3_V1.6.1
  #pragma message ("Using TTGO LoRa32 v2.1 marked T3_V1.6.1 + Display")
  #define PIN_LORA_NSS      LORA_CS
  #define PIN_LORA_RST      LORA_RST
  #define PIN_LORA_IRQ      LORA_IRQ
  #define PIN_LORA_GPIO     LORA_D1
  #define PIN_LORA_DIO2     RADIOLIB_NC
  #define LORA_CHIP SX1276

  #elif defined(ARDUINO_LILYGO_T3S3_SX1262)
  // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_sx1262/pins_arduino.h
  #pragma message("ARDUINO_LILYGO_T3S3_SX1262 defined; using on-board transceiver")
  #define PIN_LORA_NSS   LORA_CS
  #define PIN_LORA_IRQ  LORA_IRQ
  #define PIN_LORA_GPIO LORA_BUSY
  #define PIN_LORA_RST  LORA_RST
  #define LORA_CHIP SX1262

#elif defined(ARDUINO_LILYGO_T3S3_SX1276)
  // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_sx127x/pins_arduino.h
  #pragma message("ARDUINO_LILYGO_T3S3_SX1276 defined; using on-board transceiver")
  #define PIN_LORA_NSS   LORA_CS
  #define PIN_LORA_IRQ  LORA_IRQ
  #define PIN_LORA_GPIO LORA_BUSY
  #define PIN_LORA_RST  LORA_RST
  #define LORA_CHIP SX1276

#elif defined(ARDUINO_LILYGO_T3S3_LR1121)
  // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_lr1121/pins_arduino.h
  #pragma message("ARDUINO_LILYGO_T3S3_LR1121 defined; using on-board transceiver")
  #define PIN_LORA_NSS   LORA_CS
  #define PIN_LORA_IRQ  LORA_IRQ
  #define PIN_LORA_GPIO LORA_BUSY
  #define PIN_LORA_RST  LORA_RST
  #define LORA_CHIP LR1121

#elif defined(ARDUINO_TBEAM_USE_RADIO_SX1262)
  #pragma message ("Using LilyGo T-Beam with SX1262")
  #define PIN_LORA_NSS      LORA_CS
  #define PIN_LORA_RST      LORA_RST
  #define PIN_LORA_IRQ      LORA_IRQ
  #define PIN_LORA_GPIO     LORA_IO1
  #define PIN_LORA_DIO2     LORA_IO2
  #define LORA_CHIP SX1262

#elif defined(ARDUINO_TBEAM_USE_RADIO_SX1276)
  #pragma message ("Using LilyGo T-Beam with SX1276")
  #define PIN_LORA_NSS      LORA_CS
  #define PIN_LORA_RST      LORA_RST
  #define PIN_LORA_IRQ      LORA_IRQ
  #define PIN_LORA_GPIO     LORA_IO1
  #define PIN_LORA_DIO2     LORA_IO2
  #define LORA_CHIP SX1276

// Heltec
#elif defined(ARDUINO_HELTEC_WIFI_LORA_32)
  #pragma error ("ARDUINO_HELTEC_WIFI_LORA_32 awaiting pin map")

#elif defined(ARDUINO_heltec_wifi_kit_32_V2)
  #pragma message ("ARDUINO_heltec_wifi_kit_32_V2 TODO pin map")
  //SX1276 radio = new Module(18, 26, 14, 35);

#elif defined(ARDUINO_heltec_wifi_kit_32_V3)
  #pragma message ("Using Heltec WiFi LoRa32 v3 - Display + USB-C TODO pin map")
  //SX1262 radio = new Module(8, 14, 12, 13);

#elif defined(ARDUINO_CUBECELL_BOARD)
  #pragma message ("Using ARDUINO_CUBECELL_BOARD")

#elif defined(ARDUINO_CUBECELL_BOARD_V2)
  #pragma error ("ARDUINO_CUBECELL_BOARD_V2 awaiting pin map")


#else
  #pragma message ("Unknown board - no automagic pinmap available")

  // Using arbitrary settings for CI workflow with FQBN esp32:esp32:esp32
  // LoRaWAN_Node board
  // https://github.com/matthias-bs/LoRaWAN_Node
  #define PIN_LORA_NSS      14
  #define PIN_LORA_RST      12
  #define PIN_LORA_IRQ       4
  #define PIN_LORA_GPIO     16
  #define LORA_CHIP SX1276
  
  // SX1262  pin order: Module(NSS/CS, DIO1, RESET, BUSY);
  // SX1262 radio = new Module(8, 14, 12, 13);

  // SX1278 pin order: Module(NSS/CS, DIO0, RESET, DIO1);
  // SX1278 radio = new Module(10, 2, 9, 3);

#endif

// Copy over the EUI's & keys in to the something that will not compile if incorrectly formatted
uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
#ifdef LORAWAN_VERSION_1_1
uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };
#else
uint8_t nwkKey[] = { 0 };
#endif


// Custom delay function:
// Communication over LoRaWAN includes a lot of delays.
// By default, RadioLib will use the Arduino delay() function,
// which will waste a lot of power. However, you can put your
// microcontroller to sleep instead by customizing the function below,
// and providing it to RadioLib via "node.setSleepFunction".
// Note:
// Implementation from
// https://github.com/jgromes/RadioLib/discussions/1401#discussioncomment-12080631
#if defined(ESP32)
void customDelay(RadioLibTime_t ms) {
    uint64_t start = millis();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((ms-2)*(uint64_t)1000);
    esp_light_sleep_start();
    
    if(millis() < start + ms) {
	// if there's still some time left, do normal delay for remainder
        vTaskDelay((start + ms - millis())/portTICK_PERIOD_MS);
    }
}
#endif


// result code to text ...
String stateDecode(const int16_t result) {
  switch (result) {
  case RADIOLIB_ERR_NONE:
    return "ERR_NONE";
  case RADIOLIB_ERR_CHIP_NOT_FOUND:
    return "ERR_CHIP_NOT_FOUND";
  case RADIOLIB_ERR_PACKET_TOO_LONG:
    return "ERR_PACKET_TOO_LONG";
  case RADIOLIB_ERR_RX_TIMEOUT:
    return "ERR_RX_TIMEOUT";
  case RADIOLIB_ERR_MIC_MISMATCH:
    return "ERR_MIC_MISMATCH";
  case RADIOLIB_ERR_INVALID_BANDWIDTH:
    return "ERR_INVALID_BANDWIDTH";
  case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
    return "ERR_INVALID_SPREADING_FACTOR";
  case RADIOLIB_ERR_INVALID_CODING_RATE:
    return "ERR_INVALID_CODING_RATE";
  case RADIOLIB_ERR_INVALID_FREQUENCY:
    return "ERR_INVALID_FREQUENCY";
  case RADIOLIB_ERR_INVALID_OUTPUT_POWER:
    return "ERR_INVALID_OUTPUT_POWER";
  case RADIOLIB_ERR_NETWORK_NOT_JOINED:
	  return "RADIOLIB_ERR_NETWORK_NOT_JOINED";

  case RADIOLIB_ERR_DOWNLINK_MALFORMED:
    return "RADIOLIB_ERR_DOWNLINK_MALFORMED";
  case RADIOLIB_ERR_INVALID_REVISION:
    return "RADIOLIB_ERR_INVALID_REVISION";
  case RADIOLIB_ERR_INVALID_PORT:
    return "RADIOLIB_ERR_INVALID_PORT";
  case RADIOLIB_ERR_NO_RX_WINDOW:
    return "RADIOLIB_ERR_NO_RX_WINDOW";
  case RADIOLIB_ERR_INVALID_CID:
    return "RADIOLIB_ERR_INVALID_CID";
  case RADIOLIB_ERR_UPLINK_UNAVAILABLE:
    return "RADIOLIB_ERR_UPLINK_UNAVAILABLE";
  case RADIOLIB_ERR_COMMAND_QUEUE_FULL:
    return "RADIOLIB_ERR_COMMAND_QUEUE_FULL";
  case RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND:
    return "RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND";
  case RADIOLIB_ERR_JOIN_NONCE_INVALID:
    return "RADIOLIB_ERR_JOIN_NONCE_INVALID";
  case RADIOLIB_ERR_DWELL_TIME_EXCEEDED:
    return "RADIOLIB_ERR_DWELL_TIME_EXCEEDED";
  case RADIOLIB_ERR_CHECKSUM_MISMATCH:
    return "RADIOLIB_ERR_CHECKSUM_MISMATCH";
  case RADIOLIB_ERR_NO_JOIN_ACCEPT:
    return "RADIOLIB_ERR_NO_JOIN_ACCEPT";
  case RADIOLIB_LORAWAN_SESSION_RESTORED:
    return "RADIOLIB_LORAWAN_SESSION_RESTORED";
  case RADIOLIB_LORAWAN_NEW_SESSION:
    return "RADIOLIB_LORAWAN_NEW_SESSION";
  case RADIOLIB_ERR_NONCES_DISCARDED:
    return "RADIOLIB_ERR_NONCES_DISCARDED";
  case RADIOLIB_ERR_SESSION_DISCARDED:
    return "RADIOLIB_ERR_SESSION_DISCARDED";
  }
  return "See TypeDef.h";
}


// Helper function to display any issues
void debug(bool isFail, const char* message, int state, bool Freeze) {
  if (isFail) {
    log_w("%s - %s (%d)", message, stateDecode(state).c_str(), state);
    while (Freeze);
  }
}

// Helper function to display a byte array
void arrayDump(uint8_t *buffer, uint16_t len) {
  for (uint16_t c = 0; c < len; c++) {
    Serial.printf("%02X", buffer[c]);
  }
  Serial.println();
}

//#define STR_HELPER(x) #x
//#define STR(x) STR_HELPER(x)
//#pragma message("Radio chip: " STR(LORA_CHIP))
//#pragma message("Pin config: NSS->" STR(PIN_LORA_NSS) ", IRQ->" STR(PIN_LORA_IRQ) ", RST->" STR(PIN_LORA_RST) ", GPIO->" STR(PIN_LORA_GPIO) )

#endif
