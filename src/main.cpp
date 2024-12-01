#include "fonts.h"
#include "ClockUi.h"
#include "Utils/Trace.h"

#include "PicoClockHw/Platform.h"
#include "PicoClockHw/Wifi.h"

#include <hardware/uart.h>

int main() 
{
    Platform::initStdIo();

    // Can be enabled to delay startup in order to debug
#if 0    
    for (int i = 5; i > 0;i--)
    {
        sleep_ms(1000);
        std::cout << i << std::endl;
    }
#endif
    TRACE << "Clock UI";
    ClockUi ui;

    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    auto baud = uart_init(uart0, 9600);
    TRACE << "Baud: " << baud;

    while (1)
    {
        bool r = uart_is_readable(uart0);
        //std::cout << "Readable: " <<r  << std::endl;
        if (r)
            std::cout << "Received: " << uart_getc(uart0) << std::endl;
    }

    if (Wifi::init())
        if (Wifi::connectBlocking())
            ui.startNtpRequest();

    TRACE <<"Start the loop\n";
    Platform::runMainLoop();

    // Not reachable for the moment, but a shutdown function may be added later.
    Wifi::deinit();

    return 0;
}