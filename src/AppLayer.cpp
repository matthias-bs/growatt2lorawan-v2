///////////////////////////////////////////////////////////////////////////////
// AppLayer.cpp
//
// LoRaWAN node application layer - Growatt PV Inverter (Modbus Interface)
//
//
// created: 05/2024
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
// 20240513 Created
// 20240316 Implemented genPayload()
//
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#include "AppLayer.h"
#include "growattInterface.h"
#include "growatt_cfg.h"

growattIF growattInterface(MAX485_RE_NEG, MAX485_DE, MAX485_RX, MAX485_TX);
//bool holdingregisters = false;

uint8_t
AppLayer::decodeDownlink(uint8_t port, uint8_t *payload, size_t size)
{
    return 0;
}

void AppLayer::genPayload(uint8_t port, LoraEncoder &encoder)
{
#if defined(EMULATE_SENSORS)
    // modbus
    encoder.writeUint8(0);
    if (port == 1) {
        // status
        encoder.writeUint8(0);
        // faultcode
        encoder.writeUint8(0);
        // energytoday
        encoder.writeRawFloat(4.4);
        // energytotal
        encoder.writeRawFloat(5555.5);
        // totalworktime
        encoder.writeRawFloat(12345678);
        // outputpower
        encoder.writeRawFloat(600.0);
        // gridvoltage
        encoder.writeRawFloat(230.0);
        // gridfrequency
        encoder.writeRawFloat(50.0);
    } else {
        // pv1voltage
        encoder.writeRawFloat(80);
        // pv1current
        encoder.writeRawFloat(8.8);
        // pv1power
        encoder.writeRawFloat(8.8 * 80);
        // tempinverter
        encoder.writeTemperature(25.5);
        // tempipm
        encoder.writeTemperature(25.5);
        // pv1energytoday
        encoder.writeRawFloat(4.6);
        // pv1energytotal
        encoder.writeRawFloat(5666.6);
    }
#endif
}

void AppLayer::getPayloadStage1(uint8_t port, LoraEncoder &encoder)
{
    (void)port;    // suppress warning regarding unused parameter
    (void)encoder; // suppress warning regarding unused parameter
}

void AppLayer::getPayloadStage2(uint8_t port, LoraEncoder &encoder)
{
    uint8_t result;

    growattInterface.initGrowatt();
    delay(500);
    /*
    if (!holdingregisters) {
      // Read the holding registers
      ReadHoldingRegisters();
    } else {
      // Read the input registers
      ReadInputRegisters();
    }
    */

    int retries = 0;
    do
    {
        result = growattInterface.ReadInputRegisters(NULL);
        log_d("ReadInputRegisters: 0x%02x", result);
        if ((result != growattInterface.Continue) && (result != growattInterface.Success))
        {
            String message = growattInterface.sendModbusError(result);
            log_e("Error: %s", message.c_str());
        }
        while (result == growattInterface.Continue)
        {
            delay(1000);
            result = growattInterface.ReadInputRegisters(NULL);
            String message = growattInterface.sendModbusError(result);
            if (result != growattInterface.Continue && (result != growattInterface.Success))
            {
                log_e("Error: %s", message.c_str());
                delay(1000);
            }
            else
            {
                log_d("%s", message.c_str());
            }
        }
    } while ((result != growattInterface.Success) && (++retries < MODBUS_RETRIES));

    encoder.writeUint8(result);
    if (result == growattInterface.Success)
    {
        log_v("Port: %d", port);
        if (port == 1)
        {
            encoder.writeUint8(growattInterface.modbusdata.status);
            encoder.writeUint8(growattInterface.modbusdata.faultcode);
            encoder.writeRawFloat(growattInterface.modbusdata.energytoday);
            encoder.writeRawFloat(growattInterface.modbusdata.energytotal);
            encoder.writeRawFloat(growattInterface.modbusdata.totalworktime);
            encoder.writeRawFloat(growattInterface.modbusdata.outputpower);
            encoder.writeRawFloat(growattInterface.modbusdata.gridvoltage);
            encoder.writeRawFloat(growattInterface.modbusdata.gridfrequency);
        }
        else
        {
            encoder.writeRawFloat(growattInterface.modbusdata.pv1voltage);
            encoder.writeRawFloat(growattInterface.modbusdata.pv1current);
            encoder.writeRawFloat(growattInterface.modbusdata.pv1power);
            encoder.writeTemperature(growattInterface.modbusdata.tempinverter);
            encoder.writeTemperature(growattInterface.modbusdata.tempipm);
            encoder.writeRawFloat(growattInterface.modbusdata.pv1energytoday);
            encoder.writeRawFloat(growattInterface.modbusdata.pv1energytotal);
        }
    }
}

void AppLayer::getConfigPayload(uint8_t cmd, uint8_t &port, LoraEncoder &encoder)
{
    (void)cmd;     // suppress warning regarding unused parameter
    (void)port;    // suppress warning regarding unused parameter
    (void)encoder; // suppress warning regarding unused parameter
}
