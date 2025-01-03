#include "Platform.h"
#include "Rtc.h"
#include <pico/stdlib.h>
#include <pico/rand.h>

void Platform::initStdIo()
{
    stdio_init_all();
}

void Platform::runMainLoop()
{
    while (1)
    {
        sleep_ms(1000);
        Rtc::onSecond();
    }
}

int Platform::getCharNonBlocking()
{
    return getchar_timeout_us(0);
}

uint64_t Platform::timeUs()
{
    return time_us_64();
}

uint32_t Platform::randomNumber32()
{
    return get_rand_32();
}