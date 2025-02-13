#pragma once

#include "Timer.h"
#include <map>
#include <functional>
#include <cstdint>
#include <pico/time.h>

class Button
{
public:
    Button(unsigned int gpio);
    ~Button();

    void setPressedCallback(std::function<void()> f);
    void setRepeatCallback(std::function<void()> f, int delayMs);

private:
    static void dispatcher(unsigned int gpio, uint32_t events);
    void onDebounce();
    Timer::Rescheduling onRepeat();

    unsigned int m_gpio;
    static std::map<unsigned int, Button *> m_buttonByGpio;

    std::function<void()> m_pressedCallback;
    std::function<void()> m_repeatCallback;

    int m_repeatDelay = 0;
    Timer m_repeatAlarm;
    Timer m_debounceAlarm;
};
