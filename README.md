[![CI](https://github.com/matthias-bs/growatt2lorawan-v2/actions/workflows/CI.yml/badge.svg)](https://github.com/matthias-bs/growatt2lorawan-v2/actions/workflows/CI.yml)
[![GitHub release](https://img.shields.io/github/release/matthias-bs/growatt2lorawan-v2?maxAge=3600)](https://github.com/matthias-bs/growatt2lorawan-v2/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](https://github.com/matthias-bs/growatt2lorawan-v2/blob/main/LICENSE)

# growatt2lorawan-v2
LoRaWAN Node for Growatt Photovoltaic Inverter Modbus Data Interface

This project can be used if you want to monitor your PV Inverter in case it is out of reach of your WiFi access point (and a WiFi repeater would not help/is not wanted) and you are not able to cover the distance with an RS485 cable.
Otherwise, the original Growatt ShineWiFi-S (serial) / ShineWiFi-X (USB), the customized [otti/Growatt_ShineWiFi-S](https://github.com/otti/Growatt_ShineWiFi-S) or [nygma2004/growatt2mqtt](https://github.com/nygma2004/growatt2mqtt) can be used.

<img src="https://user-images.githubusercontent.com/83612361/234670204-18edc9af-aa36-49aa-8b0f-26082e73d37f.png" alt="growatt2lorawan Architecture" height="600" />

This is a reimplementation of [growatt2lorawan](https://github.com/matthias-bs/growatt2lorawan) using [RadioLib](https://github.com/jgromes/RadioLib).

:construction_worker: **Work in Progress!** :construction_worker: 

- [X] Scheduled uplink (2 message types)
- [X] Network time sync
- [ ] Downlink decoding
- [ ] Commanded uplink (configuration)

## Contents
* [Hardware Requirements](#hardware-requirements)
* [Inverter Modbus Interface Options](#inverter-modbus-interface-options)
  * [Modbus Interface Select Input](#modbus-interface-select-input)
* [Power Supply](#power-supply)
* [Library Dependencies](#library-dependencies)
* [Pinning Configuration](#pinning configuration)
  * [Modbus Interface Selection](#modbus-interface-selection)
  * [Modbus via RS485 Interface](#modbus-via-rs485-interface)
  * [Debug Interface in case of using Modbus via USB Interface (optional)]()
* [LoRaWAN Network Service Configuration](#lorawan-network-service-configuration)
* [Software Build Configuration](#software-build-configuration)
* [LoRaWAN Payload Formatters](#lorawan-payload-formatters)
  * [The Things Network Payload Formatters Setup](#the-things-network-payload-formatters-setup)
* [MQTT Integration and IoT MQTT Panel Example](#mqtt-integration-and-iot-mqtt-panel-example)
  * [Set up *IoT MQTT Panel* from configuration file](#set-up-iot-mqtt-panel-from-configuration-file)

## Hardware Requirements
* ESP32 (optionally with LiPo battery charger and battery)
* SX1276 or SX1262 (or compatible) LoRaWAN Radio Transceiver
* LoRaWAN Antenna
* optional: RS485 Transceiver - 3.3V compatible, half-duplex capable (e.g [Waveshare 4777](https://www.waveshare.com/wiki/RS485_Board_(3.3V)) module)
* optional: USB-to-TTL converter for Debugging (e.g. [AZ Delivery HW-598](https://www.az-delivery.de/en/products/hw-598-usb-auf-seriell-adapter-mit-cp2102-chip-und-kabel))

## Inverter Modbus Interface Options

1. USB Interface

    The inverter's USB port operates like a USB serial port (UART) interface at 115200 bits/s. If the length of a standard USB cable is sufficient to connect the ESP32 to the inverter (and there are no compatibility issues with the ESP32 board's USB serial interface), this is the easiest variant, because no extra hardware is needed.
    
    As pointed out in [otti/Growatt_ShineWiFi-S](https://github.com/otti/Growatt_ShineWiFi-S/blob/master/README.md), **only CH340-based USB-Serial converters are compatible** - converters with CP21XX and FTDI chips **do not work**!

2. COM Interface

    The inverter's COM port provides an RS485 interface at 9600 bits/s. An RS485 tranceiver is required to connect it to the ESP32.

### Modbus Interface Select Input

The desired interface is selected by pulling the GPIO pin `INTERFACE_SEL` (defined in `settings.h`) to 3.3V or GND, respectively:

| Level | Modbus Interface Selection |
| ----- | ---------------------------- |
| low (GND) | USB Interface |
| high (3.3V/open) | RS485 Interface |

## Power Supply

The ESP32 development board can be powered from the inverter's USB port **which only provides power if the inverter is active**.

**No sun - no power - no transmission!** :sunglasses:

But: Some ESP32 boards have an integrated LiPo battery charger. You could power the board from a battery while there is no PV power (at least for a few hours). 

## Library Dependencies

* [RadioLib](https://github.com/jgromes/RadioLib) by Jan Gromeš  
* [Lora-Serialization](https://github.com/thesolarnomad/lora-serialization) by Joscha Feth
* [ESP32Time](https://github.com/fbiego/ESP32Time) by Felix Biego
* [ModbusMaster](https://github.com/4-20ma/ModbusMaster) by Doc Walker

## Pinning Configuration

See [src/growatt_cfg.h](src/growatt_cfg.h)

### Modbus Interface Selection

| GPIO define | Description |
| ----------- | ----------- |
| INTERFACE_SEL | Modbus Interface Selection (USB/RS485) |

### Modbus via RS485 Interface

| GPIO define   | Waveshare 4777 pin  |
| ------------- | ------------------- |
| MAX485_DE     | RSE                 |
| MAX485_RE_NEG | n.c.                |
| MAX485_RX     | RO                  |
| MAX485_TX     | DI                  |

### Debug Interface in case of using Modbus via USB Interface (optional)

USB-to-TTL converter, e.g. [AZ Delivery HW-598](https://www.az-delivery.de/en/products/hw-598-usb-auf-seriell-adapter-mit-cp2102-chip-und-kabel)

| GPIO define | USB to TTL Converter |
| ----------- | -------------------- | 
| DEBUG_TX    | RXD                  |
| DEBUG_RX    | TXD / n.c.           |

## LoRaWAN Network Service Configuration

Create an account and set up a device configuration in your LoRaWAN network provider's web console, e.g. [The Things Network](https://www.thethingsnetwork.org/).

* LoRaWAN v1.1
* Regional Parameters 1.1 Revision A
* Device class A
* Over the air activation (OTAA)

> [!IMPORTANT]
> Check the maximum permitted payload size and uplink/downlink rate according to your regional parameters and change the configuration if required!
> See [Airtime calculator for LoRaWAN](https://avbentem.github.io/airtime-calculator/ttn/eu868).

## Software Build Configuration

* Install the Arduino ESP32 board package in the Arduino IDE
* Select your ESP32 board
* Install all libraries as listed in [package.json](package.json) &mdash; section 'dependencies' &mdash; via the Arduino IDE Library Manager 
* Clone (or download and unpack) the latest ([growatt2lorawan-v2 release](https://github.com/matthias-bs/growatt2lorawan-v2/releases))
* Set your LoRaWAN Network Service credentials &mdash; `RADIOLIB_LORAWAN_DEV_EUI`, `RADIOLIB_LORAWAN_NWK_KEY` and `RADIOLIB_LORAWAN_APP_KEY` &mdash; in [secrets.h](secrets.h):

```
// The Device EUI & two keys can be generated on the TTN console

// Replace with your Device EUI
#define RADIOLIB_LORAWAN_DEV_EUI   0x---------------

// Replace with your App Key
#define RADIOLIB_LORAWAN_APP_KEY   0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--

// Put your Nwk Key here
#define RADIOLIB_LORAWAN_NWK_KEY   0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--, 0x--
```

* Load the sketch [growatt2lorawan-v2.ino](growatt2lorawan-v2.ino)
* Compile and Upload

## LoRaWAN Payload Formatters

Upload [Uplink Formatter](scripts/uplink_formatter.js) and [Downlink Formatter](scripts/downlink_formatter.js) scripts in your LoRaWAN network service provider's web console to allow decoding / encoding of raw data to / from JSON format.

See [The Things Network MQTT Integration and Payload Formatters](https://github.com/matthias-bs/BresserWeatherSensorTTN/blob/main/README.md#the-things-network-mqtt-integration-payload-formatters) and [TS013-1.0.0 Payload Codec API](https://resources.lora-alliance.org/technical-specifications/ts013-1-0-0-payload-codec-api) for more details.

### The Things Network Payload Formatters Setup

#### Uplink Formatter

Decode uplink payload (a sequence of bytes) into JSON format, i.e. data structures which are readable/suitable for further processing.

In The Things Network Console:
1. Go to "Payload formatters" -> "Uplink"
2. Select "Formatter type": "Custom Javascript formatter"
3. "Formatter code": Paste [scripts/uplink_formatter.js](scripts/uplink_formatter.js)
4. Apply "Save changes"

![TTN Uplink Formatter](https://github.com/matthias-bs/BresserWeatherSensorTTN/assets/83612361/38b66478-688a-4028-974a-c517cddae662)

#### Downlink Formatter

Encode downlink payload from JSON to a sequence of bytes.

In The Things Network Console:
1. Go to "Payload formatters" -> "Downlink"
2. Select "Formatter type": "Custom Javascript formatter"
3. "Formatter code": Paste [scripts/downlink_formatter.js](scripts/downlink_formatter.js)
4. Apply "Save changes"

## MQTT Integration and IoT MQTT Panel Example

Arduino App: [IoT MQTT Panel](https://snrlab.in/iot/iot-mqtt-panel-user-guide)

<img src="https://user-images.githubusercontent.com/83612361/225129950-c323e0c7-a58b-4a3f-ba30-e0fd9adc1594.jpg" alt="Screenshot_20230314-130325_IoT_MQTT_Panel_Pro-1" width="400">

### Set up *IoT MQTT Panel* from configuration file

You can either edit the provided [JSON configuration file](IoT_MQTT_Panel_Growatt2LoRaWAN.json) before importing it or import it as-is and make the required changes in *IoT MQTT Panel*. Don't forget to add the broker's certificate if using Secure MQTT! (in the App: *Connections -> Edit Connections: Certificate path*.)

**Editing [IoT_MQTT_Panel_Growatt2LoRaWAN.json](IoT_MQTT_Panel_Growatt2LoRaWAN.json)**

Change *USERNAME* and *PASSWORD* as needed:
```
[...]
"username":"USERNAME","password":"PASSWORD"
[...]
```
