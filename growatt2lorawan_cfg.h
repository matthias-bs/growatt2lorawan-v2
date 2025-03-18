///////////////////////////////////////////////////////////////////////////////
// growatt2lorawan_cfg.h
// 
// LoRaWAN node specific configuration
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
// 20240814 Created
// 20250316 Added EMULATE_SENSORS
//
// ToDo:
// - 
//
///////////////////////////////////////////////////////////////////////////////

#if !defined(_GROWATT2LORAWAN_CFG_H)
#define _GROWATT2LORAWAN_CFG_H

//
// User Configuration
//
//#define EMULATE_SENSORS

// Enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
#define TZINFO_STR "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"

// RTC to network time sync interval (in minutes)
#define CLOCK_SYNC_INTERVAL 24 * 60

// Number of uplink ports
#define NUM_PORTS 2

typedef struct
{
  int port;
  int mult;
} Schedule;

const Schedule UplinkSchedule[NUM_PORTS] = {
    // {port, mult}
    {1, 1},
    {2, 3}
};

// Maximum downlink payload size (bytes)
const uint8_t MAX_DOWNLINK_SIZE = 51;

// Minimum sleep interval (in seconds)
#define SLEEP_INTERVAL_MIN 60

// MCU will sleep for SLEEP_INTERVAL seconds after succesful transmission
#define SLEEP_INTERVAL 360

// Long sleep interval, MCU will sleep for SLEEP_INTERVAL_LONG seconds if battery voltage is below BATTERY_WEAK
#define SLEEP_INTERVAL_LONG 900

// LoRaWAN Node status message interval (in frames; 0 = disabled)
#define LW_STATUS_INTERVAL 0

// If battery voltage <= BATTERY_WEAK [mV], MCU will sleep for SLEEP_INTERVAL_LONG
// Go to sleep mode immediately after start if battery voltage <= BATTERY_LOW [mV]
#define BATTERY_WEAK 3500
#define BATTERY_LOW 3200

// Battery voltage limits in mV (usable range for the device) for battery state calculation
#define BATTERY_DISCHARGE_LIM 3200
#define BATTERY_CHARGE_LIM 4200


// Battery Voltage Measurement
//
// This is only needed if the board is powered from a battery when the main power supply
// (e.g. from PV inverter via USB) is not available.
//
// The battery voltage is used for
// - Battery protection (immediately go to sleep if u_batt < BATTERY_LOW)
// - Selection between normal/long sleep interval
//
// Prerequisites:
// - The board has a voltage measurement circuit
//   (e.g. a voltage divider between battery and ADC input)
// - The battery voltage measurement circuit is enabled
//   (e.g. by connecting the voltage divider to the battery and to the ACD input 
//   in case of the FireBeetle ESP32)

//#define EN_UBATT_MEASUREMENT

// ADC for supply/battery voltage measurement
// Defaults:
// ---------
// FireBeetle ESP32:            on-board connection to VB (with R10+R11 assembled)
// TTGO LoRa32:                 on-board connection to VBAT
// Adafruit Feather ESP32:      on-board connection to VBAT
// Heltec WiFi LoRa 32 V3:      on-board connection to VBAT, controlled by GPIO37
#if defined(ARDUINO_TTGO_LoRa32_V1) || defined(ARDUINO_TTGO_LoRa32_V2) || defined(ARDUINO_TTGO_LoRa32_v21new)
#define PIN_ADC_IN 35
#elif defined(ARDUINO_FEATHER_ESP32)
#define PIN_ADC_IN A13
#elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
#pragma message("On-board voltage divider must be enabled for battery voltage measurement (see schematic).")
// On-board VB
#define PIN_ADC_IN A0
#elif defined(ARDUINO_HELTEC_WIFI_LORA_32_V3)
// On-board VB
#define PIN_ADC_IN A0
#else
#pragma message("Unknown battery voltage measurement circuit.")
#pragma message("No power-saving & deep-discharge protection implemented yet.")
// unused
#define PIN_ADC_IN -1
#endif

// Voltage divider R1 / (R1 + R2) -> V_meas = V(R1 + R2); V_adc = V(R1)
#if defined(ARDUINO_THINGPULSE_EPULSE_FEATHER)
const float UBATT_DIV = 0.6812;
#elif defined(ARDUINO_HELTEC_WIFI_LORA_32_V3)
#define ADC_CTRL 37
// R17=100k, R14=390k => 100k / (100k + 390 k)
const float UBATT_DIV = 0.2041;
#else
const float UBATT_DIV = 0.5;
#endif
const uint8_t UBATT_SAMPLES = 10;

// Debug printing
// To enable debug mode (debug messages via serial port):
// Arduino IDE: Tools->Core Debug Level: "Debug|Verbose"
// or
// set CORE_DEBUG_LEVEL in this file
#define DEBUG_PORT Serial2


// Time source & status, see below
//
// bits 0..3 time source
//    0x00 = GPS
//    0x01 = RTC
//    0x02 = LORA
//    0x03 = unsynched
//    0x04 = set (source unknown)
//
// bits 4..7 esp32 sntp time status (not used)
enum class E_TIME_SOURCE : uint8_t
{
  E_GPS = 0x00,
  E_RTC = 0x01,
  E_LORA = 0x02,
  E_UNSYNCHED = 0x04,
  E_SET = 0x08
};

#endif
