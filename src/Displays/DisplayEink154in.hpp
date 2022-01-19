#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_154_D67 // GDEH0154D67 200x200

// SS is usually used for CS. define here for easy change
#ifndef EPD_CS
#define EPD_CS SS
#endif

#if defined(GxEPD2_DISPLAY_CLASS) && defined(GxEPD2_DRIVER_CLASS)

// somehow there should be an easier way to do this
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)

#endif

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
#define MAX_HEIGHT(EPD)                                                                            \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8)                                     \
         ? EPD::HEIGHT                                                                             \
         : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#define Eink154in_X_PIXELS 200
#define Eink154in_Y_PIXELS 200

#define Eink154in_WIDTH_IN_MM 28.5
#define Eink154in_HEIGHT_IN_MM 28.5

#if !defined(DISABLE_SPIRAM)
#define Eink154in_USE_SPRAM 1
#endif

class DisplayEink154in : public DisplaySystem {
public:
    DisplayEink154in();
    ~DisplayEink154in() = default;

#ifdef Eink154in_USE_SPRAM
    auto operator new(size_t size) -> void * {
        void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        return p;
    }

    void operator delete(void *p) { heap_caps_free(p); }
#endif

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
    void putUpHelloWorld();

    void drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) override {
        assert(0);
    }

    void setFont(const uint8_t *newFont);

protected:
    void drawTextCmdsOnly(const String &newText, int x, int y, int lineHeight);
    void drawBoxCmdsOnly(int xStart, int yStart, int xEnd, int yEnd);
    void setupDisplayForText();

    const uint8_t *font_;
    // U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2;
    U8G2_FOR_ADAFRUIT_GFX u8g2Fonts_;
#ifdef HUZZAH32_SINDARIN
    GxEPD2_BW<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display =
        GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(
            GxEPD2_DRIVER_CLASS(/*CS=5*/ 14, /*DC=*/15, /*RST=*/27, /*BUSY=*/13));
#else
    GxEPD2_BW<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display_ =
        GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(
            GxEPD2_DRIVER_CLASS(/*CS=5*/ 5, /*DC=*/4, /*RST=*/32, /*BUSY=*/33));
#endif

    uint16_t bg_ = GxEPD_WHITE;
    uint16_t fg_ = GxEPD_BLACK;
};

#endif