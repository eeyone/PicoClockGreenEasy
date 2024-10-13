#include "Clock.h"
#include "Utils/Trace.h"

Clock::Clock(int tickPerSec, Settings &settings) : 
    m_tickCount(tickPerSec), 
    m_settings(settings),
    m_rtc(std::make_unique<Rtc>()), 
    m_ntp(std::make_unique<Ntp>())
{
    // Initialize m_time and m_tm from the RTC. It will be used for displaying time 
    // while waiting for the sync from the RTC to be finished.
    tm rtcTime;
    if (m_rtc->read(rtcTime))
    {
        setFromNonDstConsideringTm(rtcTime);
        m_rtcSync = SyncingFromRtc;

        TRACE << "Set m_lastRtcSec to be able to detect when the second changes in the RTC";
        m_lastRtcSec = rtcTime.tm_sec;
    } else
    {
        TRACE << "No RTC available";
        m_rtc.release();
        m_rtcSync = SyncDone;
    }
}

void Clock::startSyncFromNtp()
{
    if (!m_ntp)
        return;

    if (!m_ntp->init())
    {
        // Will not use NTP.
        m_ntp.release();
        return;
    }

    using namespace std::placeholders;
    m_ntp->setTimeCallback(std::bind(&Clock::onNtpTimeReceived, this, _1, _2));
    m_ntp->startRequest();
}

void Clock::onNtpTimeReceived(time_t utcTime, uint32_t ms)
{
    TRACE << "Received ntp time:" << utcTime <<", setting it";
    m_time = utcTime + UTC_OFFSET * 60 * 60;
    setTmFromTime();
    m_tickCount = ms * m_tickCount.wrapValue() / 1000;
    m_clockAdjusted = true;

    // Now that we got the time from NTP, plan RTC sync at the next second change.
    m_rtcSync = SyncingToRtc;
}

void Clock::tick(bool &clockAdjusted, Settings::AlarmMode &reachedAlarmMode)
{
    clockAdjusted = false;
    reachedAlarmMode = Settings::AlarmMode::Off;

    m_tickCount.increment();

    if (m_rtc && m_rtcSync == SyncingFromRtc) // RTC available and synchronizing with it?
    {
        TRACE << "Synchronizing with RTC";
        tm rtcTime;
        if (m_rtc->read(rtcTime))
        {
            if (rtcTime.tm_sec != m_lastRtcSec)
            {
                TRACE << "done";
                // The second just changed in the RTC, synchronize.
                setFromNonDstConsideringTm(rtcTime);
                m_tickCount = 0;
                m_rtcSync = SyncDone;
            }
        } else
        {
            // RTC read failed, give up with synchronization
            m_rtcSync = SyncDone;
        }
    } else
    {
        // Count time in the program. No longer read from the RTC. Update it if needed.
        if (m_tickCount == 0)
        {
            m_time++;
            setTmFromTime();

            if (m_rtc && m_rtcSync == SyncingToRtc)
            {
                TRACE << "Set RTC";

                // To avoid ambiguity, save the time without DST consideration into the RTC. Thus,
                // on the next start, m_dst will be able to determine if DST is active only by 
                // looking at the time and date.
                tm tm = *localtime(&m_time);

                if (m_rtc->write(tm))
                    m_rtcSync = SyncDone;
            }
        }
    }   

    if (m_tickCount == 0 && m_tm.tm_sec == 0)
    {
        const struct Settings::Alarm *reachedAlarm = nullptr;
        if (alarmReached(Alarm1))
        {
            reachedAlarm = &m_settings.get().alarm1;
            reachedAlarmMode = m_settings.get().alarm1.mode;
        } else if (alarmReached(Alarm2))
        {
            reachedAlarm = &m_settings.get().alarm2;
            reachedAlarmMode = m_settings.get().alarm2.mode;
        }

        if (reachedAlarm != nullptr)
        {
            // Disable the alarm if it has to ring only once.
            if (reachedAlarm->ringsOnce())
            {
                struct Settings::Alarm &alarm = 
                    reachedAlarm == 
                        &m_settings.get().alarm1 ? m_settings.modify().alarm1 : m_settings.modify().alarm2;

                alarm.mode = Settings::AlarmMode::Off;
            }

            if (m_settings.get().skipNextAlarm)
            {
                // This alarm must be skipped. Disable the "skip next alarm" function and report 
                // that no alarm was reached.
                m_settings.modify().skipNextAlarm = false;
                reachedAlarmMode = Settings::AlarmMode::Off;
            }
        }
    }

    if (m_clockAdjusted)
    {
        m_clockAdjusted = false;
        clockAdjusted = true;
    } 
}

void Clock::setTmFromTime()
{
    time_t timeConsideringDst = m_dst.considerDst(m_time);
    m_tm = *localtime(&timeConsideringDst);
    // TRACE << "It is" << m_tm;
}

void Clock::set(const tm &tm)
{
    TRACE << "Set clock to" << tm;
    m_tm = tm;

    // If DST is active, unapply it so that the time stays as it was set by the user, as the DST 
    // offset will be readded each time setTmFromTime() is called.
    m_time = m_dst.unconsiderDst(mktime(&m_tm));

    m_clockAdjusted = true;
}

// tm is passed by copy so that its address can be passed to mktime
void Clock::setFromNonDstConsideringTm(tm tm)
{
    m_time = mktime(&tm);
    setTmFromTime();

    m_clockAdjusted = true;
}

bool Clock::nextAlarm(int &weekday, int &hour, int &min) const
{
    // Keep track of which alarms would be enabled, as alarms that ring only once disable 
    // themselves.
    unsigned int enabledAlarmsMask = 0;
    if (alarm(Alarm1).mode != Settings::AlarmMode::Off)
        enabledAlarmsMask |= 1;
    if (alarm(Alarm2).mode != Settings::AlarmMode::Off)
        enabledAlarmsMask |= 2;

    // Figure out what is the next alarm after now
    Time time;
    if (!nextAlarmAfter(
            m_tm.tm_wday, {m_tm.tm_hour, m_tm.tm_min}, weekday, time, enabledAlarmsMask))
        return false; // No alarm enabled

    // If this alarm will be skipped, get the next alarm after it
    if (m_settings.get().skipNextAlarm)
    {
        if (!nextAlarmAfter(weekday, time, weekday, time, enabledAlarmsMask))
            return false; // No other alarm enabled
    }

    // deliver the result
    hour = time.hour;
    min = time.min;
    return true;
}

bool Clock::nextAlarmAfter(
        int startWeekday, 
        const Time &startTime, 
        int &weekday, 
        Time &time, 
        unsigned int &enabledAlarmsMask) const
{
    // Check if there is an alarm later on the start day
    Time t1 = alarmTimeAtDay(Alarm1, startWeekday, enabledAlarmsMask);
    if (t1 <= startTime)
    {
        TRACE << "Alarm 1 already passed today";
        t1 = Time();
    }
    Time t2 = alarmTimeAtDay(Alarm2, startWeekday, enabledAlarmsMask);
    if (t2 <= startTime)
    {
        TRACE << "Alarm 2 already passed today";
        t2 = Time();
    }

    // Return the earliest reacheable alarm on the start day if one was found.
    if (earliestAlarm(t1, t2, time, enabledAlarmsMask))
    {
        weekday = startWeekday;
        return true;
    }

    // Check the following 7 weekdays, wrapping around to the same weekday in 7 days
    CyclicCounter currentWeekDay{7, startWeekday};
    for (int i = 0; i < 7; i++)
    {
        currentWeekDay.increment();
        t1 = alarmTimeAtDay(Alarm1, currentWeekDay, enabledAlarmsMask);
        t2 = alarmTimeAtDay(Alarm2, currentWeekDay, enabledAlarmsMask);

        // Return the earliest reacheable alarm on this day if one was found.
        if (earliestAlarm(t1, t2, time, enabledAlarmsMask))
        {
            weekday = currentWeekDay;
            return true;
        }
    }

    // No next alarm found
    weekday = -1;
    return false;
}

bool Clock::earliestAlarm(
    const Time &alarm1Time, 
    const Time &alarm2Time, 
    Time &earliestTime,  
    unsigned int &enabledAlarmsMask) const
{
    if (alarm1Time.isValid() && alarm1Time <= alarm2Time)
    {
        earliestTime = alarm1Time;
        if (alarm(Alarm1).ringsOnce())
        {
            // This alarm would disable itself, even if "skip next alarm" is used
            enabledAlarmsMask &= ~1;
        }
        return true;
    }

    if (alarm2Time.isValid())
    {
        earliestTime = alarm2Time;
        if (alarm(Alarm2).ringsOnce())
        {
            // This alarm would disable itself, even if "skip next alarm" is used
            enabledAlarmsMask &= ~2;
        }
        return true;
    }

    return false;
}

Clock::Time Clock::alarmTimeAtDay(
    AlarmId alarmId, int weekday, unsigned int enabledAlarmsMask) const
{
    if ((enabledAlarmsMask & (1<<alarmId)) &&
        (alarm(alarmId).ringsOnce() || alarm(alarmId).enabledOnWeekDay(weekday)))
    {
        TRACE 
            << "Alarm" 
            << alarmId + 1 
            << "will ring on day" 
            << weekday 
            << "at" 
            << alarm(alarmId).hour 
            << ":" 
            << alarm(alarmId).min;
        return {alarm(alarmId).hour, alarm(alarmId).min};
    }
    else
        return Time();
}

bool Clock::alarmReached(AlarmId id) const
{
    const Settings::Alarm &al = alarm(id);
    return 
        m_tm.tm_min == al.min && 
        m_tm.tm_hour == al.hour && 
        (al.ringsOnce() || al.enabledOnWeekDay(m_tm.tm_wday)) && 
        al.mode != Settings::AlarmMode::Off;
}

const Settings::Alarm &Clock::alarm(AlarmId id) const
{
    if (id == Alarm1)
        return m_settings.get().alarm1;
    else
        return m_settings.get().alarm2;
}

bool Clock::Time::operator <(const Time &other) const
{
    if (hour < other.hour)
        return true;
    if (hour > other.hour)
        return false;
    return min < other.min;
}

bool Clock::Time::operator <=(const Time &other) const
{
    return !(other < *this);
}

bool Clock::Time::isValid() const
{
    return hour >= 0 && hour < 24 && min >= 0 && min < 60;
}
