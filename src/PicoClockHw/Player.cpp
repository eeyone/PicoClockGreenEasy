// https://www.electronicoscaldas.com/datasheet/DFR0299-DFPlayer-Mini-Manual.pdf?srsltid=AfmBOoq-OtiGXRcKGhYnv_vwpQqS3vaBDtOVyyfwmSWj7EP0cMv1CriK
// http://www.trainelectronics.com/Arduino/MP3Sound/TalkingTemperature/FN-M16P%20Embedded%20MP3%20Audio%20Module%20Datasheet.pdf

#include "Player.h"
#include "Utils/Trace.h"
#include "Utils/Trampoline.h"
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <iomanip>
#include <sstream>

namespace
{
    const auto PLAYER_UART = uart1;
}

Player *Player::m_instance = nullptr;

Player::Player()
{
    m_instance = this;

    uart_init(PLAYER_UART, 9600);
    gpio_set_function(8, GPIO_FUNC_UART);
    gpio_set_function(9, GPIO_FUNC_UART);

    // TODO: centralize this in a common place, as Gps.cpp has similar code
    // Select correct interrupt for the UART we are using
    const int UART_IRQ = PLAYER_UART == uart0 ? UART0_IRQ : UART1_IRQ;

    // Set up and enable the interrupt handler
    MAKE_TRAMPOLINE(Player, onUartRx, singleton);
    irq_set_exclusive_handler(UART_IRQ, onUartRx);
    irq_set_enabled(UART_IRQ, true);        

    // Enable the UART to send interrupts on reception
    uart_set_irq_enables(PLAYER_UART, true/*has data*/, false/*needs data*/);
}

void Player::onUartRx()
{
    while (uart_is_readable(PLAYER_UART)) 
    {
        uint8_t c = uart_getc(PLAYER_UART);
        m_receiveBuffer.push_back(c);

        if (m_receiveBuffer.size() >= 10) // End of message
        {
            m_rxTimeoutTimer.stop();
            onMsgEnd();
            m_receiveBuffer.clear();
        } else
        {
            m_rxTimeoutTimer.startSingleShot(10, std::bind(&Player::onRxTimeout, this));
        }
    }
}

void Player::onRxTimeout()
{
    m_receiveBuffer.clear();
    TRACE << "Timeout waiting for end of message";
}

void Player::onMsgEnd()
{
#if 0 // Code to dump the message as hex codes
    std::ostringstream stream;
    stream << std::uppercase << std::hex;
    for (auto c : m_receiveBuffer)
    {
        stream  << std::setfill('0') << std::setw(2) << (int)c << " ";
    }
    TRACE  <<"Received message:" << stream.str();
#endif

    if (m_receiveBuffer.size() != 10 || 
        m_receiveBuffer[0] != 0x7E || 
        m_receiveBuffer[1] != 0xFF || 
        m_receiveBuffer[2] != 0x06 || 
        m_receiveBuffer[9] != 0xEF)
    {
        TRACE << "Invalid message received";
        return;
    }

    int param = m_receiveBuffer[5] << 8 | m_receiveBuffer[6];

    switch(m_receiveBuffer[3])
    {
        case 0x3A: // storage device is plugged in
            switch(param)
            {
                case 0x01:
                    TRACE << "USB flash drive plugged in";
                    break;
                case 0x02:
                    TRACE << "SD card plugged in";
                    break;
                default:
                    TRACE << "Unknown storage device plugged in" << param;
            }
            break;
        case 0x3B: // storage device was unplugged
            switch(param)
            {
                case 0x01:
                    TRACE << "USB flash drive unplugged";
                    break;
                case 0x02:
                    TRACE << "SD card unplugged";
                    break;
                default:
                    TRACE << "Unknown storage device plugged out" << param;
            }
            break;
        case 0x3D: // Track finished playing
            TRACE << "Track #" <<param <<"finished playing";
            break;
        case 0x3F: // Storage device
            switch(param)
            {
                case 0x00:
                    TRACE << "No storage device";
                    break;
                case 0x01:
                    TRACE << "Storage device is USB";
                    break;
                case 0x02:
                    TRACE << "Storage device is SD";
                    break;
                default:
                    TRACE << "Unknown storage device" << param;
            }
            break;
        case 0x40: // Error
            switch(param)
            {
                case 0x01:
                    TRACE << "Error: module busy";
                    break;
                case 0x02:
                    TRACE << "Error: Currently in sleep mode";
                    break;
                case 0x03:
                    TRACE << "Error: Serial receiving error";
                    break;
                case 0x04:
                    TRACE << "Error: Checksum incorrect";
                    break;
                case 0x05:
                    TRACE << "Error: Specified track is out of current track scope";
                    break;
                case 0x06:
                    TRACE << "Error: Specified track is not found";
                    break;
                case 0x07:
                    TRACE << "Error: Insertion error";
                    break;
                case 0x08:
                    TRACE << "Error: SD card reading failed";
                    break;
                case 0x0A:
                    TRACE << "Error: Entered into sleep mode";
                    break;
// TODO: method too long, refactor it
                default:
                    TRACE << "Unknown error" << param;
            }
            break;
        default:
            TRACE << "Unknown message with command code" 
                << std::uppercase << std::hex
                << (int)m_receiveBuffer[3] 
                << "and parameter" 
                << param;
    }
}

void Player::selectDevice()
{
    TRACE << "Specified device is micro SD";
    sendCommand(0x09, 0x00, 0x02);
}

void Player::setVolume(int volume)
{
    sendCommand(0x06, 0x00, volume);
}

void Player::play()
{
    {
        TRACE << "Play";
        uint8_t msg[] = {0x7E, 0xFF, 0x06, 0x0D, 0x00, 0x00, 0x00, 0xFE, 0xEE, 0xEF};
    //    uint8_t msg[] = {0x7E, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x01, 0xFE, 0xF7, 0xEF};
        uart_write_blocking(PLAYER_UART, msg, sizeof(msg));
    }
}

void Player::playTrackInFolder(int track, int folder)
{
    sendCommand(0x0F, folder, track);
}

void Player::queryFolderCount()
{
    sendCommand(0x4F, 0x00, 0x00);
}

void Player::sendCommand(uint8_t command, uint8_t param1, uint8_t param2)
{
    TRACE << "Send command" << std::hex << (int)command << "with parameters" << (int)param1 <<"and" << (int)param2;
    uint8_t msg[] = {0x7E, 0xFF, 0x06, command, 0x00, param1, param2, 0, 0, 0xEF};

    // Calculate checksum
    uint16_t checksum = 0;
    for (int i = 1; i < sizeof(msg) - 3; i++)
    {
        checksum -= msg[i];
    }
    msg[7] = checksum >> 8; // High byte
    msg[8] = checksum & 0xFF; // Low byte

    std::ostringstream stream;
    stream << std::uppercase << std::hex;
    for (auto c : msg)
    {
        stream  << std::setfill('0') << std::setw(2) << (int)c << " ";
    }
    TRACE  <<"Send message:" << stream.str();

    uart_write_blocking(PLAYER_UART, msg, sizeof(msg));
}

void Player::queryCurrentStatus()
{
    TRACE << "Query current status";
    uint8_t msg[] = {0x7E, 0xFF, 0x06, 0x42, 0x00, 0x00, 0x00, 0xFE, 0xB9, 0xEF};
    uart_write_blocking(PLAYER_UART, msg, sizeof(msg));
    {
        TRACE << "Query number of tracks in the root of micro SD card";
        uint8_t msg[] = {0x7E, 0xFF, 0x06, 0x48, 0x00, 0x00, 0x00, 0xFE, 0xB3, 0xEF};
        uart_write_blocking(PLAYER_UART, msg, sizeof(msg));
    }
}