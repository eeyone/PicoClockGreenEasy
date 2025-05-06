#pragma once

#include "Timer.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <vector>

class Player 
{
public:
    Player();
    void playTrackInFolder(int track, int folder);
    // TODO: remove unused/test methods
    void play();
    void queryCurrentStatus();
    void queryFolderCount();
    void selectDevice();
    void setVolume(int volume);

    static Player *instance()
    {
        return m_instance;
    }

private:
    void onUartRx();
    void onRxTimeout();
    void onMsgEnd();
    void sendCommand(uint8_t command, uint8_t param1, uint8_t param2);

    static Player *m_instance;
    std::vector<uint8_t> m_receiveBuffer;
    Timer m_rxTimeoutTimer;
};
