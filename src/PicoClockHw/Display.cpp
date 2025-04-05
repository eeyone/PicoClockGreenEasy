#include "Display.h"
#include "Utils/Trace.h"
#include "gpio.h"
#include "Utils/Trampoline.h"

#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <hardware/pwm.h>
#include <iostream>

#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>

#include "Display.pio.h"

namespace 
{
    const int AMBIENT_LIGHT_HYSTERESIS = 1;
    const int PWM_WRAP = 1000;
    PIO g_pio = pio0;
}

Display *Display::m_instance = nullptr;

Display::Display(const uint32_t *frameBuffer, std::function<void()> frameCallback) 
    : m_frameBuffer(frameBuffer), m_frameCallback(frameCallback) 
{
    TRACE << "Display constructor";
    // Configure GPIOs used for sending data to the LED matrix controller
    gpio_init(A0);
    gpio_init(A1);
    gpio_init(A2);
    gpio_init(SDI);
    gpio_init(LE);
    gpio_init(CLK);
    gpio_set_dir(A0, GPIO_OUT);
    gpio_set_dir(A1, GPIO_OUT);
    gpio_set_dir(A2, GPIO_OUT);
    gpio_set_dir(SDI, GPIO_OUT);
    gpio_set_dir(LE, GPIO_OUT);
    gpio_set_dir(CLK, GPIO_OUT);

    // Configure reading of the ambient light sensor
    adc_init();
    adc_gpio_init(AIN);
    adc_select_input(0); // Select ADC input 0, which is ADC_LIGHT
    
    // Configure PWM to adjust display brightness
    gpio_set_function(OE, GPIO_FUNC_PWM);
    int slice = pwm_gpio_to_slice_num(OE);

    // Some leds do not light uniformely if the brightness is low and the PWM frequency is high.
    // Dividing the frequency prevents that.
    pwm_set_clkdiv(slice, 2);

    pwm_set_wrap(slice, PWM_WRAP);

    // Initially set to minimum brightness to avoid a flash on startup (represented by the maximum as OE is 
    // active low)
    pwm_set_gpio_level(OE, PWM_WRAP);

    pwm_set_enabled(slice, true);

    m_instance = this;

    initSendPixelsPioStateMachine();
    initSelectRowsPioStateMachine();

    // Start both state machines
    pio_set_sm_mask_enabled(g_pio, (1<<m_sendPixelsSm) | (1<<m_selectRowsSm), true);

    initDma();

    TRACE << "Display constructor done";
}

Display::~Display()
{
    m_instance = nullptr;

    // Stop DMA and unclaim channels
    dma_channel_abort(m_dataChannel);
    dma_channel_abort(m_ctrlChannel);
    dma_timer_unclaim(0);
    dma_channel_unclaim(m_dataChannel);
    dma_channel_unclaim(m_ctrlChannel);

    // Stop state machines and unclaim them
    pio_set_sm_mask_enabled(g_pio, (1<<m_sendPixelsSm) | (1<<m_selectRowsSm), false);
    pio_sm_unclaim(g_pio, m_sendPixelsSm);
    pio_sm_unclaim(g_pio, m_selectRowsSm);
}

void Display::initSendPixelsPioStateMachine()
{
    // Claim state machine and configure GPIOs
    m_sendPixelsSm = pio_claim_unused_sm(g_pio, true);
    pio_gpio_init(g_pio, SDI);
    pio_gpio_init(g_pio, CLK);
    pio_gpio_init(g_pio, LE);
    pio_sm_set_consecutive_pindirs(g_pio, m_sendPixelsSm, SDI, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, m_sendPixelsSm, CLK, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, m_sendPixelsSm, LE, 1, true);
    
    // Configure state machine
    uint offset = pio_add_program(g_pio, &send_pixels_program);
    pio_sm_config c = send_pixels_program_get_default_config(offset);
    sm_config_set_out_pins(&c, SDI, 1);
    sm_config_set_out_shift(&c, false /* shift OSR to left */, false /* autopull disabled */, 0);
    sm_config_set_sideset_pins(&c, CLK);
    sm_config_set_set_pins(&c, LE, 1);
    pio_sm_init(g_pio, m_sendPixelsSm, offset, &c);
}

void Display::initSelectRowsPioStateMachine()
{
    // Claim state machine and configure GPIOs
    m_selectRowsSm = pio_claim_unused_sm(g_pio, true);
    pio_gpio_init(g_pio, A0);
    pio_gpio_init(g_pio, A1);
    pio_gpio_init(g_pio, A2);
    pio_sm_set_consecutive_pindirs(g_pio, m_selectRowsSm, A0, 1, true); 
    pio_sm_set_consecutive_pindirs(g_pio, m_selectRowsSm, A1, 1, true); 
    pio_sm_set_consecutive_pindirs(g_pio, m_selectRowsSm, A2, 1, true);
    
    // Prepare state machine configuration
    m_selectRowsProgramOffset = pio_add_program(g_pio, &select_rows_program);
    pio_sm_config c = select_rows_program_get_default_config(m_selectRowsProgramOffset);

    // Assign "set pins" to A0 and A1, with the ignored pin 17 in between
    sm_config_set_set_pins(&c, A0, 3);

    // Side set will control A2
    sm_config_set_sideset_pins(&c, A2);

    // Init SM
    pio_sm_init(g_pio, m_selectRowsSm, m_selectRowsProgramOffset, &c);
}

void Display::initDma()
{
    // The data channel will transfer the frame buffer to the led matrix controller, whereas the
    // control channel will restart it after every frame.
    m_dataChannel = dma_claim_unused_channel(true);
    m_ctrlChannel = dma_claim_unused_channel(true);

    // Prepare the configuration of the data channel
    dma_channel_config cfg = dma_channel_get_default_config(m_dataChannel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32); // Each element is a row
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    // Configure a DMA timer that will pace the transfer so that the time to display a frame 
    // corresponds to the frame rate.
    dma_timer_claim(0);
    dma_timer_set_fraction(0, 1, clock_get_hz(clk_sys) / FRAME_RATE / HEIGHT); 
    channel_config_set_dreq(&cfg, dma_get_timer_dreq(0));
    
    // Chain the data channel to the control channel, which will continuously restart the data channel.
    channel_config_set_chain_to(&cfg, m_ctrlChannel);

    dma_channel_configure(
        m_dataChannel, 
        &cfg, 
        &pio0_hw->txf[0],   // Write to the TX FIFO of the "send pixels" SM
        m_frameBuffer, 
        HEIGHT, 
        false               // do no start immediately
    );

    // Install the IRQ handler that will be called whenever one frame transfer is completed.
    dma_channel_set_irq0_enabled(m_dataChannel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, onDmaTransferredFrame);
    irq_set_enabled(DMA_IRQ_0, true);

    // Configure the control channel to reset the data channel read address and retrigger it.
    cfg = dma_channel_get_default_config(m_ctrlChannel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, false);
    dma_channel_configure(
        m_ctrlChannel, 
        &cfg, 
        &dma_hw->ch[m_dataChannel].al3_read_addr_trig,  // Write data channel read address and trigger
        &m_frameBuffer,                                 // Read address of frame buffer
        1,                                              // This address is the only element
        false                                           // Do no start immediately
    );

    // Now that everything is ready, start the continuous transfer. The led matrix controller is fully
    // driven by DMA and PIO, so that the CPU does not have to handle anything for it.
    dma_channel_start(m_dataChannel);
}

void Display::setFlashlightMode(bool flashlightMode)
{
    if (flashlightMode)
    {
        if (m_flashlightMode)
            return; // Already in flashlight mode

        // Disable the channel chaining by setting it to itself, so that it won't restart when
        // aborting the data channel.
        dma_channel_config cfg = dma_get_channel_config(m_dataChannel);
        channel_config_set_chain_to(&cfg, m_dataChannel);
        dma_channel_set_config(m_dataChannel, &cfg, false);

        TRACE << "Stop display scanning by stoppping the DMA";
        dma_channel_abort(m_dataChannel);

        // Since the DMA timer is no longer called, use an alternative timer instead.
        m_flashlightModeTimer.startRepeatable(1000 / FRAME_RATE, []()
            { 
                if (m_instance->m_frameCallback)
                    m_instance->m_frameCallback();
                return Timer::RescheduleFromPreviousCall; 
            });

        // Enable only white leds on the left side
        pio_sm_put(g_pio, m_sendPixelsSm, (1 << 29) | (1 << 26));

        // Stop the state machine that selects the rows and stay on row 0, which is the one white 
        // leds are connected to
        pio_sm_set_enabled(g_pio, m_selectRowsSm, false);
        pio_sm_set_pins(g_pio, m_selectRowsSm, 0);
    } else
    {
        if (!m_flashlightMode)
            return; // Already in normal mode

        // Set brightness to zero to avoid a glitch when switching back to the display.
        setBrightness(0);

        // Restart the state machine that selects the rows and jump to the second instruction so
        // that rows are aligned correctly (not sure why it needs to be the second instruction)
        pio_sm_set_enabled(g_pio, m_selectRowsSm, true);
        pio_sm_exec(g_pio, m_selectRowsSm, pio_encode_jmp(m_selectRowsProgramOffset + 1));

        // Restore the channel chaining.
        dma_channel_config cfg = dma_get_channel_config(m_dataChannel);
        channel_config_set_chain_to(&cfg, m_ctrlChannel);
        dma_channel_set_config(m_dataChannel, &cfg, false);

        // Stop the alternative timer and restart the DMA transfer.
        m_flashlightModeTimer.stop();
        dma_channel_start(m_dataChannel);
    }

    m_flashlightMode = flashlightMode;
}

void Display::onDmaTransferredFrame()
{
    if (m_instance->m_frameCallback)
        m_instance->m_frameCallback();

    // Clear the interrupt request.
    dma_hw->ints0 = 1u << m_instance->m_dataChannel;
}

float Display::ambientLight() const
{
    uint16_t adc = 4095 - adc_read();

    // Stabilize the output of the DAC using a filter
    m_ambientLightFilter.put(adc * 100.0f / 4095);
    return m_ambientLightFilter.get();
}

void Display::setBrightness(float percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

//    TRACE << "Set brightness to" << percent << "%";

    // OE is active low, so reverse the percentage.
    // Also set a level of at least 1 so that the display does not completely turn off.
    pwm_set_gpio_level(OE, PWM_WRAP - std::max(1.0f, percent * PWM_WRAP / 100));
}