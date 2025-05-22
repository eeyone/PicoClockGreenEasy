#include "SyncInfo.h"
#include "Clock.h"
#include "UiTexts.h"
#include <sstream>

namespace
{
    std::string to2DigitsString(int number)
    {
        std::string s = std::to_string(number);
        if (s.size() == 1)
            s = '0' + s;
        return s;
    }
}

void SyncInfo::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh) 
{
    Clock::SyncInfo info;
    clock().syncInfo(info);
    std::string sourceText = uiText(info.lastSyncSource);

    std::string text;
    bool morning;
    switch (m_entry)
    {
        case DailySyncTime:
            text = 
                uiText(TextId::DailySyncTimeColon) + 
                timeToString(info.dailySyncHour, info.dailySyncMin, morning);
            putAmPmIndicators(frame, morning);
            break;
        case LastSyncTimestamp:        
            text =
                uiText(TextId::LastSyncColon) + timeToString(info.lastSyncTm, morning) +
                " " + dateToString(info.lastSyncTm) +
                " (" + sourceText + ")";
            putAmPmIndicators(frame, morning);
            break;
        case LastSyncDrift:
            text = uiText(TextId::LastDriftColon) + std::to_string(info.lastSyncDriftMs) + " ms";
            break;
    }

    renderScrollingText(frame, fullRefresh, text);
}

std::string SyncInfo::timeToString(int hour, int min, bool &morning) const
{
    int displayedHour;
    convertHour(hour, displayedHour, morning);

    return std::to_string(displayedHour) + ":" + to2DigitsString(min);
}

std::string SyncInfo::timeToString(const tm &tm, bool &morning) const
{
    int displayedHour;
    convertHour(tm.tm_hour, displayedHour, morning);

    return 
        std::to_string(displayedHour) + 
        ":" + 
        to2DigitsString(tm.tm_min) + 
        ":" + 
        to2DigitsString(tm.tm_sec);
}

std::string SyncInfo::dateToString(const tm &tm) const
{
    std::string month = to2DigitsString(tm.tm_mon + 1);
    std::string day = to2DigitsString(tm.tm_mday);

    switch(settings().dateFormat)
    {
        case Settings::DateFormat::MonthDashDay:
            return month + "-" + day;
        case Settings::DateFormat::MonthSlashDay:
            return month + "/" + day;
        case Settings::DateFormat::DayDashMonth:
            return day + "-" + month;
        case Settings::DateFormat::DaySlashMonth:
            return day + "/" + month;
        case Settings::DateFormat::DayDotMonth:
            return day + "." + month;
    }

    return "";
}