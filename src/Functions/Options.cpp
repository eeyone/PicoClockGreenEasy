#include "Options.h"
#include "UiTexts.h"
#include "Utils/Trace.h"
#include "PicoClockHw/Player.h"

namespace
{
    void toggleBool(bool &option)
    {
        option = !option;
    }
}

void Options::renderFrame(Bitmap &frame, int editedValueIndex, int blinkingCounter, bool fullRefresh)
{
    switch(editedValueIndex)
    {
        case NoEditing:
            renderScrollingText(frame, fullRefresh, uiText(TextId::Options));
            break;
        case EditingAutoScroll:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::AutoScrollColon), 
                settings().autoScroll ? uiText(TextId::On) : uiText(TextId::Off));
            break;
        case EditingTimeFormat:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::TimeFormatColon), 
                settings().format24h ? uiText(TextId::Format24h) : uiText(TextId::Format12h));
            break;
        case EditingDateFormat:
        {
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::DateFormatColon), 
                uiText(settings().dateFormat));
            break;
        }
        case EditingHourlyChimeMode:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::HourlyChimeColon), 
                uiText(settings().hourlyChimeMode));
            break;
        case EditingChimeVolume:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::ChimeVolumeColon), 
                std::to_string(settings().chimeSoundVolume));
            break;
        case EditingAutoLight:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::AutoLightColon), 
                settings().autoLight ? uiText(TextId::On) : uiText(TextId::Off));
            break;
        case EditingManualBrightness:
            if (settings().autoLight)
            {
                renderScrollingText(
                    frame, 
                    fullRefresh, 
                    uiText(TextId::BrightnessDarkColon), 
                    std::to_string(settings().brightnessDark) + "%");
            } else
            {
                renderScrollingText(
                    frame, 
                    fullRefresh, 
                    uiText(TextId::BrightnessColon), 
                    std::to_string(settings().manualBrightness) + "%");
            }
            break;
        case EditingBrightnessDim:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::BrightnessDimColon), 
                std::to_string(settings().brightnessDim) + "%");
            break;
        case EditingBrightnessBright:
            renderScrollingText(
                frame, 
                fullRefresh, 
                uiText(TextId::BrightnessBrightColon), 
                std::to_string(settings().brightnessBright) + "%");
            break;
    }
}

int Options::valueCount() const 
{
    if (settings().autoLight)
        return ValueCount;
    else
        return EditingManualBrightness + 1; // No more editable value after this one
}

bool Options::isValueAvailable(int valueIndex) const 
{
    // The chime volume can only be set if the sound effects mode is selected.
    return 
        valueIndex != EditingChimeVolume || 
        settings().hourlyChimeMode == Settings::HourlyChimeMode::Sound ||
        settings().hourlyChimeMode == Settings::HourlyChimeMode::SoundOnDayLight;
}

void Options::modifyValue(int valueIndex, Direction direction)
{
    switch(valueIndex)
    {
        case EditingAutoScroll:
            toggleBool(modifySettings().autoScroll);
            break;

        case EditingTimeFormat:
            toggleBool(modifySettings().format24h);
            break;

        case EditingDateFormat:
            adjustEnum(modifySettings().dateFormat, direction);
            break;

        case EditingHourlyChimeMode:
            do {
                adjustEnum(modifySettings().hourlyChimeMode, direction);
                // Do not allow selecting "sound effect" if no player module is detected.
            } while ((settings().hourlyChimeMode == Settings::HourlyChimeMode::Sound ||
                     settings().hourlyChimeMode == Settings::HourlyChimeMode::SoundOnDayLight) &&
                    !player().detected());
            break;

        case EditingChimeVolume:
            adjustField(
                direction, PlayerVolume, modifySettings().chimeSoundVolume, Player::MAX_VOLUME);

            player().setVolume(settings().chimeSoundVolume);

            // Start playing sound if not playing yet (deferred in the lambda expression as the answer 
            // from the module is needed)
            player().queryStatus([this](Player::PlaybackStatus status)
                {
                    if (status != Player::Playing)
                        player().playTrackInFolder(1, 1); 
                });

            // TODO: check if it makes sense to put the player in sleep mode after editing volume
            break;

        case EditingAutoLight:
            toggleBool(modifySettings().autoLight);
            break;

        case EditingManualBrightness:
            if (settings().autoLight)
                adjustField(direction, AutoBrightnessPoint, modifySettings().brightnessDark);
            else
                adjustField(direction, ManualBrightness, modifySettings().manualBrightness);
            break;

        case EditingBrightnessDim:
            adjustField(direction, AutoBrightnessPoint, modifySettings().brightnessDim);
            break;

        case EditingBrightnessBright:
            adjustField(direction, AutoBrightnessPoint, modifySettings().brightnessBright);
            break;
    }
}
