#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "config.h"

#include "Bitmaps/Menu_Blank.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Fonts/LucidaGrande10pt7b.h"
#include "Fonts/LucidaGrande14pt7b.h"
#include "Fonts/LucidaGrande5pt7b.h"
#include "Fonts/LucidaGrande6pt7b.h"

#define MAX_WIDTH 152

// BC: I'm reducing the number of fonts to 2 here as I feel 14 is too big and 5 is too
// small when I look at it in glass
const GFXfont *shrinkingFonts[] = {/*&LucidaGrande14pt7b,*/
                                   &LUCIDA_GRANDE10PT7B, &LUCIDA_GRANDE6PT7B
                                   /*,&LucidaGrande5pt7b */};

void PagedMenuScreen::render() {
    if (!needsRerender) {
        return;
    }
    log_i("PagedMenuScreen render (selected = %s)", getCurrItem().title.c_str());
#ifdef EINKET013TT1
    auto *eink = (DisplayEinkET013TT1 *)renderer_.getDisplay();
    if (eink != nullptr) {
        auto autoUpdate = eink->getAutoUpdate();
        eink->setAutoUpdate(false);
        // Blank out screen
        eink->fillScreen(eink->bg);
        // Draw menu surroundings
        eink->drawXBitmap(this->renderer_.localXOffset, this->renderer_.localYOffset,
                          MENU_BLANK_BITS, 256, 256, eink->fg);
        String itemTitle = getCurrItem().title;
        // Cache / replace old font; then try descending size fonts until we find one that fits our
        // string in the desired space.
        auto prevFont = eink->getFont();
        for (int i = 0; i < sizeof(shrinkingFonts); ++i) {
            eink->setFont(shrinkingFonts[i]);
            if (eink->getTextWidth(itemTitle.c_str()) < MAX_WIDTH) {
                break;
            }
            log_d("Font size %d was too big for menu item title; stepping down!", i);
        }
        // If we still don't fit, shrink the string until we do.
        while (eink->getTextWidth(itemTitle.c_str()) > MAX_WIDTH) {
            log_d("Item title %s still too long at smallest font size; truncating...",
                  itemTitle.c_str());
            itemTitle = itemTitle.substring(0, itemTitle.length() - 4) + "...";
        }
        // Now draw!
        uint16_t width = 0, height = 0;
        String batteryStatus = this->power_.getBatteryLifeString();
        eink->getTextBounds(itemTitle.c_str(), &width, &height);
        eink->drawText(itemTitle.c_str(),
                       this->renderer_.localXOffset + (EinkET013TT1_X_PIXELS) / 2 - (width / 2),
                       this->renderer_.localYOffset + (EinkET013TT1_Y_PIXELS) / 2 + (height / 2));
        eink->setFont(&LUCIDA_GRANDE5PT7B);
        eink->getTextBounds(batteryStatus.c_str(), &width, &height);
        // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
        // This is just so we always have it in our prototype, not that it'll be
        // the right place (or icon) long term
        // Not taking into account the rednerer x and y offsets here on purpose
        eink->drawText(batteryStatus.c_str(), (EinkET013TT1_X_PIXELS) / 2 - (width / 2),
                       EinkET013TT1_Y_PIXELS - 20);
        eink->updateDisplay();
        eink->setFont(prevFont);
        eink->setAutoUpdate(autoUpdate);
    }
#endif
    needsRerender = false;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::nextSelection() -> int {
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    return numMenuItems_ > 1 ? 1 : 0;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::prevSelection() -> int {
    currSelection_ = (currSelection_ - 1) % numMenuItems_;
    return numMenuItems_ > 1 ? 1 : 0;
}