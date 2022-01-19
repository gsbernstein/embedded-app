#include "DisplayEink154in.hpp"

#include <Fonts/FreeMonoBold9pt7b.h>
#include <utility>

const char HELLO_WORLD[] = "Hello Fy World!";

// Set up the U8g2lib display
DisplayEink154in::DisplayEink154in() {
    font_ = u8g2_font_tom_thumb_4x6_tf;
#ifdef HUZZAH32_SINDARIN
    display.init();
    display.setRotation(2);
#else
    display_.init();
    display_.setRotation(0);
#endif
    u8g2Fonts_.begin(display_);
    u8g2Fonts_.setFont(font_);
    log_d("Set up display (%ld)", (long)&display_);

    setX(Eink154in_X_PIXELS);
    setY(Eink154in_Y_PIXELS);
    setWidth(Eink154in_WIDTH_IN_MM);
    setHeight(Eink154in_HEIGHT_IN_MM);
}

// Fill in with the right splash screen for Eink
void DisplayEink154in::displaySplashScreen() {}

void DisplayEink154in::setPartialRefresh() { partialRefresh_ = 1; }

void DisplayEink154in::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplayEink154in::getPartialRefresh() -> int { return partialRefresh_; }

void DisplayEink154in::setFont(const uint8_t *newFont) { font_ = newFont; }

auto DisplayEink154in::getTextHeight() -> int {
    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2Fonts_.getFontAscent();
    descent = u8g2Fonts_.getFontDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    lineHeight = ascent - descent + 1;

    return lineHeight;
}

auto DisplayEink154in::getTextWidth(const char *buffer) -> int {
    return u8g2Fonts_.getUTF8Width(buffer);
}

void DisplayEink154in::drawTextInBox(String newText, int startX, int startY, int endX, int endY) {

    int lineHeight = 0;

    setupDisplayForText();

    log_d("Printing some stuff to screen! (%ld)", (long)&display_);

    lineHeight = getTextHeight();

    if (getPartialRefresh()) {
        display_.setPartialWindow(startX - 2, startY - 2, (endX - startX) + 4, (endY - startY) + 4);
    }

    display_.firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
        drawTextCmdsOnly(newText, startX + 3, startY + lineHeight + 1, lineHeight);
    } while (display_.nextPage());

    /*
     display.setRotation(2);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());*/
}

void DisplayEink154in::drawBoxCmdsOnly(int startX, int startY, int endX, int endY) {

    if (!getPartialRefresh()) {
        display_.fillScreen(bg_);
    }
    display_.drawRect(startX, startY, endX - startX, endY - startY, fg_);
}

void DisplayEink154in::drawBox(int startX, int startY, int endX, int endY) {
    // clear out one pixel around all of it
    if (getPartialRefresh()) {
        display_.setPartialWindow(startX - 2, startY - 2, (endX - startX) + 4, (endY - startY) + 4);
    }
    DEBUG_LN("Got here: drawing a box");

    display_.firstPage();
    do {
        drawBoxCmdsOnly(startX, startY, endX, endY);
    } while (display_.nextPage());
}

void DisplayEink154in::drawText() { drawText(stringToDraw_, xToDraw_, yToDraw_); }

void DisplayEink154in::drawText(String newText) { drawText(newText, xToDraw_, yToDraw_); }

// This method takes in a string, and assumes that it will have
// \n characaters in it to determine new lines. By default, the u8g2
// driver doesn't do new lines, so we have to do the parsing of that ourselves
// in order to determine the right placement of each new line.

void DisplayEink154in::drawText(String newText, int x, int y) {

    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;

    setupDisplayForText();

    DEBUG_LN("Printing to screen!");

    // hack for now
    // figure out what the right window is
    // SUPER HACK -- the y value is just guessing about ascent
    // Need to figure out ascent

    // text to squeeze as much in there as we can
    ascent = u8g2Fonts_.getFontAscent();
    descent = u8g2Fonts_.getFontDescent();

    // Note that descent is returned as a negative number. Therefore to get
    // the correct height, you have to subtract off the descent. We add
    // 1 to add the additional pixel between lines.
    lineHeight = ascent - descent + 1;

    if (getPartialRefresh()) {
        display_.setPartialWindow(x, y - 10, Eink154in_X_PIXELS, (lineHeight + 1) * numLines_ + 10);
    }

    display_.firstPage();
    do {
        drawTextCmdsOnly(newText, x, y, lineHeight);
    } while (display_.nextPage());
}

void DisplayEink154in::setupDisplayForText() {

    u8g2Fonts_.setFontMode(1);
    u8g2Fonts_.setFontDirection(0);
    u8g2Fonts_.setForegroundColor(fg_);
    u8g2Fonts_.setBackgroundColor(bg_);
    u8g2Fonts_.setFont(font_);
}

void DisplayEink154in::drawTextCmdsOnly(const String &newText, int x, int y, int lineHeight) {

    stringToDraw_ = newText;

    // We want to draw the characters down from the x/y coordinates given
    // so the renderer doesn't need to concern itself with how tall the
    // font is, etc.
    // u8g2Fonts->setFontPosTop();

    splitStringToLines();

    // We want to have only 1 pixel length difference between the rows of
    // text to squeeze as much in there as we can
    // ascent = u8g2Fonts->getAscent();
    // descent = u8g2Fonts->getDescent();

    if (!getPartialRefresh()) {
        display_.fillScreen(bg_);
    }

    for (int i = 0; i < numLines_; i++) {
        // Don't know if it starts the line from the baseline or from the top -- TO LEARN: HACK
        u8g2Fonts_.setCursor(x, y + i * (lineHeight));
        u8g2Fonts_.println(lines_[i] + "\n");
        // DEBUG_LN(lines[i]);
        // DEBUG_VAR(x);
        // DEBUG_LN(" is the x direction");
        // DEBUG_VAR(y+i*(line_height+1));
        // DEBUG_LN(" is the y direction");
    }
}
