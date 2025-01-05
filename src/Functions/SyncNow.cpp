#include "SyncNow.h"
#include "UiTexts.h"
#include "Clock.h"

void SyncNow::renderFrame(
    Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    renderScrollingText(frame, fullRefresh, uiText(TextId::SyncNow));
}

void SyncNow::activate()
{
    clock().syncNow();

    if (m_nextFunction != nullptr)
        m_nextFunction->select();
}