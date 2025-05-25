#include "SyncSource.h"
#include "UiTexts.h"

void SyncSource::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    std::string source = uiText(settings().syncSource);
    if (editedValueIndex == 0)
        renderScrollingText(frame, fullRefresh, uiText(TextId::SourceColon) + source);
    else
        renderScrollingText(frame, fullRefresh, uiText(TextId::SourceColon), source);
}

void SyncSource::modifyValue(int valueIndex, Direction direction)
{
    if (valueIndex == 1)
    {
        do {
            adjustEnum(modifySettings().syncSource, direction);
            // Only allow selecting GPS if the corresponding hardware is detected. Wifi can always
            // be selected, because it is difficult to realiably detect if the Pico supports Wifi.
        } while (settings().syncSource == Settings::SyncSource::Gps && !clock().gpsDetected());
    }
}

void SyncSource::activate()
{
    editValues();

    // Scroll to the right to make sure the user notices that editing started.
    bringScrollingToRight();
}