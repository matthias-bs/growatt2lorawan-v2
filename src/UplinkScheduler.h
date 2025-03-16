///////////////////////////////////////////////////////////////////////////////
// UplinkScheduler.h
//
// Schedule LoRaWAN uplinks based on a scheduling table and the current cycle
//
// created: 03/2025
//
//
// MIT License
//
// Copyright (c) 2025 Matthias Prinke
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
// 20250316 Created
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "growatt2lorawan_cfg.h"

/**
 * \brief Sensor data uplink scheduler
 */
class UplinkScheduler
{
private:
    uint8_t uplinks = 0;
    uint8_t index = 0;
    uint8_t cycle = 0;

public:
    /**
     * \brief Uplink scheduler constructor
     */
    UplinkScheduler()
    {};

    /**
     * \brief Initialize uplink scheduler
     * 
     * \param current_cycle Current operating cycle
     */
    void begin(uint8_t current_cycle)
    {
        index = 0;
        cycle = current_cycle;
        for (int i = 0; i < NUM_PORTS; i++)
        {
            if (UplinkSchedule[i].mult && (cycle % UplinkSchedule[i].mult == 0))
            {
                uplinks++;
            }
        }
        log_d("UplinkScheduler: cycle=%u uplinks=%u", cycle, uplinks);
    };

    /**
     * \brief Check if there are uplinks scheduled
     * 
     * \return true Uplinks are scheduled
     * \return false No uplinks are scheduled
     */
    bool hasUplinks()
    {
        return uplinks > 0;
    };

    /**
     * \brief Get next uplink port
     * 
     * \return uint8_t Next uplink port (or 0 if none)
     */
    uint8_t getNextPort()
    {
        for (int i = index; i < NUM_PORTS; i++)
        {
            if (UplinkSchedule[i].mult && (cycle % UplinkSchedule[i].mult == 0))
            {
                index = i + 1;
                uplinks--;
                log_d("UplinkScheduler: fPort=%d remaining: %u", UplinkSchedule[i].port, uplinks);
                return UplinkSchedule[i].port;
            }
        }
        return 0;
    };
};
