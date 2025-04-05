#include "Flashlight.h"
#include "UiTexts.h"
#include "ClockUi.h"

void Flashlight::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    if (editedValueIndex == 0)
    {
        renderScrollingText(frame, fullRefresh, uiText(TextId::Flashlight));
    }
}

void Flashlight::startEditingValue(int valueIndex)
{
    if (valueIndex == 1)
    {
        // Turn on the flashlight (which turns off the display) and set its brightness.
        display().setFlashlightMode(true);
        display().setBrightness(settings().flashlightBrightness);
    }
}

void Flashlight::modifyValue(int valueIndex, Direction direction)
{
    int &brightness = modifySettings().flashlightBrightness;
    if (direction == Up || direction == RepeatedUp)
    {
        brightness = std::min(brightness + 10, 100);
    }
    else if (direction == Down || direction == RepeatedDown)
    {
        brightness = std::max(brightness - 10, 10);
    }

    display().setBrightness(brightness);
}

void Flashlight::finishEditing()
{
    // Turn off the flashlight and turn on the display
    display().setFlashlightMode(false);
}