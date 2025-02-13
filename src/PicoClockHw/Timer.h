#pragma once

#include <pico/time.h>
#include <cstdint>
#include <functional>

// Abstraction for the Pico SDK alarm API, called "Timer" to avoid confusion with the Alarm class
class Timer
{
public:
    enum Rescheduling
    {
        Stop,
        RescheduleFromPreviousCall,
        RescheduleFromNow
    };

    Timer()
    {}
    ~Timer();

    void startSingleShot(uint32_t delayMs, std::function<void()> callback);
    void startRepeatable(uint32_t delayMs, std::function<Rescheduling()> callback);
    void stop();

private : 
    alarm_id_t m_alarmId = -1;
    std::function <void()> m_callback;
    std::function <Rescheduling()> m_repeatableCallback;
    uint32_t m_delayMs = 0;

    static int64_t onAlarm(alarm_id_t id, void *user);
    static int64_t onRepeatableAlarm(alarm_id_t id, void *user);
};
