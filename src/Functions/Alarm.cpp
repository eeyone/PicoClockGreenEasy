#include "Alarm.h"

#include "UiTexts.h"
#include "fonts.h"
#include "Bitmap.h"
#include "Clock.h"
#include "PicoClockHw/Player.h"

namespace
{
    std::string alarmModeText(const Settings::Alarm &alarm)
    {
        TextId textId;
        switch(alarm.mode)
        {
            // Todo: find a way to avoid such mapping switches and casts between enums
            case Settings::AlarmMode::Off:   
                textId = TextId::Off;
                break;
            case Settings::AlarmMode::Loud:   
                textId = TextId::Loud;
                break;
            case Settings::AlarmMode::Gradual:
                textId = TextId::Gradual;
                break;
            case Settings::AlarmMode::Music:
                textId = TextId::Music;
                break;
            default:
                return "";
        }
        return uiText(textId);
    }
}

void Alarm::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    std::string prefix = 
        m_alarmId == Clock::Alarm1 ? uiText(TextId::AlarmShortened1Colon) : uiText(TextId::AlarmShortened2Colon);
    auto &alarm = 
        m_alarmId == Clock::Alarm1 ? settings().alarm1 : settings().alarm2;

    int displayedHour;
    bool morning;
    convertHour(alarm.hour, displayedHour, morning);
    std::string hourS = std::to_string(displayedHour);
    
    // Add a leading space to hours, but not in view mode for better readability
    if (hourS.size() == 1 && editedValueIndex != NoEditing)
    {
        // Use a fix width space to avoid shifting of characters when changing numbers.
        hourS = std::string(1, FixedWidthSpace) + hourS;
    }

    std::string minS = std::to_string(alarm.min);
    if (minS.size() == 1)
        minS = '0' + minS;

    switch (editedValueIndex)
    {
        case NoEditing:
            if (alarm.mode == Settings::AlarmMode::Off)
                renderScrollingText(frame, fullRefresh, prefix + uiText(TextId::Off));
            else
            {
                renderScrollingText(frame, fullRefresh, prefix + hourS + ":" + minS);
                putAmPmIndicators(frame, morning);
            }
            break;
        case EditingAlarmMode:
            renderScrollingText(frame, fullRefresh, prefix, alarmModeText(alarm));
            break;
        case EditingAlarmVolume:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::VolumeColon), 
                std::to_string(settings().alarmMusic[m_alarmId].volume));
            break;
        case EditingAlarmTrack:
            if (settings().alarmMusic[m_alarmId].track == 0)
                renderScrollingText(
                    frame, fullRefresh, uiText(TextId::TrackColon), uiText(TextId::Random));
            else
                renderScrollingText(
                    frame,
                    fullRefresh,
                    uiText(TextId::TrackColon),
                    std::to_string(settings().alarmMusic[m_alarmId].track),
                    "/" + std::to_string(m_trackCount));
            break;
        case EditingAlarmHour:
            renderScrollingText(frame, fullRefresh, prefix, hourS, ":" + minS);
            putAmPmIndicators(frame, morning);
            break;
        case EditingAlarmMinute:
            renderScrollingText(frame, fullRefresh, prefix + hourS + ":", minS);
            putAmPmIndicators(frame, morning);
            break;
        case EditingAlarmWeekDays:
            std::string frequency = 
                alarm.ringsOnce() ? uiText(TextId::Once) : uiText(TextId::Weekly);
            renderScrollingText(frame, fullRefresh, prefix + frequency);
            putAmPmIndicators(frame, morning);
            break;
    }
    
    if (alarm.mode == Settings::AlarmMode::Off)
        frame.putWeekDays(0);
    else
        frame.putWeekDays(alarm.weekDayBits);
    
    if (editedValueIndex == EditingAlarmWeekDays)
    {
        bool on = blinkingCounter < BLINKING_DISAPPEAR_FRAME / 2;

        if (alarm.enabledOnWeekDay(m_editedAlarmWeekDay))
            on = !on;

        frame.putWeekDay(m_editedAlarmWeekDay, on);
    }
}

int Alarm::valueCount() const
{
    if (alarmSettings().mode == Settings::AlarmMode::Off)
    {
        // As the alarm is off, only the mode can be edited. Time/weekdays values are not accessible.
        return 2;
    } else
    {
        return ValueCount;
    }
}

void Alarm::startEditingValue(int valueIndex)
{
    switch (valueIndex)
    {
        case EditingAlarmTrack:
            player().queryTotalTrackCount(
                [this](int trackCount){m_trackCount = trackCount;});
            break;
        case EditingAlarmHour:
            bringScrollingToRight();
            player().stop();
            break;
        case EditingAlarmMode:
        case EditingAlarmMinute:
            bringScrollingToRight();
            break;
        case EditingAlarmWeekDays:
            m_editedAlarmWeekDay = 1; // Monday
            break;
    }
}

bool Alarm::isValueAvailable(int valueIndex) const
{
    // Volume and track are only available when the alarm mode is music.
    return 
        alarmSettings().mode == Settings::AlarmMode::Music || 
        valueIndex != EditingAlarmVolume &&
        valueIndex != EditingAlarmTrack;
}

const Settings::Alarm &Alarm::alarmSettings() const
{
    if (m_alarmId == Clock::Alarm1)
        return settings().alarm1;
    else
        return settings().alarm2;
}

Settings::Alarm &Alarm::modifyAlarmSettings()
{
    if (m_alarmId == Clock::Alarm1)
        return modifySettings().alarm1;
    else
        return modifySettings().alarm2;
}

void Alarm::modifyValue(int valueIndex, Direction direction)
{
    switch(valueIndex)
    {
    case EditingAlarmMode:
        adjustEnum(modifyAlarmSettings().mode, direction);
        break;

    case EditingAlarmVolume:
        adjustField(
            direction, 
            PlayerVolume, 
            modifySettings().alarmMusic[m_alarmId].volume, 
            Player::MAX_VOLUME);

        player().setVolume(settings().alarmMusic[m_alarmId].volume);

        // Start playing track if not playing yet (deferred in the lambda expression as the answer 
        // from the module is needed)
        player().queryStatus([this](Player::PlaybackStatus status)
            {
                if (status != Player::Playing)
                    playAlarmMusic(m_alarmId);
            });

        break;

    case EditingAlarmTrack:
        adjustField(
            direction, PlayerTrack, modifySettings().alarmMusic[m_alarmId].track, m_trackCount);

        player().setVolume(settings().alarmMusic[m_alarmId].volume);
        playAlarmMusic(m_alarmId);
        break;

    case EditingAlarmHour:
        adjustField(
            direction, 
            Hour, 
            m_alarmId == Clock::Alarm1 ? modifySettings().alarm1.hour : modifySettings().alarm2.hour);
        break;

    case EditingAlarmMinute:
        adjustField(
            direction, 
            Minute, 
            m_alarmId == Clock::Alarm1 ? modifySettings().alarm1.min : modifySettings().alarm2.min);
        break;
    
    case EditingAlarmWeekDays:
        if (direction == Up || direction == RepeatedUp)
        {
            // Middle button cycles through weekdays
            m_editedAlarmWeekDay.increment();
        } else
        {
            // Bottom button toggle the selected weekday
            modifyAlarmSettings().weekDayBits ^= 1 << m_editedAlarmWeekDay;
        }
        break;
    }
}
