#include "Platform.hpp"

#include <esp_adc_cal.h>

#include "config.h"

#include "sindarin-debug.h"

const int MAX_ANALOG_VAL = 4095.0;
// Max LiPo cell voltage is 4.2 but as soon as its supplying current it tends to drop closer to 4V
const float MAX_BATTERY_VOLTAGE = 4.0;
const float MIN_BATTERY_VOLTAGE = 3.0;

#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
#define DEFAULT_VREF 1100 // Default referance voltage in mv

void InitializePlatform() {
    pinMode(BAT_VOLTAGE_LEFT, ANALOG);
    pinMode(BAT_VOLTAGE_RIGHT, ANALOG);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    pinMode(DISPLAY_POWER, OUTPUT);
}

// Forked from TinyPico's GetBatteryVoltage() method:
// https://github.com/tinypico/tinypico-arduino/blob/master/TinyPICO-Helper/src/TinyPICO.cpp#L207
float GetBatteryVoltageLeft() {
    uint32_t raw, mv;
    esp_adc_cal_characteristics_t chars;

    // only check voltage every 1 second

    // grab latest voltage
    analogRead(BAT_VOLTAGE_LEFT);          // Just to get the ADC setup
    raw = adc1_get_raw(BATT_CHANNEL_LEFT); // Read of raw ADC value

    // Get ADC calibration values
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &chars);

    // Convert to calibrated mv then volts
    mv = esp_adc_cal_raw_to_voltage(raw, &chars) * (LOWER_DIVIDER + UPPER_DIVIDER) / LOWER_DIVIDER;
    return (float)mv / 1000.0;
}

float GetBatteryVoltageRight() {
    uint32_t raw, mv;
    esp_adc_cal_characteristics_t chars;

    // grab latest voltage
    analogRead(BAT_VOLTAGE_RIGHT);          // Just to get the ADC setup
    raw = adc1_get_raw(BATT_CHANNEL_RIGHT); // Read of raw ADC value

    // Get ADC calibration values
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &chars);

    // Convert to calibrated mv then volts
    mv = esp_adc_cal_raw_to_voltage(raw, &chars) * (LOWER_DIVIDER + UPPER_DIVIDER) / LOWER_DIVIDER;

    return (float)mv / 1000.0;
}

// std::clamp is not working even though it's supposed to be available since C++17
#define clamp(v, lo, hi) (((v) < (lo)) ? (lo) : ((v) > (hi)) ? (hi) : (v))

auto BatteryLifeString() -> String {
#if defined(GILPICO_SINDARIN)
    float voltageLevelLeft = GetBatteryVoltageLeft();
    float voltageLevelRight = GetBatteryVoltageRight();
    float batteryFractionLeft =
        (voltageLevelLeft - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE);
    float batteryFractionRight =
        (voltageLevelRight - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE);
    batteryFractionLeft = clamp(batteryFractionLeft, 0, 1.0);
    batteryFractionRight = clamp(batteryFractionRight, 0, 1.0);
    String battery_output = " Raw L:" + String(voltageLevelLeft, 2) +
                            " R:" + String(voltageLevelRight, 2) +
                            "  L:" + String(batteryFractionLeft * 100) +
                            "% R:" + String(batteryFractionRight * 100) + "%";
#else
    // A13 pin is not exposed on Huzzah32 board because it's tied to
    // measuring voltage level of battery. Note: you must
    // multiply the analogRead value by 2x to get the true battery
    // level. See:
    // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/esp32-faq
    int rawValue = analogRead(A13);

    // Reference voltage on ESP32 is 1.1V
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration
    // See also: https://bit.ly/2zFzfMT
    float voltageLevel = (rawValue / MAX_ANALOG_VAL) * 2 * 1.1 * 3.3; // calculate voltage level
    float batteryFraction = voltageLevel / MAX_BATTERY_VOLTAGE;
    String battery_output = "  Percent: " + String(batteryFraction * 100) + "%";
#endif

    DEBUG_LN(battery_output.c_str());
    SHOW_HEAP();
    return battery_output;
}
