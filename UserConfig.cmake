# Used pico board, pico or pico_w
set(PICO_BOARD pico_w)

# Enable/disable debug traces to stdio. Traces can be filtered by file in the 
# isEnabledForFile method of src/Utils/Trace.cpp
set(TRACE_TO_STDIO "0")

add_compile_definitions(
    # Wi-fi SSID and password
    WIFI_SSID=\"\"
    WIFI_PASSWORD=\"\"
    
    # UTC offset as real number of hours. For example, set 5.5 for UTC+05:30.
    UTC_OFFSET=0

    # Set the language of the user interface. Possible values: English, French
    LANGUAGE=English
    
    # Set location for automatic daylight saving time observation. 
    # Possible values: Europe, USA or Unknown (no automatic change)
    DST_LOCATION=Unknown

    # This can be defined to simulate the three buttons using the standard input. Enter triggers SET
    # and the arrow keys trigger UP and DOWN.
    # SIMULATE_BUTTONS_FROM_STDIO

    # This can be defined to use GPS date/time even if there is no GPS fix yet. This is less
    # accurate (a few seconds difference) but can help in case of difficulty synchronizing from GPS
    # indoors.
    # GPS_SYNC_WITHOUT_FIX

    # This can be set to compensate the temperature measurement of the RTC. The value is the 
    # correction applied to the temperature in degrees celsius. For example, set -2.0 for lowering 
    # 2.0 degrees celsius on the display.
    RTC_TEMP_CALIB=0.0
)
