#ifndef DISPLAY_SHARPMEM_H
#define DISPLAY_SHARPMEM_H

#include <U8g2lib.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define SHARPMEM_X_PIXELS 144
#define SHARPMEM_Y_PIXELS 168

#define SHARPMEM_WIDTH_IN_MM 21.5
#define SHARPMEM_HEIGHT_IN_MM 24.5

class DisplaySharpMem : public DisplaySystem {
public:
    DisplaySharpMem();
    ~DisplaySharpMem();

    void drawBox(int xStart, int yStart, int xEnd, int yEnd) override;

    void drawText() override;
    void drawText(String newText) override;
    void drawText(String newText, int x, int y) override;

    void drawTextInBox(String newText, int startX, int startY, int endX, int endY) override;

    auto getTextHeight() -> int override;
    auto getTextWidth(const char *buffer) -> int override;

    void setPartialRefresh() override;
    void unsetPartialRefresh() override;
    auto getPartialRefresh() -> int override;

    void displaySplashScreen() override;

    void drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) override;

    void setFont(const uint8_t *newFont);

protected:
    void drawTextCmdsOnly(const String &newText, int x, int y);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);
    void setupDisplayForText();

    const uint8_t *font_;
    const uint8_t *bigFont_;
    // U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2;
    U8G2_LS013B7DH05_144X168_F_4W_HW_SPI *u8g2_;
};

#endif