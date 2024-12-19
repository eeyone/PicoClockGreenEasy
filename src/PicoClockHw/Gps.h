#pragma once

#include <string>
#include <functional>

class Gps
{
public:
    Gps();
    ~Gps();
    void setEnabled(bool enabled);
    void setTimeCallback(std::function<void(time_t utcTime, uint32_t ms)> c)
    {
        m_timeCallback = c;
    }

private:
    static void onUartRx();
    void onMessage(const std::string &msg);
    void onDateTime(const std::string &date, const std::string &time);

    static Gps *m_instance;
    std::string g_receiveBuffer;
    std::function<void(time_t utcTime, uint32_t ms)> m_timeCallback;
};