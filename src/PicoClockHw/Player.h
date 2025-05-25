#pragma once

#include "Timer.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <vector>
#include <functional>

class Player 
{
public:
    enum PlaybackStatus
    {
        Unknown,
        Stopped,
        Playing,
        Paused,
        Sleeping
    };

    static const int MAX_VOLUME = 30;

    Player();
    ~Player();
    bool detected() const;
    void setVolume(int volume); // Volume from 0 to MAX_VOLUME
    void playTrack(int track);
    void playTrackInFolder(int track, int folder);
    void playRandom();
    void stop();
    void queryTotalTrackCount(const std::function<void(int)> &callback);
    void queryStatus(const std::function<void(PlaybackStatus)> &callback);
    
    static Player *instance()
    {
        return m_instance;
    }

private:
    void onUartRx();
    void onRxTimeout();
    void onMsgEnd();
    void sendCommand(uint8_t command, uint8_t param1, uint8_t param2);
    void sendQueuedMessage();

    static Player *m_instance;
    bool m_detected = false;
    std::vector<uint8_t> m_receiveBuffer;
    Timer m_rxTimeoutTimer;
    std::function<void(PlaybackStatus)> m_statusCallback;
    std::function<void(int)> m_trackCountCallback;
};
