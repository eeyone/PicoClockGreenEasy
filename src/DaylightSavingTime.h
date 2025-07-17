#pragma once

#include <time.h>

class DaylightSavingTime
{
    friend class TestDaylightSavingTime;

public:
    enum Location
    {
        Unknown,
        Europe,
        Usa,
        USA = Usa,
    };

	DaylightSavingTime(float utcOffset) : m_utcOffset(utcOffset)
	{}
    time_t considerDst(time_t time);
    time_t unconsiderDst(time_t time);

private:
    bool isDstActive(time_t time);
    void determineDstStartAndEnd(const tm &givenTm);

	const float m_utcOffset;
	time_t m_yearStart = 0;
    time_t m_dstStart = 0;
    time_t m_dstEnd = 0;
    bool m_wasDstActive = false;
};