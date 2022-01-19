#include "DisplaySharpMem.hpp"

#include <U8g2lib.h>
#include <utility>

#include "friend.h"
#include "sindarin-debug.h"

#define SHARP_SCK 5
#define SHARP_MOSI 18
#define SHARP_SS 21

#define BLACK 0
#define WHITE 1

// Set up the U8g2lib display
DisplaySharpMem::DisplaySharpMem() {
    font_ = u8g2_font_tom_thumb_4x6_tf;
    bigFont_ = u8g2_font_6x13_tf;
    u8g2_ = new U8G2_LS013B7DH05_144X168_F_4W_HW_SPI(
        U8G2_R0, /* cs=*/SHARP_SS, /* dc=*/U8X8_PIN_NONE,
        /* reset=*/U8X8_PIN_NONE); // there is no DC line for this display

    // u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, I2C_SCL,
    // I2C_SDA);
    if (u8g2_) {
        u8g2_->begin();
        u8g2_->clearBuffer(); // clear the internal memory
        u8g2_->setFont(font_);
    }
    setX(SHARPMEM_X_PIXELS);
    setY(SHARPMEM_Y_PIXELS);
    setWidth(SHARPMEM_WIDTH_IN_MM);
    setHeight(SHARPMEM_HEIGHT_IN_MM);
}

DisplaySharpMem::~DisplaySharpMem() {
    if (u8g2_) delete u8g2_;
}

// Show off the "Friend" graphic.
void DisplaySharpMem::displaySplashScreen() {
    DEBUG_LN("DRAWING SPLASH SCREEN!");
    u8g2_->firstPage();
    do {
        u8g2_->drawXBM(0, 6, FRIEND_WIDTH, FRIEND_HEIGHT, friendBits);
    } while (u8g2_->nextPage());
}

void DisplaySharpMem::setPartialRefresh() { partialRefresh_ = 1; }

void DisplaySharpMem::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplaySharpMem::getPartialRefresh() -> int { return partialRefresh_; }

void DisplaySharpMem::setFont(const uint8_t *newFont) { font_ = newFont; }

void DisplaySharpMem::drawBox(int startX, int startY, int endX, int endY) {
    u8g2_->firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
    } while (u8g2_->nextPage());
}

void DisplaySharpMem::drawTextInBox(String newText, int startX, int startY, int endX, int endY) {
    u8g2_->setFontPosTop();
    u8g2_->setFont(font_);
    u8g2_->firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
        drawTextCmdsOnly(newText, startX + 3, startY + 2);
    } while (u8g2_->nextPage());
}

void DisplaySharpMem::drawBigWordInBox(String newText, int startX, int startY, int endX, int endY) {
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

void DisplaySharpMem::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {
    u8g2_->drawLine(startX, startY, endX, startY);
    u8g2_->drawLine(startX, startY, startX, endY);
    u8g2_->drawLine(startX, endY, endX, endY);
    u8g2_->drawLine(endX, startY, endX, endY);
    // Uncomment if you want to draw the lines from corner to corner
    // u8g2->drawLine(start_x, start_y, end_x, end_y);
    // u8g2->drawLine(end_x, start_y, start_x, end_y);
}

void DisplaySharpMem::drawText() { drawText(stringToDraw_, xToDraw_, yToDraw_); }

void DisplaySharpMem::drawText(String newText) { drawText(newText, xToDraw_, yToDraw_); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplaySharpMem::drawText(String newText, int x, int y) {
    u8g2_->setFontPosTop();
    u8g2_->setFont(font_);
    u8g2_->firstPage();
    do {
        drawTextCmdsOnly(newText, x, y);
    } while (u8g2_->nextPage());
}

auto DisplaySharpMem::getTextHeight() -> int {
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

auto DisplaySharpMem::getTextWidth(const char *buffer) -> int { return u8g2_->getStrWidth(buffer); }

void DisplaySharpMem::drawTextCmdsOnly(const String &newText, int x, int y) {

    int lineHeight = 0;

    stringToDraw_ = newText;

    splitStringToLines();

    lineHeight = getTextHeight();

    for (int i = 0; i < numLines_; i++) {
        u8g2_->setCursor(x, y + i * lineHeight);
        u8g2_->print(lines_[i]);
    }
}