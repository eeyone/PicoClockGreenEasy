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

    std::string hexDump(const uint8_t *byteArray, size_t size)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex;
        for (size_t i = 0; i < size; i++)
        {
            stream  << std::setfill('0') << std::setw(2) << (int)byteArray[i] << " ";
        }
        return stream.str();
    }

    std::string hexDump(const std::vector<uint8_t> &data)
    {
        return hexDump(data.data(), data.size());
    }

    template <size_t size>
    std::string hexDump(const uint8_t (&byteArray)[size])
    {
        return hexDump(byteArray, size);
    }

    Player::PlaybackStatus byteToStatus(int param)
    {
        switch(param & 255)
        {
            case 0x01:
                return Player::PlaybackStatus::Playing;
            case 0x02:
                return Player::PlaybackStatus::Paused;
            case 0x00:
                return Player::PlaybackStatus::Stopped;
            case 0x08:
                return Player::PlaybackStatus::Sleeping;
            default:
                return Player::PlaybackStatus::Unknown;
        }
    }
}

Player *Player::m_instance = nullptr;

Player::Player()
{
    assert(m_instance == nullptr); 
    m_instance = this;

    uart_init(PLAYER_UART, 9600);
    gpio_set_function(PLAYER_TX, GPIO_FUNC_UART);
    gpio_set_function(PLAYER_RX, GPIO_FUNC_UART);

    // Set up and enable the interrupt handler
    MAKE_TRAMPOLINE(Player, onUartRx, singleton);
    irq_set_exclusive_handler(uartIrq(PLAYER_UART), onUartRx);
    irq_set_enabled(uartIrq(PLAYER_UART), true);        

    // Enable the UART to send interrupts on reception
    uart_set_irq_enables(PLAYER_UART, true/*has data*/, false/*needs data*/);

    // Query the status of the player to see if it is online so that m_detected will be set.
    queryStatus(nullptr); 
}

Player::~Player()
{
    uart_set_irq_enables(PLAYER_UART, false/*has data*/, false/*needs data*/);
    irq_set_enabled(uartIrq(PLAYER_UART), false);

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
    TRACE << "Timeout waiting for end of message. Received so far:" << hexDump(m_receiveBuffer);

    m_receiveBuffer.clear();
}

std::string Player::receivedCommandAsText() const
{
    int param = m_receiveBuffer[5] << 8 | m_receiveBuffer[6];
    switch(m_receiveBuffer[3])
    {
        case 0x3A: // storage device is plugged in
            switch(param)
            {
                case 0x01:
                    return "USB flash drive plugged in";
                case 0x02:
                    return "SD card plugged in";
                default:
                    return "Unknown storage device plugged in" + std::to_string(param);
            }
            break;
        case 0x3B: // storage device was unplugged
            switch(param)
            {
                case 0x01:
                    return "USB flash drive unplugged";
                case 0x02:
                    return "SD card unplugged";
                default:
                    return "Unknown storage device plugged out" + std::to_string(param);
            }
        case 0x3D: // Track finished playing
            return "Track #" + std::to_string(param) + " finished playing";
        case 0x3F: // Storage device
            switch(param)
            {
                case 0x00:
                    return "No storage device";
                case 0x01:
                    return "Storage device is USB";
                case 0x02:
                    return "Storage device is SD";
                default:
                    return "Unknown storage device" + std::to_string(param);
            }
            break;
        case 0x40: // Error
            switch(param)
            {
                case 0x01:
                    return "Error: module busy";
                case 0x02:
                    return "Error: Currently in sleep mode";
                case 0x03:
                    return "Error: Serial receiving error";
                case 0x04:
                    return "Error: Checksum incorrect";
                case 0x05:
                    return "Error: Specified track is out of current track scope";
                case 0x06:
                    return "Error: Specified track is not found";
                case 0x07:
                    return "Error: Insertion error";
                case 0x08:
                    return "Error: SD card reading failed";
                case 0x0A:
                    return "Error: Entered into sleep mode";
                default:
                    return "Unknown error" + std::to_string(param);
            }
        case 0x41: 
            return "Command acknowledged";
        case 0x42:
        {
            switch(param & 255)
            {
                case 0x01:
                    return "Current status: playing";
                case 0x02:
                    return "Current status: paused";
                case 0x00:
                    return "Current status: Playback finished";
                case 0x08:
                    return "No device online or sleeping";
                default:
                    return "Unknown current status:" + std::to_string(param);
            }
        }
        case 0x43:
            return "Current volume:" + std::to_string(param);
        case 0x48:
            return "Number of tracks in the root of micro SD card: " + std::to_string(param);
        case 0x4C:
            return "Number of folders in the root of micro SD card: " + std::to_string(param);
        default:
            return "Unknown message with command code " 
                + std::to_string(m_receiveBuffer[3])
                + "and parameter" 
                + std::to_string(param);
    }
}

void Player::onMsgEnd()
{
#if 1 // Code to dump the message as hex codes
    TRACE  <<"Received message:" << hexDump(m_receiveBuffer);
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

    TRACE << "Meaning:" << receivedCommandAsText();

    int param = m_receiveBuffer[5] << 8 | m_receiveBuffer[6];
    switch(m_receiveBuffer[3])
    {
        case 0x42: // Status response
        {
            PlaybackStatus status = byteToStatus(m_receiveBuffer[6]);
            if (status != PlaybackStatus::Unknown && m_statusCallback)
            {
                m_statusCallback(status);
                m_statusCallback = nullptr;
            }
            break;
        }
        case 0x48: // Number of tracks in the root of micro SD card
            if (m_trackCountCallback)
            {
                m_trackCountCallback(param);
                m_trackCountCallback = nullptr;
            }
            break;
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

    TRACE  <<"Send message:" << hexDump(msg);

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
