// Based on these datasheets:
// https://www.electronicoscaldas.com/datasheet/DFR0299-DFPlayer-Mini-Manual.pdf?srsltid=AfmBOoq-OtiGXRcKGhYnv_vwpQqS3vaBDtOVyyfwmSWj7EP0cMv1CriK
// http://www.trainelectronics.com/Arduino/MP3Sound/TalkingTemperature/FN-M16P%20Embedded%20MP3%20Audio%20Module%20Datasheet.pdf

// TODO: check power consumption of the module and if it is reduced by entering standby mode using
// command 0x0A. 

#include "Player.h"
#include "gpio.h"
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
    assert(m_instance == nullptr); 
    m_instance = this;

    uart_init(PLAYER_UART, 9600);
    gpio_set_function(PLAYER_TX, GPIO_FUNC_UART);
    gpio_set_function(PLAYER_RX, GPIO_FUNC_UART);

    // TODO: centralize this in a common place, as Gps.cpp has similar code
    // Select correct interrupt for the UART we are using
    const int UART_IRQ = PLAYER_UART == uart0 ? UART0_IRQ : UART1_IRQ;

    // Set up and enable the interrupt handler
    MAKE_TRAMPOLINE(Player, onUartRx, singleton);
    irq_set_exclusive_handler(UART_IRQ, onUartRx);
    irq_set_enabled(UART_IRQ, true);        

    // Enable the UART to send interrupts on reception
    uart_set_irq_enables(PLAYER_UART, true/*has data*/, false/*needs data*/);

    // Query the status of the player to see if it is online so that m_detected will be set.
    queryStatus(nullptr); 
}

Player::~Player()
{
    uart_set_irq_enables(PLAYER_UART, false/*has data*/, false/*needs data*/);
    const int UART_IRQ = PLAYER_UART == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_enabled(UART_IRQ, false);

    uart_deinit(PLAYER_UART);
    m_instance = nullptr;
}

bool Player::detected() const
{
    TRACE << "Player detected:" << m_detected;
    return m_detected;
}

void Player::onUartRx()
{
    TRACE << "DF Player UART RX interrupt";
    while (uart_is_readable(PLAYER_UART)) 
    {
        uint8_t c = uart_getc(PLAYER_UART);
        m_receiveBuffer.push_back(c);

        if (m_receiveBuffer.size() >= 10) // End of message
        {
            // Now that a message is received, the player module is considered detected. (if the
            // module is not present, one zero byte is still received)
            m_detected = true;

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
    // TODO: move this and other similar snippets to Trace()
#ifdef TRACE_TO_STDIO
    std::ostringstream stream;
    stream << std::uppercase << std::hex;
    for (auto c : m_receiveBuffer)
        stream  << std::setfill('0') << std::setw(2) << (int)c << " ";
#endif

    TRACE << "Timeout waiting for end of message. Received so far:" << stream.str();

    m_receiveBuffer.clear();
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
        case 0x41: 
            TRACE << "Command acknowledged";
//            sendQueuedMessage();
            break;
        case 0x42:
        {
            PlaybackStatus status = PlaybackStatus::Unknown;
            switch(param & 255)
            {
                case 0x01:
                    TRACE << "Current status: playing";
                    status = PlaybackStatus::Playing;
                    break;
                case 0x02:
                    TRACE << "Current status: paused";
                    status = PlaybackStatus::Paused;
                    break;
                case 0x00:
                    TRACE << "Current status: Playback finished";
                    status = PlaybackStatus::Stopped;
                    break;
                case 0x08:
                    TRACE << "No device online or sleeping";
                    status = PlaybackStatus::Sleeping;
                    break;
                default:
                    TRACE << "Unknown current status:" << param;
            }

            if (status != PlaybackStatus::Unknown && m_statusCallback)
            {
                m_statusCallback(status);
                m_statusCallback = nullptr;
            }

            break;
        }
        case 0x43:
            TRACE << "Current volume:" << param;
            break;
        case 0x48:
            TRACE << "Number of tracks in the root of micro SD card:" << param;
            if (m_trackCountCallback)
            {
                m_trackCountCallback(param);
                m_trackCountCallback = nullptr;
            }
            break;
        case 0x4C:
            TRACE << "Number of folders in the root of micro SD card:" << param;
            break;
        default:
            TRACE << "Unknown message with command code" 
                << std::uppercase << std::hex
                << (int)m_receiveBuffer[3] 
                << "and parameter" 
                << param;
    }
}

void Player::setVolume(int volume)
{
    sendCommand(0x06, 0x00, volume);
}

void Player::playTrack(int track)
{
    sendCommand(0x03, 0x00, track);
}

void Player::playTrackInFolder(int track, int folder)
{
    sendCommand(0x0F, folder, track);
}

void Player::playRandom()
{
    sendCommand(0x18, 0x00, 0x00);
}

void Player::stop()
{
    sendCommand(0x16, 0x00, 0x00);
}

void Player::sendCommand(uint8_t command, uint8_t param1, uint8_t param2)
{
    TRACE << "Send command" << std::hex << (int)command << "with parameters" << (int)param1 <<"and" << (int)param2;
    uint8_t msg[] = {0x7E, 0xFF, 0x06, command, 1, param1, param2, 0, 0, 0xEF};

    // Calculate checksum
    uint16_t checksum = 0;
    for (int i = 1; i < sizeof(msg) - 3; i++)
    {
        checksum -= msg[i];
    }
    msg[7] = checksum >> 8; // High byte
    msg[8] = checksum & 0xFF; // Low byte

#ifdef TRACE_TO_STDIO
    std::ostringstream stream;
    stream << std::uppercase << std::hex;
    for (auto c : msg)
    {
        stream  << std::setfill('0') << std::setw(2) << (int)c << " ";
    }
    TRACE  <<"Send message:" << stream.str();
#endif

    uart_write_blocking(PLAYER_UART, msg, sizeof(msg));
}

void Player::queryStatus(const std::function<void(PlaybackStatus)> &callback)
{
    TRACE << "Query current status";
    sendCommand(0x42, 0, 0);
    m_statusCallback = callback;
}

void Player::queryTotalTrackCount(const std::function<void(int)> &callback)
{
    TRACE << "Query total track count";
    sendCommand(0x48, 0x00, 0x00);
    m_trackCountCallback = callback;
}
