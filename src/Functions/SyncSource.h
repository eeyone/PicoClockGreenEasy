#pragma once

#include "AbstractFunction.h"

class SyncSource : public AbstractFunction
{
public:
    SyncSource(ClockUi *clockUi) : AbstractFunction(clockUi)
    {}

private:
    void renderFrame(
        Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh) override;
    int valueCount() const override
    {
        return 2;
    }
    void modifyValue(int valueIndex, Direction direction) override;
    void activate() override;
};