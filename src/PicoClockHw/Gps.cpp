#include "Gps.h"

#include "gpio.h"

#include "Utils/Trace.h"
#include "Utils/Trampoline.h"

#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <sstream>
#include <iomanip>

// Based on this protocol specification:
// https://content.u-blox.com/sites/default/files/products/documents/u-blox6_ReceiverDescrProtSpec_%28GPS.G6-SW-10018%29_Public.pdf)

namespace
{
    const auto GPS_UART = uart0;
    const int TIMEOUT_MS = 2000;

    void putUbxMsgChecksum(uint8_t *msg, size_t size)
    {
        if (size < 2)
        {
            TRACE << "Incorrect msg size!";
            return;
        }

        uint8_t ckA = 0, ckB = 0;
        for(size_t i = 2; i <size - 2 ;i++)
        {
            ckA = ckA + msg[i];
            ckB = ckB + ckA;
        }
        msg[size - 2] = ckA;
        msg[size - 1] = ckB;
    }
}

Gps *Gps::m_instance = nullptr;

Gps::Gps()
{
    m_instance = this;
    uart_init(GPS_UART, 9600);
    gpio_set_function(GPS_TX, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX, GPIO_FUNC_UART);

    // Select correct interrupt for the UART we are using
    const int UART_IRQ = GPS_UART == uart0 ? UART0_IRQ : UART1_IRQ;

    // Set up and enable the interrupt handler
    MAKE_TRAMPOLINE(Gps, onUartRx, singleton);
    irq_set_exclusive_handler(UART_IRQ, onUartRx);
    irq_set_enabled(UART_IRQ, true);        

    // Enable the UART to send interrupts on reception
    uart_set_irq_enables(GPS_UART, true/*has data*/, false/*needs data*/);
}

Gps::~Gps()
{
    setEnabled(false);

    uart_set_irq_enables(GPS_UART, false/*has data*/, false/*needs data*/);
    const int UART_IRQ = GPS_UART == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_enabled(UART_IRQ, false);

    uart_deinit(GPS_UART);
    m_instance = nullptr;
}

void Gps::setEnabled(bool enabled)
{
    m_enabled = enabled;
    
    // Turn the GPS module on or off using the backup mode.
    setBackupMode(!enabled);

    if (enabled)
    {
        // Set up timer to detect reception timeout
        resetTimeoutAlarm();
    } else
    {
        TRACE << "Stop timer";
        m_timeoutAlarm.stop();
    }
}

void Gps::resetTimeoutAlarm()
{
    m_timeoutAlarm.stop();

    if (m_enabled)
    {
        m_timeoutAlarm.startSingleShot(TIMEOUT_MS, std::bind(&Gps::onTimeout, this));
    }
}

void Gps::onTimeout()
{
    m_timeoutCallback();
}

void Gps::onUartRx()
{
    while (uart_is_readable(GPS_UART)) 
    {
        char c = uart_getc(GPS_UART);

        switch(c)
        {
            case 10: // LR: ignore
                break;
            case 13: // CR: Handle the received line
                TRACE << g_receiveBuffer;
                onNmeaMessage(g_receiveBuffer);
                g_receiveBuffer.clear();
                break;
            case 0xB5:
                TRACE << "Start of UBX message detected";
                // fallthrough, as the received byte is the first of the message. However, the 
                // message will not be decoded properly, as support for receiving UBX messages is no 
                // longer needed and has been removed.
            default: // Gather line characters
                g_receiveBuffer.push_back(c);
                break;
        }
    }
}

void Gps::onNmeaMessage(const std::string &msg)
{
    // Postpone timeout timer
    resetTimeoutAlarm();

    // Decode RMC message 

    std::istringstream stream(msg);
    std::string msgId;
    if (!std::getline(stream, msgId, ','))
        return;

    std::string utcTime;
    if (!std::getline(stream, utcTime, ','))
        return;
//    TRACE << "UTC time:" << utcTime;

    std::string status;
    if (!std::getline(stream, status, ','))
        return;
    // Ignore the status. As long as it contains a time and date, that will be sufficient for
    // synchronization. We don't need a real GPS lock.

    // Skip fields we don't need
    for (int i = 0; i < 6; i++)
    {
        std::string s;
        if (!std::getline(stream, s, ','))
            return;
    }

    std::string date;
    if (!std::getline(stream, date, ','))
        return;
//    TRACE << "Date:" << date;

    onDateTime(date, utcTime);
}

void Gps::onDateTime(const std::string &date, const std::string &time)
{
    if (date.size() < 6 || time.size() < 6)
        return;

    tm dt = {};
    float secWithMs = std::stof(time.substr(4));
    dt.tm_sec = secWithMs;
    int ms = secWithMs * 1000 - dt.tm_sec * 1000;
    dt.tm_min = std::stoi(time.substr(2, 2));
    dt.tm_hour = std::stoi(time.substr(0, 2));
    dt.tm_mday = std::stoi(date.substr(0, 2));
    dt.tm_mon = std::stoi(date.substr(2, 2)) - 1; // tm::tm_mon is 0-based.
    dt.tm_year = std::stoi(date.substr(4, 2)) + 100; // tm::tm_year is "years since 1900"

    if (m_timeCallback)
        m_timeCallback(mktime(&dt), ms);
}

void Gps::setBackupMode(bool backupMode)
{
    uint8_t flag = backupMode ? 2 : 0;

    //               header----  id--------  len-  payload------------------  checksum
    uint8_t msg[] = {0xB5, 0x62, 0x02, 0x41, 8, 0, 0, 0, 0, 0, flag, 0, 0, 0, 0, 0};
    putUbxMsgChecksum(msg, sizeof(msg));

    TRACE << "Send RXM-PMREQ message";
    uart_write_blocking(GPS_UART, msg, sizeof(msg));
}
