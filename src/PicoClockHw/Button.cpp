#include "Button.h"
#include "Utils/Trace.h"
#include "Utils/Trampoline.h"

#include <hardware/gpio.h>

namespace {
    const int DEBOUNCE_DELAY_MS = 10;
}

std::map<unsigned int, Button *> Button::m_buttonByGpio;

Button::Button(unsigned int gpio) : m_gpio(gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
    gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, dispatcher);

    m_buttonByGpio.insert(std::make_pair(gpio, this));
}

Button::~Button()
{
    m_buttonByGpio.erase(m_gpio);
}

void Button::setPressedCallback(std::function<void()> f)
{
    m_pressedCallback = f;
}

void Button::setRepeatCallback(std::function<void()> f, int delayMs)
{
    m_repeatCallback = f;
    m_repeatDelay = delayMs;
}

void Button::dispatcher(unsigned int gpio, uint32_t events)
{
    auto it = m_buttonByGpio.find(gpio);

    if (it == m_buttonByGpio.end())
        return;

    Button &obj = *it->second;

    // To debounce, delay the actual processing using an alarm.
    obj.m_debounceAlarm.startSingleShot(DEBOUNCE_DELAY_MS, std::bind(&Button::onDebounce, &obj));
    TRACE <<"End of dispatcher";
}

void Button::onDebounce()
{
    if (!gpio_get(m_gpio))
    {
        // Button pressed, call the user callback
        TRACE << "Call user callback\n";
        m_pressedCallback();
        
        // If a repeat callback is installed, start an alarm for it
        if (m_repeatCallback)
        {
            TRACE << "Start repeat alarm";
            m_repeatAlarm.startRepeatable(m_repeatDelay, std::bind(&Button::onRepeat, this));
        }
    } else
    { 
        // Button released. Cancel the repetition alarm.
        TRACE << "Cancel repeat alarm";
        m_repeatAlarm.stop();
    }
}

Timer::Rescheduling Button::onRepeat()
{
    if (gpio_get(m_gpio))
    { 
        // Button not pressed anymore. Do not reschedule the alarm
        TRACE << "Not rescheduled\n";
        return Timer::Stop;
    } else
    {
        m_repeatCallback();
        return Timer::RescheduleFromPreviousCall;
    }
}