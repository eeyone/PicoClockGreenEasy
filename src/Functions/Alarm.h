#pragma once

#include "AbstractFunction.h"

class Bitmap;

class Alarm : public AbstractFunction
{
public:   
    Alarm(ClockUi *clockUi, Clock::AlarmId id) : AbstractFunction(clockUi), m_alarmId(id)
    {}

private:
    void renderFrame(
        Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh) override;
    int valueCount() const override;
    void startEditingValue(int valueIndex) override;
    void modifyValue(int valueIndex, Direction direction) override;
    bool isValueAvailable(int valueIndex) const override;
  
    const Settings::Alarm &alarmSettings() const;
    Settings::Alarm &modifyAlarmSettings();

    enum EditableValue
    {
        NoEditing = 0,
        EditingAlarmMode,
        EditingAlarmVolume,
        EditingAlarmTrack,
        EditingAlarmHour,
        EditingAlarmMinute,
        EditingAlarmWeekDays,
        ValueCount 
    };

    const Clock::AlarmId m_alarmId;
    CyclicCounter m_editedAlarmWeekDay {7, 0};
    int m_trackCount = 0;
};