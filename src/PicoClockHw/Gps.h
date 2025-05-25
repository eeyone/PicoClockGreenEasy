#pragma once

#include "Timer.h"
#include <pico/time.h>
#include <string>
#include <vector>
#include <functional>
#include <time.h>

class Gps
{
public:
    Gps();
    ~Gps();
    void setEnabled(bool enabled);
    bool detected() const;
    void setTimeCallback(std::function<void(time_t utcTime, uint32_t ms)> c)
    {
        m_timeCallback = c;
    }
    void setTimeoutCallback(std::function<void()> c)
    {
        m_timeoutCallback = c;
    }
    static Gps *instance()
    {
        return m_instance;
    }

private:
    void onUartRx();
    void onNmeaMessage(const std::string &msg);
    void onDateTime(const std::string &date, const std::string &time);
    void resetTimeoutAlarm();
    void onTimeout();
    void setBackupMode(bool backupMode);

    static Gps *m_instance;
    bool m_detected = false;
    bool m_enabled = false;
    std::string g_receiveBuffer; // Not printable if the message is an UBX one
    std::function<void(time_t utcTime, uint32_t ms)> m_timeCallback;
    std::function<void()> m_timeoutCallback;
    Timer m_timeoutAlarm;
};