#pragma once

#include "AbstractFunction.h"

class SyncInfo : public AbstractFunction
{
public:
    enum Entry
    {
        DailySyncTime,
        LastSyncTimestamp,
        LastSyncDrift
    };
    SyncInfo(ClockUi *clockUi, Entry entry) : AbstractFunction(clockUi), m_entry(entry)
    {}

private:
    void renderFrame(
        Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh) override;
    int valueCount() const override
    {
        return 1; // Nothing to edit, the information is read only
    }
    std::string timeToString(int hour, int min, bool &morning) const;
    std::string timeToString(const tm &tm, bool &morning) const;
    std::string dateToString(const tm &tm) const;

    Entry m_entry;
};