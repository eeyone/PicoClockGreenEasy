#pragma once

#include "AbstractFunction.h"

class Bitmap;

class Flashlight : public AbstractFunction
{
public:
    Flashlight(ClockUi *clockUi) : AbstractFunction(clockUi)
    {}

private:
    void renderFrame(
        Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh) override;
    int valueCount() const override
    {
        // The "Flashlight" label and the status in which the flashlight is on (which turns off the 
        // display)
        return 2; 
    }
    void startEditingValue(int valueIndex) override;
    void modifyValue(int valueIndex, Direction direction) override;
    void finishEditing() override;
    BrightnessHandling brightnessHandling(int valueIndex) const override
    {
        // Tell not to regulate brightness while the flashlight is on.
        return valueIndex == 1 ? NoBrightnessHandling : WithBoost;
    }
};
