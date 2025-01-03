#include "SyncSource.h"
#include "UiTexts.h"

void SyncSource::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    std::string source = 
        uiText(static_cast<TextId>(
            static_cast<int>(TextId::Rtc) + static_cast<int>(settings().syncSource)));
    if (editedValueIndex == 0)
        renderScrollingText(frame, fullRefresh, uiText(TextId::SourceColon) + source);
    else
        renderScrollingText(frame, fullRefresh, uiText(TextId::SourceColon), source);
}

void SyncSource::modifyValue(int valueIndex, Direction direction)
{
    if (valueIndex == 1)
        adjustEnum(modifySettings().syncSource, direction);
}

void SyncSource::activate()
{
    editValues();

    // Scroll to the right to make sure the user notices that editing started.
    bringScrollingToRight();
}