///////////////////////////////////////////////////////////////////////////////
// growatt2lorawan-v2.ino
//
// LoRaWAN Node for Growatt PV-Inverter Data Interface
//
// - implements power saving by using the ESP32 deep sleep mode
// - implements fast re-joining after sleep by storing network session data
// - LoRa_Serialization is used for encoding various data types into bytes
// - internal Real-Time Clock (RTC) set from LoRaWAN network time
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
// RadioLib                             7.4.0
// LoRa_Serialization                   3.3.1
// ESP32Time                            2.0.6
//
//
// created: 07/2024
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
// 20240216 Created from matthias-bs/growatt2lorawan
// 20240814 Initial draft version
// 20240820 Fixed sleep time calculation
// 20240828 Renamed Preferences: BWS-LW to GRO2LW
// 20250307 Added custom sleep function for power saving between LoRaWAN transmissions
// 20250308 Updated to RadioLib v7.1.2
//          Added support for LoRaWAN v1.0.4
//          Synced with BresserWeatherSensorLW
// 20250316 Re-implemented uplink scheduling
// 20250318 Added battery voltage measurement
// 20250530 Added creation of radio module & LoRaWANNode objects
//          Added implementation for using Lilygo T3S3 SX1262/SX1276/LR1121
// 20250622 Updated to RadioLib v7.2.0
// 20251028 Updated to RadioLib v7.4.0
//
//
// Notes:
// - Set "Core Debug Level: Debug" for initial testing
// - The lines with "#pragma message()" in the compiler output are not errors, but useful hints!
// - The default LoRaWAN credentials are read at compile time from secrets.h (included in config.h),
//   they can be overriden by the JSON file secrets.json placed in LittleFS.
//   (Use https://github.com/earlephilhower/arduino-littlefs-upload for uploading.)
// - Pin mapping of radio transceiver module is done in config.h
// - Pin mapping of RS485 interface is done in growatt_cfg.h
// - For LoRaWAN Specification 1.1.0, a small set of data (the "nonces") have to be stored persistently -
//   this implementation uses Flash (via Preferences library)
// - Storing LoRaWAN network session information speeds up the connection (join) after a restart -
//   this implementation uses the ESP32's RTC RAM or a variable located in the RP2040's RAM, respectively.
//   In the latter case, an uninitialzed linker section is used for this purpose.
// - settimeofday()/gettimeofday() must be used to access the ESP32's RTC time
// - Arduino ESP32 package has built-in time zone handling, see
//   https://github.com/SensorsIot/NTP-time-for-ESP8266-and-ESP32/blob/master/NTP_Example/NTP_Example.ino
//
///////////////////////////////////////////////////////////////////////////////
/*! \file growatt2lorawan-v2.ino */

// include the library
#include <RadioLib.h>
#include <Preferences.h>
#include <ESP32Time.h>
#include <LoraMessage.h>
#include "config.h"
#include "growatt2lorawan_cfg.h"
#include "src/growatt_cfg.h"
#include "src/adc/adc.h"
#include "src/growatt2lorawan_cmd.h"
#include "src/AppLayer.h"
#include "src/LoadSecrets.h"
#include "src/UplinkScheduler.h"

/// Modbus interface select: 0 - USB / 1 - RS485
bool modbusRS485;

// Uplink message payload size
// The maximum allowed for all data rates is 51 bytes.
const uint8_t MAX_UPLINK_SIZE = 51;

/// Preferences (stored in flash memory)
Preferences preferences;

static Preferences store;

struct sPrefs
{
  uint16_t sleep_interval;      //!< preferences: sleep interval
  uint16_t sleep_interval_long; //!< preferences: sleep interval long
  uint8_t lw_stat_interval;     //!< preferences: LoRaWAN node status uplink interval
} prefs;


// Create radio object
LORA_CHIP radio = new Module(PIN_LORA_NSS, PIN_LORA_IRQ, PIN_LORA_RST, PIN_LORA_GPIO);

#if defined(ARDUINO_LILYGO_T3S3_SX1262) || defined(ARDUINO_LILYGO_T3S3_SX1276) || defined(ARDUINO_LILYGO_T3S3_LR1121)
SPIClass *spi = nullptr;
#endif

#if defined(ARDUINO_LILYGO_T3S3_LR1121)
static const uint32_t rfswitch_dio_pins[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
    // mode                  DIO5  DIO6
    { LR11x0::MODE_STBY,   { LOW,  LOW  } },
    { LR11x0::MODE_RX,     { HIGH, LOW  } },
    { LR11x0::MODE_TX,     { LOW,  HIGH } },
    { LR11x0::MODE_TX_HP,  { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF,  { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,   { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,   { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};
#endif // ARDUINO_LILYGO_T3S3_LR1121

// Time zone info
const char *TZ_INFO = TZINFO_STR;

// Variables which must retain their values after deep sleep
#if defined(ESP32)
// Stored in RTC RAM
RTC_DATA_ATTR bool longSleep;              //!< last sleep interval; 0 - normal / 1 - long
RTC_DATA_ATTR time_t rtcLastClockSync = 0; //!< timestamp of last RTC synchonization to network time

// utilities & vars to support ESP32 deep-sleep. The RTC_DATA_ATTR attribute
// puts these in to the RTC memory which is preserved during deep-sleep
RTC_DATA_ATTR uint16_t bootCount = 1;
RTC_DATA_ATTR uint16_t bootCountSinceUnsuccessfulJoin = 0;
RTC_DATA_ATTR E_TIME_SOURCE rtcTimeSource;
RTC_DATA_ATTR bool appStatusUplinkPending = false;
RTC_DATA_ATTR bool lwStatusUplinkPending = false;
RTC_DATA_ATTR uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];

#else
// Saved to/restored from Watchdog SCRATCH registers
bool longSleep;          //!< last sleep interval; 0 - normal / 1 - long
time_t rtcLastClockSync; //!< timestamp of last RTC synchonization to network time

// utilities & vars to support deep-sleep
// Saved to/restored from Watchdog SCRATCH registers
uint16_t bootCount;
uint16_t bootCountSinceUnsuccessfulJoin;

/// RP2040 RAM is preserved during sleep; we just have to ensure that it is not initialized at startup (after reset)
uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] __attribute__((section(".uninitialized_data")));

/// RTC time source
E_TIME_SOURCE rtcTimeSource __attribute__((section(".uninitialized_data")));

/// AppLayer status uplink pending
bool appStatusUplinkPending __attribute__((section(".uninitialized_data")));

/// LoRaWAN Node status uplink pending
bool lwStatusUplinkPending __attribute__((section(".uninitialized_data")));
#endif

/// Real time clock
ESP32Time rtc;

/// Application layer
AppLayer appLayer(&rtc, &rtcLastClockSync);

/// Uplink scheduler
UplinkScheduler uplinkScheduler;

#if defined(ESP32)
/*!
 * \brief Print wakeup reason (ESP32 only)
 *
 * Abbreviated version from the Arduino-ESP32 package, see
 * https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/deepsleep.html
 * for the complete set of options.
 */
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    log_i("Wake from sleep");
  }
  else
  {
    log_i("Wake not caused by deep sleep: %u", wakeup_reason);
  }
}
#endif


void uplinkDelay(uint32_t timeUntilUplink, uint32_t uplinkInterval)
{
  // wait before sending uplink
  uint32_t minimumDelay = uplinkInterval * 1000UL;
  // uint32_t interval = node.timeUntilUplink();     // calculate minimum duty cycle delay (per FUP & law!)
  uint32_t delayMs = max(timeUntilUplink, minimumDelay); // cannot send faster than duty cycle allows

  log_d("Sending uplink in %u s", delayMs / 1000);
#if defined(ESP32)
  esp_sleep_enable_timer_wakeup(delayMs * 1000);
  esp_light_sleep_start();
#else
  delay(delayMs);
#endif
}

/*!
 * \brief Compute sleep duration
 *
 * Minimum duration: SLEEP_INTERVAL_MIN
 * If battery voltage is available and <= BATTERY_WEAK:
 *   sleep_interval_long
 * else
 *   sleep_interval
 *
 * Additionally, the sleep interval is reduced from the
 * default value to achieve a wake-up time alinged to
 * an integer multiple of the interval after a full hour.
 *
 * \returns sleep duration in seconds
 */
uint32_t sleepDuration(uint16_t battery_weak)
{
  uint32_t sleep_interval = prefs.sleep_interval;
  longSleep = false;

  uint16_t voltage = getBatteryVoltage();
  // Long sleep interval if battery is weak
  if (voltage && voltage <= battery_weak)
  {
    sleep_interval = prefs.sleep_interval_long;
    longSleep = true;
  }

  // If the real time is available, align the wake-up time to the
  // to next non-fractional multiple of sleep_interval past the hour
  if (rtcLastClockSync)
  {
    struct tm timeinfo;
    time_t t_now = rtc.getLocalEpoch();
    localtime_r(&t_now, &timeinfo);

    uint32_t diff = (timeinfo.tm_min * 60) % sleep_interval + timeinfo.tm_sec;
    if (diff > sleep_interval)
    {
      diff -= sleep_interval;
    }
    sleep_interval = sleep_interval - diff;
  }

  sleep_interval = max(sleep_interval, static_cast<uint32_t>(SLEEP_INTERVAL_MIN));
  return sleep_interval;
}

#if defined(ESP32)
/*!
 * \brief Enter sleep mode (ESP32 variant)
 *
 *  ESP32 deep sleep mode
 *
 * \param seconds sleep duration in seconds
 */
void gotoSleep(uint32_t seconds)
{
  log_i("Sleeping for %lu s", seconds);
  esp_sleep_enable_timer_wakeup(seconds * 1000UL * 1000UL); // function uses uS
  Serial.flush();

  esp_deep_sleep_start();

  // if this appears in the serial debug, we didn't go to sleep!
  // so take defensive action so we don't continually uplink
  log_w("\n\n### Sleep failed, delay of 5 minutes & then restart ###");
  delay(5UL * 60UL * 1000UL);
  ESP.restart();
}

#else
/*!
 * \brief Enter sleep mode (RP2040 variant)
 *
 * \param seconds sleep duration in seconds
 */
void gotoSleep(uint32_t seconds)
{
  log_i("Sleeping for %lu s", seconds);
  time_t t_now = rtc.getLocalEpoch();
  datetime_t dt;
  epoch_to_datetime(&t_now, &dt);
  rtc_set_datetime(&dt);
  sleep_us(64);
  pico_sleep(seconds);

  // Save variables to be retained after reset
  watchdog_hw->scratch[3] = (bootCountSinceUnsuccessfulJoin << 16) | bootCount;
  watchdog_hw->scratch[2] = rtcLastClockSync;

  if (longSleep)
  {
    watchdog_hw->scratch[1] |= 2;
  }
  else
  {
    watchdog_hw->scratch[1] &= ~2;
  }
  // Save the current time, because RTC will be reset (SIC!)
  rtc_get_datetime(&dt);
  time_t now = datetime_to_epoch(&dt, NULL);
  watchdog_hw->scratch[0] = now;
  log_i("Now: %llu", now);

  rp2040.restart();
}
#endif

/// Print date and time (i.e. local time)
void printDateTime(void)
{
  struct tm timeinfo;
  char tbuf[25];

  time_t tnow = rtc.getLocalEpoch();
  localtime_r(&tnow, &timeinfo);
  strftime(tbuf, 25, "%Y-%m-%d %H:%M:%S", &timeinfo);
  log_i("%s", tbuf);
}

/*!
 * \brief Activate node by restoring session or otherwise joining the network
 *
 * \return RADIOLIB_LORAWAN_NEW_SESSION or RADIOLIB_LORAWAN_SESSION_RESTORED
 */
int16_t lwActivate(LoRaWANNode &node)
{
  // setup the OTAA session information
#if defined(LORAWAN_VERSION_1_1)
  int16_t state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
#elif defined(LORAWAN_VERSION_1_0_4)
  int16_t state = node.beginOTAA(joinEUI, devEUI, nullptr, appKey);
#else
#error "LoRaWAN version not defined"
#endif

  debug(state != RADIOLIB_ERR_NONE, "Initialise node failed", state, true);

  log_d("Recalling LoRaWAN nonces & session");
  // ##### setup the flash storage
  store.begin("radiolib");
  // ##### if we have previously saved nonces, restore them and try to restore session as well
  if (store.isKey("nonces"))
  {
    uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];                   // create somewhere to store nonces
    store.getBytes("nonces", buffer, RADIOLIB_LORAWAN_NONCES_BUF_SIZE); // get them from the store
    state = node.setBufferNonces(buffer);                               // send them to LoRaWAN
    debug(state != RADIOLIB_ERR_NONE, "Restoring nonces buffer failed", state, false);

    // recall session from RTC deep-sleep preserved variable
    state = node.setBufferSession(LWsession); // send them to LoRaWAN stack

    // if we have booted more than once we should have a session to restore, so report any failure
    // otherwise no point saying there's been a failure when it was bound to fail with an empty LWsession var.
    debug((state != RADIOLIB_ERR_NONE) && (bootCount > 1), "Restoring session buffer failed", state, false);

    // if Nonces and Session restored successfully, activation is just a formality
    // moreover, Nonces didn't change so no need to re-save them
    if (state == RADIOLIB_ERR_NONE)
    {
      log_d("Succesfully restored session - now activating");
      state = node.activateOTAA();
      debug((state != RADIOLIB_LORAWAN_SESSION_RESTORED), "Failed to activate restored session", state, true);

      // ##### close the store before returning
      store.end();
      return (state);
    }
  }
  else
  { // store has no key "nonces"
    log_d("No Nonces saved - starting fresh.");
  }

  // if we got here, there was no session to restore, so start trying to join
  state = RADIOLIB_ERR_NETWORK_NOT_JOINED;
  while (state != RADIOLIB_LORAWAN_NEW_SESSION)
  {
    log_i("Join ('login') to the LoRaWAN Network");
    state = node.activateOTAA();

    // ##### save the join counters (nonces) to permanent store
    log_d("Saving nonces to flash");
    uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];                   // create somewhere to store nonces
    uint8_t *persist = node.getBufferNonces();                          // get pointer to nonces
    memcpy(buffer, persist, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);          // copy in to buffer
    store.putBytes("nonces", buffer, RADIOLIB_LORAWAN_NONCES_BUF_SIZE); // send them to the store

    // we'll save the session after an uplink

    if (state != RADIOLIB_LORAWAN_NEW_SESSION)
    {
      log_i("Join failed: %d", state);

      // how long to wait before join attempts. This is an interim solution pending
      // implementation of TS001 LoRaWAN Specification section #7 - this doc applies to v1.0.4 & v1.1
      // it sleeps for longer & longer durations to give time for any gateway issues to resolve
      // or whatever is interfering with the device <-> gateway airwaves.
      uint32_t sleepForSeconds = min((bootCountSinceUnsuccessfulJoin++ + 1UL) * 60UL, 3UL * 60UL);
      log_i("Boots since unsuccessful join: %u", bootCountSinceUnsuccessfulJoin);
      log_i("Retrying join in %u seconds", sleepForSeconds);

      gotoSleep(sleepForSeconds);

    } // if activateOTAA state
  } // while join

  log_i("Joined");

  // reset the failed join count
  bootCountSinceUnsuccessfulJoin = 0;

  delay(1000); // hold off off hitting the airwaves again too soon - an issue in the US

  // ##### close the store
  store.end();
  return (state);
}

// setup & execute all device functions ...
void setup()
{
  String timeZoneInfo(TZ_INFO);
  uint16_t battery_weak = BATTERY_WEAK;
  uint16_t battery_low = BATTERY_LOW;
  uint16_t battery_discharge_lim = BATTERY_DISCHARGE_LIM;
  uint16_t battery_charge_lim = BATTERY_CHARGE_LIM;

  pinMode(INTERFACE_SEL, INPUT_PULLUP);
  modbusRS485 = digitalRead(INTERFACE_SEL);

  // set baud rate
  if (modbusRS485)
  {
    Serial.setDebugOutput(false);
    DEBUG_PORT.begin(115200, SERIAL_8N1, DEBUG_RX, DEBUG_TX);
    DEBUG_PORT.setDebugOutput(true);
    log_d("Modbus interface: USB");
  }

  //  Set time zone
  setenv("TZ", TZ_INFO, 1);
  printDateTime();

#if defined(ARDUINO_ARCH_RP2040)
  // see pico-sdk/src/rp2_common/hardware_rtc/rtc.c
  rtc_init();

  // Restore variables and RTC after reset
  time_t time_saved = watchdog_hw->scratch[0];
  datetime_t dt;
  epoch_to_datetime(&time_saved, &dt);

  // Set HW clock (only used in sleep mode)
  rtc_set_datetime(&dt);

  // Set SW clock
  rtc.setTime(time_saved);

  longSleep = ((watchdog_hw->scratch[1] & 2) == 2);
  rtcLastClockSync = watchdog_hw->scratch[2];
  bootCount = watchdog_hw->scratch[3] & 0xFFFF;
  if (bootCount == 0)
  {
    bootCount = 1;
  }
  bootCountSinceUnsuccessfulJoin = watchdog_hw->scratch[3] >> 16;
#else
  print_wakeup_reason();
#endif
  log_i("Boot count: %u", bootCount);

  if (bootCount == 1)
  {
    rtcTimeSource = E_TIME_SOURCE::E_UNSYNCHED;
    appStatusUplinkPending = false;
    lwStatusUplinkPending = false;
  }
  bootCount++;

  // Set time zone
  setenv("TZ", timeZoneInfo.c_str(), 1);
  printDateTime();

  // Try to load LoRaWAN secrets from LittleFS file, if available
#ifdef LORAWAN_VERSION_1_1
  bool requireNwkKey = true;
#else
  bool requireNwkKey = false;
#endif
  loadSecrets(requireNwkKey, joinEUI, devEUI, nwkKey, appKey);

  log_d("JoinEUI: %016llX", joinEUI);
  log_d("DevEUI:  %016llX", devEUI);
  log_d("AppKey: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X",
        appKey[0], appKey[1], appKey[2], appKey[3], appKey[4], appKey[5],
        appKey[6], appKey[7], appKey[8], appKey[9], appKey[10], appKey[11],
        appKey[12], appKey[13], appKey[14], appKey[15]);

  log_d("NwkKey: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X",
        nwkKey[0], nwkKey[1], nwkKey[2], nwkKey[3],
        nwkKey[4], nwkKey[5], nwkKey[6], nwkKey[7],
        nwkKey[8], nwkKey[9], nwkKey[10], nwkKey[11],
        nwkKey[12], nwkKey[13], nwkKey[14], nwkKey[15]);

  preferences.begin("GRO2LW", false);
  prefs.sleep_interval = preferences.getUShort("sleep_int", SLEEP_INTERVAL);
  log_d("Preferences: sleep_interval:        %u s", prefs.sleep_interval);
  prefs.sleep_interval_long = preferences.getUShort("sleep_int_long", SLEEP_INTERVAL_LONG);
  log_d("Preferences: sleep_interval_long:   %u s", prefs.sleep_interval_long);
  prefs.lw_stat_interval = preferences.getUChar("lw_stat_int", LW_STATUS_INTERVAL);
  log_d("Preferences: lw_stat_interval:      %u cycles", prefs.lw_stat_interval);
  preferences.end();

  uint16_t voltage = getBatteryVoltage();
  if (voltage && voltage <= battery_low)
  {
    log_i("Battery low!");
    gotoSleep(sleepDuration(battery_weak));
  }

  // Initialize Application Layer - starts sensor reception
  appLayer.begin();

  // build payload byte array (+ reserve to prevent overflow with configuration at run-time)
  uint8_t uplinkPayload[MAX_UPLINK_SIZE + 8];

  LoraEncoder encoder(uplinkPayload);

  // Get payload before starting LoRaWAN - not used here
  uint8_t fPort = 1;
  appLayer.getPayloadStage1(fPort, encoder);

  int16_t state = 0; // return value for calls to RadioLib

  #if defined(ARDUINO_LILYGO_T3S3_SX1262) || defined(ARDUINO_LILYGO_T3S3_SX1276) || defined(ARDUINO_LILYGO_T3S3_LR1121)
  // Use local radio object with custom SPI configuration
  spi = new SPIClass(SPI);
  spi->begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  radio = new Module(PIN_LORA_NSS, PIN_LORA_IRQ, PIN_LORA_RST, PIN_LORA_GPIO, *spi);
  #elif defined(ARDUINO_CUBECELL_BOARD)
  SX1262 radio = new Module(RADIOLIB_BUILTIN_MODULE);
  #endif

  // setup the radio based on the pinmap (connections) in config.h
  radio.reset();
  LoRaWANNode node(&radio, &Region, subBand);

  log_v("Initalise the radio");
  state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, "Initalise radio failed", state, true);

#if defined(ESP32)
  // Optionally provide a custom sleep function - see config.h
  node.setSleepFunction(customDelay);
#endif

  // activate node by restoring session or otherwise joining the network
  state = lwActivate(node);
  // state is one of RADIOLIB_LORAWAN_NEW_SESSION or RADIOLIB_LORAWAN_SESSION_RESTORED

  // Set battery fill level -
  // the LoRaWAN network server may periodically request this information
  // 0 = external power source
  // 1 = lowest (empty battery)
  // 254 = highest (full battery)
  // 255 = unable to measure
  uint8_t battLevel;
  if (voltage == 0)
  {
    battLevel = 255;
  }
  else if (voltage > battery_charge_lim)
  {
    battLevel = 0;
  }
  else
  {
    battLevel = static_cast<uint8_t>(
        static_cast<float>(voltage - battery_discharge_lim) / static_cast<float>(battery_charge_lim - battery_discharge_lim) * 255);
    battLevel = (battLevel == 0) ? 1 : battLevel;
    battLevel = (battLevel == 255) ? 254 : battLevel;
  }
  log_d("Battery level: %u", battLevel);
  node.setDeviceStatus(battLevel);

  // Check if clock was never synchronized or sync interval has expired
  if ((rtcLastClockSync == 0) || ((rtc.getLocalEpoch() - rtcLastClockSync) > (CLOCK_SYNC_INTERVAL * 60)))
  {
    log_i("RTC sync required");
    node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
  }

  uint8_t downlinkPayload[MAX_DOWNLINK_SIZE]; // Make sure this fits your plans!
  size_t downlinkSize;                        // To hold the actual payload size rec'd

  enum class E_FSM_STAGE : uint8_t
  {
    E_SENSORDATA = 0x00,
    E_RESPONSE = 0x01,
    E_LWSTATUS = 0x02,
    E_APPSTATUS = 0x03,
    E_DONE = 0x04
  };

  E_FSM_STAGE fsmStage = E_FSM_STAGE::E_SENSORDATA;

  /// Uplink request - command received via downlink
  uint8_t uplinkReq = 0;

  uplinkScheduler.begin(bootCount);

  do
  {
    // Retrieve the last uplink frame counter
    uint32_t fCntUp = node.getFCntUp();
    log_d("FcntUp: %u", node.getFCntUp());

    bool isConfirmed = false;

    // Send a confirmed uplink every 64th frame
    // and also request the LinkCheck command
    if (fCntUp && (fCntUp % 64 == 0))
    {
      log_i("[LoRaWAN] Requesting LinkCheck");
      node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_LINK_CHECK);
      isConfirmed = true;
    }

    // Set appStatusUplink flag if required
    uint8_t appStatusUplinkInterval = appLayer.getAppStatusUplinkInterval();
    if (appStatusUplinkInterval && (fCntUp % appStatusUplinkInterval == 0))
    {
      appStatusUplinkPending = true;
      log_i("App status uplink pending");
    }

    // Set lwStatusUplink flag if required
    if (prefs.lw_stat_interval && (fCntUp % prefs.lw_stat_interval == 0))
    {
      lwStatusUplinkPending = true;
      log_i("LoRaWAN node status uplink pending");
    }

    /// Uplink size in bytes
    uint8_t uplinkSize;

    if (fsmStage == E_FSM_STAGE::E_SENSORDATA)
    {
      log_d("Sending data uplink.");
      fPort = uplinkScheduler.getNextPort();
      LoraEncoder encoder(uplinkPayload);

#if defined(EMULATE_SENSORS)
      appLayer.genPayload(fPort, encoder);
#else
      // get payload immediately before uplink
      appLayer.getPayloadStage2(fPort, encoder);
#endif

      uplinkSize = encoder.getLength();
      uint8_t maxPayloadLen = node.getMaxPayloadLen();
      log_d("Max payload length: %u", maxPayloadLen);
      if (uplinkSize > maxPayloadLen)
      {
        log_w("Payload size exceeds maximum of %u bytes - truncating", maxPayloadLen);
        uplinkSize = maxPayloadLen;
      }
    }
    else if (fsmStage == E_FSM_STAGE::E_RESPONSE)
    {
      log_d("Sending response uplink.");
      fPort = uplinkReq;
      encodeCfgUplink(fPort, uplinkPayload, uplinkSize);
      uplinkDelay(node.timeUntilUplink(), 0);
    }
    else if (fsmStage == E_FSM_STAGE::E_LWSTATUS)
    {
      log_d("Sending LoRaWAN status uplink.");
      fPort = CMD_GET_LW_STATUS;
      encodeCfgUplink(fPort, uplinkPayload, uplinkSize);
      uplinkDelay(node.timeUntilUplink(), 0);
      lwStatusUplinkPending = false;
    }
    // else if (fsmStage == E_FSM_STAGE::E_APPSTATUS)
    // {
    //   log_d("Sending application status uplink.");
    //   fPort = CMD_GET_SENSORS_STAT;
    //   encodeCfgUplink(fPort, uplinkPayload, uplinkSize);
    //   uplinkDelay(node.timeUntilUplink(), uplinkIntervalSeconds);
    //   appStatusUplinkPending = false;
    // }

    LoRaWANEvent_t downlinkDetails;

    log_i("Sending uplink; port %u, size %u", fPort, uplinkSize);

    state = node.sendReceive(
        uplinkPayload,
        uplinkSize,
        fPort,
        downlinkPayload,
        &downlinkSize,
        isConfirmed,
        nullptr,
        &downlinkDetails);
    debug(state < RADIOLIB_ERR_NONE, "Error in sendReceive", state, false);

    uplinkReq = 0;

    // Check if downlink was received
    // (state 0 = no downlink, state 1/2 = downlink in window Rx1/Rx2)
    if (state > 0)
    {
      // Did we get a downlink with data for us
      if (downlinkSize > 0)
      {
        log_i("Downlink port %u, data: ", downlinkDetails.fPort);
        arrayDump(downlinkPayload, downlinkSize);

        if (downlinkDetails.fPort > 0)
        {
          uplinkReq = decodeDownlink(downlinkDetails.fPort, downlinkPayload, downlinkSize);
        }
      }
      else
      {
        log_d("<MAC commands only>");
      }

      // print RSSI (Received Signal Strength Indicator)
      log_d("[LoRaWAN] RSSI:\t\t%f dBm", radio.getRSSI());

      // print SNR (Signal-to-Noise Ratio)
      log_d("[LoRaWAN] SNR:\t\t%f dB", radio.getSNR());

      // print frequency error
      log_d("[LoRaWAN] Frequency error:\t%f Hz", radio.getFrequencyError());

      // print extra information about the event
      log_d("[LoRaWAN] Event information:");
      log_d("[LoRaWAN] Confirmed:\t%d", downlinkDetails.confirmed);
      log_d("[LoRaWAN] Confirming:\t%d", downlinkDetails.confirming);
      log_d("[LoRaWAN] Datarate:\t%d", downlinkDetails.datarate);
      log_d("[LoRaWAN] Frequency:\t%7.3f MHz", downlinkDetails.freq);
      log_d("[LoRaWAN] Output power:\t%d dBm", downlinkDetails.power);
      log_d("[LoRaWAN] Frame count:\t%u", downlinkDetails.fCnt);
      log_d("[LoRaWAN] fPort:\t\t%u", downlinkDetails.fPort);
      log_d("[LoRaWAN] Time-on-air: \t%u ms", node.getLastToA());
      log_d("[LoRaWAN] Rx window: %d", state);
    }

    uint32_t networkTime = 0;
    uint16_t milliseconds = 0;
    if (node.getMacDeviceTimeAns(&networkTime, &milliseconds, true) == RADIOLIB_ERR_NONE)
    {
      log_i("[LoRaWAN] DeviceTime Unix:\t %ld", networkTime);
      log_i("[LoRaWAN] DeviceTime frac:\t%u ms", milliseconds);

      // Update the system time with the time read from the network
      rtc.setTime(networkTime);

      // Save clock sync timestamp and clear flag
      rtcLastClockSync = rtc.getLocalEpoch();
      rtcTimeSource = E_TIME_SOURCE::E_LORA;
      log_d("RTC sync completed");
      printDateTime();
    }

    uint8_t margin = 0;
    uint8_t gwCnt = 0;
    if (node.getMacLinkCheckAns(&margin, &gwCnt) == RADIOLIB_ERR_NONE)
    {
      log_d("[LoRaWAN] LinkCheck margin:\t%d", margin);
      log_d("[LoRaWAN] LinkCheck count:\t%u", gwCnt);
    }

    if (uplinkReq)
    {
      // fsmStage == E_FSM_STAGE::E_SENSORDATA
      fsmStage = E_FSM_STAGE::E_RESPONSE;
    }
    else if (uplinkScheduler.hasUplinks())
    {
      // fsmStage == E_FSM_STAGE::E_SENSORDATA ||
      // fsmStage == E_FSM_STAGE::E_RESPONSE
      fsmStage = E_FSM_STAGE::E_SENSORDATA;
      uplinkDelay(node.timeUntilUplink(), 0);
    }
    else if (lwStatusUplinkPending)
    {
      fsmStage = E_FSM_STAGE::E_LWSTATUS;
    }
    // else if (appStatusUplinkPending)
    // {
    //   fsmStage = E_FSM_STAGE::E_APPSTATUS;
    // }
    else
    {
      fsmStage = E_FSM_STAGE::E_DONE;
    }
  } while (fsmStage != E_FSM_STAGE::E_DONE);

  // now save session to RTC memory
  uint8_t *persist = node.getBufferSession();
  memcpy(LWsession, persist, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

  // wait until next uplink - observing legal & TTN Fair Use Policy constraints
  gotoSleep(sleepDuration(battery_weak));
}

// The ESP32 wakes from deep-sleep and starts from the very beginning.
// It then goes back to sleep, so loop() is never called and which is
// why it is empty.

void loop() {}
