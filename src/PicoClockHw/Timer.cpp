#include "Timer.h"
#include "Utils/Trace.h"

Timer::~Timer()
{
    if (m_alarmId != -1)
        cancel_alarm(m_alarmId);
}

void Timer::startSingleShot(uint32_t delayMs, std::function<void()> callback)
{
    if (m_alarmId != -1)
        cancel_alarm(m_alarmId);

    m_callback = callback;
    m_alarmId = add_alarm_in_ms(delayMs, &Timer::onAlarm, this, true /* fire if past*/);
}

void Timer::startRepeatable(uint32_t delayMs, std::function<Rescheduling()> callback) 
{
    if (m_alarmId != -1)
        cancel_alarm(m_alarmId);

    m_delayMs = delayMs;
    m_repeatableCallback = callback;
    m_alarmId = add_alarm_in_ms(delayMs, &Timer::onRepeatableAlarm, this, true /* fire if past*/);
}

void Timer::stop() 
{
    if (m_alarmId != -1) 
    {
        cancel_alarm(m_alarmId);
        m_alarmId = -1;
    }
}

int64_t Timer::onAlarm(alarm_id_t id, void *user)
{
    Timer &obj = *static_cast<Timer *>(user);
    obj.m_callback();
    obj.m_alarmId = -1;
    return 0; // Do not reschedule
}

int64_t Timer::onRepeatableAlarm(alarm_id_t id, void *user)
{
    Timer &obj = *static_cast<Timer *>(user);
    switch (obj.m_repeatableCallback())
    {
        case RescheduleFromPreviousCall:
            TRACE << "Reschedule the same alarm"
                  << obj.m_delayMs 
                  << "ms from the time the alarm was previously scheduled to fire";
            return static_cast<int64_t>(obj.m_delayMs) * -1000;
        case RescheduleFromNow:
            return static_cast<int64_t>(obj.m_delayMs) * 1000;
        default:
            obj.m_alarmId = -1;
            return 0;
    }
}