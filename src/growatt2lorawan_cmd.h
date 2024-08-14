///////////////////////////////////////////////////////////////////////////////
// growatt2lorawan_cmd.h
//
// LoRaWAN Command Interface
//
// Definition of control/configuration commands and status responses for
// LoRaWAN network layer and application layer
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
// 20240721 Copied from BresserWeatherSensorLW project
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#if !defined(_LWCMD_H)
#define _LWCMD_H

#include <Arduino.h>
#include "growatt2lorawan_cfg.h"

// ===========================
// LoRaWAN command interface
// ===========================

// ---------------------------
// -- LoRaWAN network layer --
// ---------------------------

// CMD_GET_DATETIME
// -----------------
#define CMD_GET_DATETIME 0x20

// Downlink (command):
// byte0:   0x00

// Uplink (response):
// byte0: unixtime[31:24]
// byte1: unixtime[23:16]
// byte2: unixtime[15: 8]
// byte3: unixtime[ 7: 0]
// byte4: rtc_source[ 7: 0]


// CMD_SET_DATETIME
// -----------------
// Port: CMD_SET_DATETIME
#define CMD_SET_DATETIME 0x21

// Downlink (command):
// byte0: unixtime[31:24]
// byte1: unixtime[23:16]
// byte2: unixtime[15: 8]
// byte3: unixtime[ 7: 0]

// Uplink: n.a.

// CMD_SET_SLEEP_INTERVAL
// -----------------------
// Note: Set normal sleep interval in seconds
// Port: CMD_SET_SLEEP_INTERVAL
#define CMD_SET_SLEEP_INTERVAL 0x31

// Downlink (command):
// byte0: sleep_interval[15:8]
// byte1: sleep_interval[ 7:0]

// Response: n.a.

// CMD_SET_SLEEP_INTERVAL_LONG
// ----------------------------
// Note: Set long sleep interval in seconds (energy saving mode)
// Port: CMD_SET_SLEEP_INTERVAL_LONG
#define CMD_SET_SLEEP_INTERVAL_LONG 0x33

// Downlink (command):
// byte0: sleep_interval_long[15:8]
// byte1: sleep_interval_long[ 7:0]

// Uplink: n.a.

// CMD_GET_LW_CONFIG
// ------------------
// Port: CMD_GET_LW_CONFIG
#define CMD_GET_LW_CONFIG 0x36

// Downlink (command):
// byte0: 0x00

// Uplink (response):
// byte0: sleep_interval[15: 8]
// byte1: sleep_interval[ 7:0]
// byte2: sleep_interval_long[15:8]
// byte3: sleep_interval_long[ 7:0]

// CMD_GET_LW_STATUS
// ------------------
// Port: CMD_GET_LW_STATUS
// Note: Get LoRaWAN device status
#define CMD_GET_LW_STATUS 0x38

// Downlink (command):
// byte0: 0x00

// Uplink (response):
// byte0: u_batt[15:8]
// byte1: u_batt[ 7:0]
// byte2: flags[ 7:0]

// -----------------------
// -- Application layer --
// -----------------------

// CMD_GET_STATUS_INTERVAL
// ------------------------
// Note: Get status interval in frame counts
// Port: CMD_GET_STATUS_INTERVAL
#define CMD_GET_STATUS_INTERVAL 0x40

// Downlink (command):
// byte0: 0x00

// Uplink (response):
// byte0: status_interval[7:0]

// CMD_SET_STATUS_INTERVAL
// ------------------------
// Note: Set status interval in frame counts
// Port: CMD_SET_STATUS_INTERVAL
#define CMD_SET_STATUS_INTERVAL 0x41

// Downlink (command):
// byte0: status_interval[7:0]

// Uplink: n.a.

// ===========================

/*!
 * \brief Decode downlink
 *
 * \param port      downlink message port
 * \param payload   downlink message payload
 * \param size      downlink message size in bytes
 *
 * \returns command ID, if downlink message requests a response, otherwise 0
 */
uint8_t decodeDownlink(uint8_t port, uint8_t *payload, size_t size);

/*!
 * \brief Send configuration uplink
 *
 * \param uplinkRequest command ID of uplink request
 */
void sendCfgUplink(uint8_t uplinkReq, uint32_t uplinkInterval);

#endif