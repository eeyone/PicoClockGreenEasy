#pragma once

#include "Timer.h"
#include <cstdint>

class Buzzer
{
public:
    Buzzer();

    void beepForMs(int delay);

private:
    void stopBeep();

    Timer m_stopBeepAlarm;
};