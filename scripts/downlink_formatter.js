///////////////////////////////////////////////////////////////////////////////
// downlink_formatter.js
// 
// LoRaWAN Node for Growatt Photovoltaic Inverter Modbus Data Interface
// based on ESP32 and SX1262/SX1276 - 
// sends data to a LoRaWAN network (e.g. The Things Network)
//
// This script allows to send downlink data to the The Things Network using
// parameters encoded in JSON format.
// The FPort must be set to the command.
//
/// Commands:
// ----------
// (see constants for ports below)
// port = CMD_SET_SLEEP_INTERVAL, {"sleep_interval": <interval_in_seconds>}
// port = CMD_SET_SLEEP_INTERVAL_LONG, {"sleep_interval_long": <interval_in_seconds>}
// port = CMD_SET_LW_STATUS_INTERVAL, {"lw_status_interval": <interval_in_uplink_frames>}
// port = CMD_GET_DATETIME, {"cmd": "CMD_GET_DATETIME"} / payload = 0x00
// port = CMD_SET_DATETIME, {"epoch": <epoch>}
// port = CMD_GET_LW_CONFIG, {"cmd": "CMD_GET_LW_CONFIG"} / payload = 0x00

// Responses:
// -----------
// (The response uses the same port as the request.)
// CMD_GET_LW_CONFIG {"sleep_interval": <sleep_interval>,
//                    "sleep_interval_long": <sleep_interval_long>}
// 
// CMD_GET_DATETIME {"epoch": <unix_epoch_time>, "rtc_source": <rtc_source>}
//
// CMD_GET_WS_TIMEOUT {"ws_timeout": <ws_timeout>}
//
//
// <sleep_interval>     : 0...65535
// <sleep_interval>     : 0...65535
// <lw_status_interval> : 0...255
// <epoch>              : unix epoch time, see https://www.epochconverter.com/ (<integer> / "0x....")
// <rtc_source>         : 0x00: GPS / 0x01: RTC / 0x02: LORA / 0x03: unsynched / 0x04: set (source unknown)
//
//
// Based on:
// ---------
// https://www.thethingsindustries.com/docs/integrations/payload-formatters/javascript/downlink/
//
// created: 08/2023
//
//
// MIT License
//
// Copyright (c) 2023 Matthias Prinke
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
// 20240818 Copied from BresserWeatherSensorLW and adapted for growatt2lorawan
//
// ToDo:
// -  
//
///////////////////////////////////////////////////////////////////////////////

// Commands
const CMD_GET_DATETIME = 0x20;
const CMD_SET_DATETIME = 0x21;
const CMD_SET_SLEEP_INTERVAL = 0x31;
const CMD_SET_SLEEP_INTERVAL_LONG = 0x33;
const CMD_SET_LW_STATUS_INTERVAL = 0x35;
const CMD_GET_LW_CONFIG = 0x36;
const CMD_GET_LW_STATUS = 0x38;


// Source of Real Time Clock setting
var rtc_source_code = {
    0x00: "GPS",
    0x01: "RTC",
    0x02: "LORA",
    0x03: "unsynched",
    0x04: "set (source unknown)"
};

function bytesToInt(bytes) {
    var i = 0;
    for (var x = 0; x < bytes.length; x++) {
        i |= +(bytes[x] << (x * 8));
    }
    return i;
}

// Big Endian
function bytesToIntBE(bytes) {
    var i = 0;
    for (var x = 0; x < bytes.length; x++) {
        i |= +(bytes[x] << ((bytes.length - 1 - x) * 8));
    }
    return i;
}

function uint8(bytes) {
    if (bytes.length !== 1) {
        throw new Error('uint8 must have exactly 1 byte');
    }
    return bytesToInt(bytes);
}

function uint16BE(bytes) {
    if (bytes.length !== 2) {
        throw new Error('uint16BE must have exactly 2 bytes');
    }
    return bytesToIntBE(bytes);
}

function uint32BE(bytes) {
    if (bytes.length !== 4) {
        throw new Error('uint32BE must have exactly 4 bytes');
    }
    return bytesToIntBE(bytes);
}

function hex16(bytes) {
    let res = "0x" + byte2hex(bytes[0]) + byte2hex(bytes[1]);
    return res;
}

function hex32(bytes) {
    let res = "0x" + byte2hex(bytes[0]) + byte2hex(bytes[1]) + byte2hex(bytes[2]) + byte2hex(bytes[3]);
    return res;
}

function byte2hex(byte) {
    return ('0' + byte.toString(16)).slice(-2);
}

function mac48(bytes) {
    var res = [];
    var j = 0;
    for (var i = 0; i < bytes.length; i += 6) {
        res[j++] = byte2hex(bytes[i]) + ":" + byte2hex(bytes[i + 1]) + ":" + byte2hex(bytes[i + 2]) + ":" +
            byte2hex(bytes[i + 3]) + ":" + byte2hex(bytes[i + 4]) + ":" + byte2hex(bytes[i + 5]);
    }
    return res;
}

function id32(bytes) {
    var res = [];
    var j = 0;
    for (var i = 0; i < bytes.length; i += 4) {
        res[j++] = "0x" + byte2hex(bytes[i]) + byte2hex(bytes[i + 1]) + byte2hex(bytes[i + 2]) + byte2hex(bytes[i + 3]);
    }
    return res;
}

function bresser_bitmaps(bytes) {
    let res = [];
    res[0] = "0x" + byte2hex(bytes[0]);
    for (var i = 1; i < 16; i++) {
        res[i] = "0x" + byte2hex(bytes[i]);
    }
    return res;
}

// Encode Downlink from JSON to bytes
function encodeDownlink(input) {
    var value;
    if (input.data.hasOwnProperty('cmd')) {
        if (input.data.cmd == "CMD_GET_DATETIME") {
            return {
                bytes: [0],
                fPort: CMD_GET_DATETIME,
                warnings: [],
                errors: []
            };
        }
        else if (input.data.cmd == "CMD_GET_LW_CONFIG") {
            return {
                bytes: [0],
                fPort: CMD_GET_LW_CONFIG,
                warnings: [],
                errors: []
            };
        }
        else if (input.data.cmd == "CMD_GET_LW_STATUS") {
            return {
                bytes: [0],
                fPort: CMD_GET_LW_STATUS,
                warnings: [],
                errors: []
            };
        }
    }
    if (input.data.hasOwnProperty('sleep_interval')) {
        return {
            bytes: [
                input.data.sleep_interval >> 8,
                input.data.sleep_interval & 0xFF
            ],
            fPort: CMD_SET_SLEEP_INTERVAL,
            warnings: [],
            errors: []
        };
    }
    if (input.data.hasOwnProperty('sleep_interval_long')) {
        return {
            bytes: [
                input.data.sleep_interval_long >> 8,
                input.data.sleep_interval_long & 0xFF
            ],
            fPort: CMD_SET_SLEEP_INTERVAL_LONG,
            warnings: [],
            errors: []
        };
    }
    else if (input.data.hasOwnProperty('lw_status_interval')) {
        return {
            bytes: [input.data.lw_status_interval],
            fPort: CMD_SET_LW_STATUS_INTERVAL,
            warnings: [],
            errors: []
        };
    }
    else if (input.data.hasOwnProperty('epoch')) {
        if (typeof input.data.epoch == "string") {
            if (input.data.epoch.substr(0, 2) == "0x") {
                value = parseInt(input.data.epoch.substr(2), 16);
            } else {
                return {
                    bytes: [],
                    warnings: [],
                    errors: ["Invalid hex value"]
                };
            }
        } else {
            value = input.data.epoch;
        }
        return {
            bytes: [
                value >> 24,
                (value >> 16) & 0xFF,
                (value >> 8) & 0xFF,
                (value & 0xFF)],
            fPort: CMD_SET_DATETIME,
            warnings: [],
            errors: []
        };
    } else {
        return {
            bytes: [],
            errors: ["Unknown command"],
            fPort: 1,
            warnings: []
        };
    }
}

// Decode Downlink from bytes to JSON
function decodeDownlink(input) {
    switch (input.fPort) {
        case CMD_GET_DATETIME:
        case CMD_GET_LW_CONFIG:
        case CMD_GET_LW_STATUS:
            return {
                data: [0],
                warnings: [],
                errors: []
            };
        case CMD_SET_DATETIME:
            return {
                data: {
                    unixtime: "0x" + uint32BE(input.bytes.slice(0, 4)).toString(16)
                }
            };
        case CMD_SET_SLEEP_INTERVAL:
            return {
                data: {
                    sleep_interval: uint16BE(input.bytes)
                }
            };
        case CMD_SET_SLEEP_INTERVAL_LONG:
            return {
                data: {
                    sleep_interval_long: uint16BE(input.bytes)
                }
            };
        case CMD_SET_LW_STATUS_INTERVAL:
            return {
                data: {
                    lw_status_interval: uint8(input.bytes)
                }
            };
        default:
            return {
                errors: ["unknown FPort"]
            };
    }
}
