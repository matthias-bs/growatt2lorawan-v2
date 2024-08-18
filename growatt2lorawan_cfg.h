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