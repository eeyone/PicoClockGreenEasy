#pragma once

#include "Utils/MovingAverage.h"

#include <memory>

class Rtc
{
public:
    Rtc();
    ~Rtc();

    bool read(tm &dateTime) const;
    bool write(const tm &dateTime);

    float temperature();

private:
    float rawTemperature() const;
    static float registersToTemp(uint8_t msb, uint8_t lsb);
    
    MovingAverage<64> m_tempFilter;
    uint64_t m_lastTempMeasurementUs;
};

