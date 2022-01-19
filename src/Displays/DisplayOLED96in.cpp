#include "DisplayOLED96in.hpp"

#include <U8g2lib.h>
#include <utility>

#include "friend.h"
#include "sindarin-debug.h"

// Set up the U8g2lib display
DisplayOLED96in::DisplayOLED96in() {
    font_ = u8g2_font_tom_thumb_4x6_tf;
    bigFont_ = u8g2_font_6x13_tf;
    u8g2_ = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/U8X8_PIN_NONE, I2C_SCL,
                                                    I2C_SDA);
    if (u8g2_) {
        u8g2_->begin();
        u8g2_->clearBuffer(); // clear the internal memory
        u8g2_->setFont(font_);
    }
    setX(OLED96in_X_PIXELS);
    setY(OLED96in_Y_PIXELS);
    setWidth(OLED96in_WIDTH_IN_MM);
    setHeight(OLED96in_HEIGHT_IN_MM);
}

DisplayOLED96in::~DisplayOLED96in() {
    if (u8g2_) delete u8g2_;
}

// Show off the "Friend" graphic.
void DisplayOLED96in::displaySplashScreen() {
    u8g2_->firstPage();
    do {
        u8g2_->drawXBM(0, 6, FRIEND_WIDTH, FRIEND_HEIGHT, friendBits);
    } while (u8g2_->nextPage());
}

void DisplayOLED96in::setPartialRefresh() { partialRefresh_ = 1; }

void DisplayOLED96in::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplayOLED96in::getPartialRefresh() -> int { return partialRefresh_; }

void DisplayOLED96in::setFont(const uint8_t *newFont) { font_ = newFont; }

void DisplayOLED96in::drawBox(int startX, int startY, int endX, int endY) {
    u8g2_->firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
    } while (u8g2_->nextPage());
}

void DisplayOLED96in::drawTextInBox(String newText, int startX, int startY, int endX, int endY) {
    u8g2_->setFontPosTop();
    u8g2_->setFont(font_);
    u8g2_->firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
        drawTextCmdsOnly(newText, startX + 3, startY + 2);
    } while (u8g2_->nextPage());
}

void DisplayOLED96in::drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) {
    int width = 0;
    u8g2_->setFontPosCenter();
    u8g2_->setFont(bigFont_);
    u8g2_->firstPage();
    width = getTextWidth(newText.c_str());

    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
        drawTextCmdsOnly(newText, startX + (endX - startX) / 2 - width / 2, (endY - startY) / 2);
    } while (u8g2_->nextPage());
}

void DisplayOLED96in::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {
    u8g2_->drawLine(startX, startY, endX, startY);
    u8g2_->drawLine(startX, startY, startX, endY);
    u8g2_->drawLine(startX, endY, endX, endY);
    u8g2_->drawLine(endX, startY, endX, endY);
    // Uncomment if you want to draw the lines from corner to corner
    // u8g2->drawLine(start_x, start_y, end_x, end_y);
    // u8g2->drawLine(end_x, start_y, start_x, end_y);
}

void DisplayOLED96in::drawText() { drawText(stringToDraw_, xToDraw_, yToDraw_); }

void DisplayOLED96in::drawText(String newText) { drawText(newText, xToDraw_, yToDraw_); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayOLED96in::drawText(String newText, int x, int y) {
    u8g2_->setFontPosTop();
    u8g2_->setFont(font_);
    u8g2_->firstPage();
    do {
        drawTextCmdsOnly(newText, x, y);
    } while (u8g2_->nextPage());
}

auto DisplayOLED96in::getTextHeight() -> int {
    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2_->getAscent();
    descent = u8g2_->getDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    lineHeight = ascent - descent + 1;

    return lineHeight;
}

auto DisplayOLED96in::getTextWidth(const char *buffer) -> int { return u8g2_->getStrWidth(buffer); }

void DisplayOLED96in::drawTextCmdsOnly(const String &newText, int x, int y) {

    int lineHeight = 0;

    stringToDraw_ = newText;

    splitStringToLines();

    lineHeight = getTextHeight();

    for (int i = 0; i < numLines_; i++) {
        u8g2_->setCursor(x, y + i * lineHeight);
        u8g2_->print(lines_[i]);
    }
}