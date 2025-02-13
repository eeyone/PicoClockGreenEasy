#include "Buzzer.h"
#include "gpio.h"
#include "Utils/Trampoline.h"

#include <hardware/gpio.h>

Buzzer::Buzzer()
{
    gpio_init(BUZZ);
    gpio_set_dir(BUZZ, GPIO_OUT);
}

void Buzzer::beepForMs(int delay)
{
    gpio_put(BUZZ, true);
    m_stopBeepAlarm.startSingleShot(delay, std::bind(&Buzzer::stopBeep, this));
}

void Buzzer::stopBeep()
{
    gpio_put(BUZZ, false);
}