#pragma once

#include <string>

enum Language
{
    English = 0,
    French = 1,
    LanguageCount
};

enum class TextId
{
    // Main menu
    Alarms = 0,
    Tools,
    Sync,
    Options,
    Exit,

    // Wifi status. These ids must match those in Wifi::Status
    Unknown,
    OK,
    NotAvailable,
    Down,
    Connecting,
    NoIp,
    Connected,
    ConnectionFailed,
    NoNetworkFound,
    AuthenticationFailed,

    // For alarms
    NextColon,
    AlarmShortened1Colon,
    AlarmShortened2Colon,
    Loud,
    Gradual,
    SkipNextAlarmColon,
    Once,
    Weekly,

    // For options
    AutoLightColon,
    TimeFormatColon,
    Format24h,
    Format12h,
    DateFormatColon,
    HourlyChimeColon,
    AutoScrollColon,
    BrightnessColon,
    BrightnessDarkColon,
    BrightnessDimColon,
    BrightnessBrightColon,
    On,
    Off,
    Day,

    // Tools menu, stopwatch and countdown
    Flashlight,
    Stopwatch,
    Reset,
    Countdown,
    Set,

    // Date formats. Must match the order of Settings::DateFormat.
    MonthDashDay,
    MonthSlashDay,
    DayDashMonth,
    DaySlashMonth,
    DayDotMonth,

    // Sync menu
    SourceColon,
    SyncNow,
    DailySyncTimeColon,
    LastSyncColon,
    LastDriftColon,
    WifiColon,

    // Sync source. Must match Settings::SyncSource
    Rtc,
    Ntp,
    Gps,

    TextCount
};

std::string uiText(TextId id);