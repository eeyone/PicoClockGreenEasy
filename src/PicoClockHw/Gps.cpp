#include "Gps.h"

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

    std::string calculateNmeaMsgChecksum(const std::string &msgContent)
    {
        uint8_t checksum = 0;
        for (char c : msgContent)
            checksum ^= c;
        std::ostringstream checksumHex;
        checksumHex 
            <<std::hex 
            <<std::uppercase 
            <<std::setw(2) 
            <<std::setfill('0') 
            <<static_cast<int>(checksum);
        return checksumHex.str();
    }
}

Gps *Gps::m_instance = nullptr;

Gps::Gps()
{
    m_instance = this;
    uart_init(GPS_UART, 9600);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

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
    
    // Tell the receiver to send the message we are interested in (RMC) or not.
    setNmeaMessageEnabled("RMC", enabled);

    if (enabled)
    {
        // Set up timer to detect reception timeout
        resetTimeoutAlarm();
    } else
    {
#ifdef DISABLE_GPS_MODULE_LED
        // Now that we stopped exchanging NMEA messages, let's send a UBX message to disable 
        // timepulse. This avoids colisions with NMEA messages.
        disableTimepulse();
#endif

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
        if (m_receivingUbxMsg)
        {
            g_receiveBuffer.push_back(c);
            if (g_receiveBuffer.size() == 6)
            {
                // Received enough bytes to read the message size. Decode it and keep it.
                m_ubxMsgSize = 
                    g_receiveBuffer[4] + (g_receiveBuffer[5] << 8) + 8;
                TRACE << "Defined size:" <<m_ubxMsgSize;
            }
            else if (g_receiveBuffer.size() == m_ubxMsgSize)
            {
                // The message can be dumped for debugging
#if 0
                std::ostringstream msg;
                msg << "UBX message: ";
                for (int i = 0; i < m_ubxMsgSize; i++)
                {
                    msg 
                    << std::hex 
                    << std::setfill('0') 
                    << std::setw(2) 
                    << (int)g_receiveBuffer[i] 
                    << ",";
                }
                TRACE << msg.str();
#endif                

                onUbxMessage(std::vector<uint8_t>(g_receiveBuffer.begin(), g_receiveBuffer.end()));

                g_receiveBuffer.clear();
                m_receivingUbxMsg = false;
            } 
            else if (g_receiveBuffer.size() > 100)
            {
                TRACE << "UBX message got too long!";
                g_receiveBuffer.clear();
                m_receivingUbxMsg = false;
            }
        } else
        {
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
                    m_receivingUbxMsg = true;
                    // fallthrough, as the received byte is the first of the message;
                default: // Gather line characters
                    g_receiveBuffer.push_back(c);
                    break;
            }

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

    // Disable this message type if the whole Gps object is disabled, or if it is not an RMC 
    // message, then ignore this particular one. 
    if (!m_enabled || msgId != "$GPRMC")
    {
        // Don't disable the message if its id looks invalid or it is one that cannot be disabled.
        if (msgId.size() != 6 || msgId == "$GPTXT")
            return;

        TRACE << "Disable unwanted message " << msgId;
        setNmeaMessageEnabled(msgId.substr(3, 3), false);
        return;
    }

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

void Gps::setNmeaMessageEnabled(const std::string &msgId, bool enabled)
{
    std::string msgContent = "PUBX,40," + msgId + ",0," + (enabled ? "1" : "0") + ",0,0,0,0";
    std::string msg = "$" + msgContent + "*" + calculateNmeaMsgChecksum(msgContent) + "\r\n";

    TRACE << "Send NMEA message:" << msg;
    uart_write_blocking(GPS_UART, reinterpret_cast<const uint8_t *>(msg.data()), msg.size());
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

void Gps::disableTimepulse()
{
    // Prepare a CFG-TP5 poll request. 
    uint8_t msg[] = {0xB5, 0x62, 0x06, 0x31, 0 ,0, 0, 0};
    putUbxMsgChecksum(msg, sizeof(msg));

    // Send CFG-TP5 poll request. The receiver will reply with a CFG-TP5 message that contains the 
    // timepulse configuration.
    TRACE << "Send CFG-TP5 UBX message";
    uart_write_blocking(GPS_UART, msg, sizeof(msg));
}

void Gps::onUbxMessage(const std::vector<uint8_t> &msg)
{
    // Postpone timeout timer
    resetTimeoutAlarm();

    if (msg[1] == 0x62 && msg[2] == 0x06)
    {
        TRACE << "Received CFG-TP5 message";
        auto copy = msg;

        // Disable the timepulse by changing the corresponding bit and sending back the message
        copy[34] &= ~1;
        putUbxMsgChecksum(copy.data(), copy.size());
        TRACE << "Resent CFG-TP5 message";
        uart_write_blocking(GPS_UART, copy.data(), copy.size());
    }
}