#pragma once

#include "Timer.h"
#include <cstdint>
#include <cstddef>

class Flash
{
public:
    // Attach a data block to be read/written from/to flash. It must remain accessible all
    // the time. 
    static bool attach(uint8_t *data, size_t size);

    // Read the data block from flash, return the number of bytes read or -1 if it failed.
    static int read();

    // Schedule the given data block to be written to flash later (as simple measure 
    // to mitigate wear). Calling the method again restarts the delay.
    static void scheduleWrite();

private:
    static void write();

    static uint8_t *m_data;
    static size_t m_size;
    static Timer m_writeAlarm;
};