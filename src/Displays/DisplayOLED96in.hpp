#ifndef DISPLAY_OLED_96IN_H
#define DISPLAY_OLED_96IN_H

#include <U8g2lib.h>

#include "DisplaySystem.hpp"

// Change these based on where the board is connected to the ESP32
// May want to make this more parameterized down the road?
#define I2C_SDA 23
#define I2C_SCL 22

#define OLED96in_X_PIXELS 128
#define OLED96in_Y_PIXELS 64

#define OLED96in_WIDTH_IN_MM 22
#define OLED96in_HEIGHT_IN_MM 11

class DisplayOLED96in : public DisplaySystem {
public:
    DisplayOLED96in();
    ~DisplayOLED96in();

    void drawBox(int xStart, int yStart, int xEnd, int yEnd) override;

    void drawText() override;
    void drawText(String newText) override;
    void drawText(String newText, int x, int y) override;
    void drawTextInBox(String newText, int startX, int startY, int endX, int endY) override;
    void drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) override;

    auto getTextHeight() -> int override;
    auto getTextWidth(const char *buffer) -> int override;

    void setFont(const uint8_t *newFont);

    void displaySplashScreen() override;

    void setPartialRefresh() override;
    void unsetPartialRefresh() override;
    auto getPartialRefresh() -> int override;

protected:
    void drawTextCmdsOnly(const String &newText, int x, int y);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);

    const uint8_t *font_;
    const uint8_t *bigFont_;

    U8G2_SSD1306_128X64_NONAME_F_HW_I2C *u8g2_;
};

#endif