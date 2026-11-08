#if CONFIG_DISPLAY_EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>
#include <cstring>
#include <freertos/task.h>
#if !SIMULATOR
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#endif

#include "Communications/USBSerial.hpp"
#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

#define VCOM_INVERTED_MODE_DELTA 2500

// Not currently used waveforms
// const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM = BC_EINK_31_35C_WAVEFORM_FULL;
// const EinkET013TT1Waveform ORIGINAL_FULL_REFRESH_WAVEFORM = EINK_ORIGINAL_WAVEFORM_FULL;
// const EinkET013TT1Waveform ORIGINAL_PARTIAL_REFRESH_WAVEFORM = EINK_31_35C_WAVEFORM_PARTIAL;

const EinkET013TT1Waveform FULL_BIG_ORIGINAL_WAVEFORM = EINK_10_13C_WAVEFORM_FULL;

// Currently used waveforms
const EinkET013TT1Waveform ERASE_WAVEFORM_REGULAR = BC_REGULAR_EINK_10_13C_WAVEFORM_ERASE;
const EinkET013TT1Waveform ERASE_WAVEFORM_INVERTED = BC_REGULAR_EINK_10_13C_WAVEFORM_ERASE;
// const EinkET013TT1Waveform PANEL_CLEAR_WAVEFORM = EINK_31_35C_WAVEFORM_CLEAR;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_REGULAR = BC_REGULAR_31_35C_WAVEFORM_FULL;
// const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_INVERTED = BC_INVERTED_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_REGULAR =
    BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_INVERTED =
    BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_ORIGINAL = EINK_10_13C_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform AU4_ORIGINAL = EINK_10_13C_WAVEFORM_AU4;

auto DisplayEinkET013TT1::setup() -> bool {
    DisplaySystem::setup();

    log_d("Initializing EPD driver");
    driver_.init();

    setFullWindow();

    setPartialRefresh();

    log_d("Checking Waveforms");

    uint8_t retVal = FULL_BIG_ORIGINAL_WAVEFORM.check();

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    retVal = PARTIAL_REFRESH_WAVEFORM_REGULAR.check();

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    return !driver_.errored();
}

// We not only need to set the bg and fg for the display,
// we also need to change the VCOM because black on white
// and white on black behave differently.
void DisplayEinkET013TT1::invertDisplay(bool newInvert) {
    bool prevInvert = invertedDisplay_;
    invertedDisplay_ = newInvert;
    if (invertedDisplay_) {
        bg = 0x0000;
        fg = 0xFFFF;
        // Only change the Vcom if the display inversion actually changes.
        if (prevInvert != invertedDisplay_) {
            log_d("Regular to dark mode, old %d, new %d", driver_.getVcom(),
                  driver_.getVcom() + VCOM_INVERTED_MODE_DELTA);
            driver_.setVcomNudge(VCOM_INVERTED_MODE_DELTA);
        }
    } else {
        bg = 0xFFFF;
        fg = 0x0000;
        // Only change the Vcom if the display inversion actually changes.
        if (prevInvert != invertedDisplay_) {
            log_d("Dark mode to regular, old %d, new %d", driver_.getVcom(),
                  driver_.getVcom() - VCOM_INVERTED_MODE_DELTA);
            driver_.setVcomNudge(0);
        }
    }
    fillScreen(bg);
}

void DisplayEinkET013TT1::driverUpdate(const EinkET013TT1Waveform &waveform, int32_t vcom,
                                       const EinkET013TT1Waveform &altWaveform, ScreenSide screen,
                                       EinkBox box) {
    const uint8_t *left = gfxCanvas_.getBuffer();
    const uint8_t *right = rightCanvas_.has_value() ? rightCanvas_->getBuffer() : left;
    if (box.rightX > box.leftX && box.bottomY > box.topY) {
        switch (gfxCanvas_.getRotation()) {
        case 1:
            box = {CONFIG_DISPLAY_WIDTH - box.bottomY, CONFIG_DISPLAY_WIDTH - box.topY, box.leftX,
                   box.rightX};
            break;
        case 2:
            box = {CONFIG_DISPLAY_WIDTH - box.rightX, CONFIG_DISPLAY_WIDTH - box.leftX,
                   CONFIG_DISPLAY_HEIGHT - box.bottomY, CONFIG_DISPLAY_HEIGHT - box.topY};
            break;
        case 3:
            box = {box.topY, box.bottomY, CONFIG_DISPLAY_HEIGHT - box.rightX,
                   CONFIG_DISPLAY_HEIGHT - box.leftX};
            break;
        default:
            break;
        }
    }
    auto rightBox = box;
    if (stereoDisparity_ != 0) {
        auto shiftCanvas = [this](EyeCanvas &output, const uint8_t *input, ScreenSide eye) {
            int16_t dx = eyeShift(eye, stereoDisparity_);
            int16_t dy = 0;
            switch (gfxCanvas_.getRotation()) {
            case 1:
                std::swap(dx, dy);
                break;
            case 2:
                dx = -dx;
                break;
            case 3:
                dy = -dx;
                dx = 0;
                break;
            default:
                break;
            }
            output.fillScreen(bg);
            output.drawBitmap(dx, dy, input, CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT, 1, 0);
            return SolPoint{dx, dy};
        };
        auto leftShift = shiftCanvas(*shiftedLeftCanvas_, left, ScreenSide::Left);
        auto rightShift = shiftCanvas(*shiftedRightCanvas_, right, ScreenSide::Right);
        left = shiftedLeftCanvas_->getBuffer();
        right = shiftedRightCanvas_->getBuffer();
        if (box.rightX > box.leftX && box.bottomY > box.topY) {
            box.leftX += leftShift.x;
            box.rightX += leftShift.x;
            box.topY += leftShift.y;
            box.bottomY += leftShift.y;
            rightBox.leftX += rightShift.x;
            rightBox.rightX += rightShift.x;
            rightBox.topY += rightShift.y;
            rightBox.bottomY += rightShift.y;
        }
    }
    driver_.update(waveform, left, vcom, altWaveform, screen, box, right, rightBox);
}

auto DisplayEinkET013TT1::setStereoDisparity(int disparity) -> bool {
    if (disparity < -MAX_STEREO_DISPARITY || disparity > MAX_STEREO_DISPARITY) {
        log_e("Invalid stereo disparity: %d", disparity);
        return false;
    }
    if (disparity == stereoDisparity_) {
        return true;
    }
    if (disparity != 0 && !shiftedLeftCanvas_.has_value()) {
        shiftedLeftCanvas_.emplace(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT);
        shiftedRightCanvas_.emplace(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT);
        if (shiftedLeftCanvas_->getBuffer() == nullptr ||
            shiftedRightCanvas_->getBuffer() == nullptr) {
            log_e("Not enough memory for global stereo frame buffers");
            shiftedLeftCanvas_.reset();
            shiftedRightCanvas_.reset();
            return false;
        }
    }
    if (disparity == 0) {
        shiftedLeftCanvas_.reset();
        shiftedRightCanvas_.reset();
    }
    stereoDisparity_ = disparity;
    clearDisplay();
    log_i("Global stereo disparity: %d pixels", disparity);
    return true;
}

#if !SIMULATOR
auto DisplayEinkET013TT1::frameBufferMemoryReport() const -> std::string {
    size_t internal = 0;
    size_t external = 0;
    auto countBuffer = [&](const uint8_t *buffer) {
        size_t size = heap_caps_get_allocated_size(const_cast<uint8_t *>(buffer));
        if (esp_ptr_external_ram(buffer)) {
            external += size;
        } else {
            internal += size;
        }
    };
    countBuffer(gfxCanvas_.getBuffer());
    if (rightCanvas_) {
        countBuffer(rightCanvas_->getBuffer());
    }
    if (shiftedLeftCanvas_) {
        countBuffer(shiftedLeftCanvas_->getBuffer());
        countBuffer(shiftedRightCanvas_->getBuffer());
    }
    return StringFormat("DRAW_BUFFERS_INTERNAL,%zu;DRAW_BUFFERS_PSRAM,%zu;STEREO_DEPTH,%d;"
                        "STEREO_CANVAS,%d",
                        internal, external, stereoDisparity_, stereoEnabled());
}
#endif

void DisplayEinkET013TT1::clearFrameBuffer() {
    gfxCanvas_.fillScreen(bg);
    if (rightCanvas_.has_value()) {
        rightCanvas_->fillScreen(bg);
    }
}

auto DisplayEinkET013TT1::setStereoEnabled(bool enabled) -> bool {
    if (enabled == stereoEnabled()) {
        return enabled;
    }
    if (enabled) {
        rightCanvas_.emplace(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT);
        if (rightCanvas_->getBuffer() == nullptr) {
            log_e("Not enough memory for a right eye frame buffer");
            rightCanvas_.reset();
            return false;
        }
        rightCanvas_->setRotation(gfxCanvas_.getRotation());
        rightCanvas_->setTextWrap(false);
        rightCanvas_->setTextColor(fg, bg);
        assert(gfxCanvas_.getBuffer() != nullptr);
        memcpy(rightCanvas_->getBuffer(), gfxCanvas_.getBuffer(),
               CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT / 8);
    } else {
        setDrawEye(ScreenSide::Left);
        rightCanvas_.reset();
    }
    log_i("Stereo %s", enabled ? "enabled" : "disabled");
    return enabled;
}

void DisplayEinkET013TT1::setDrawEye(ScreenSide eye) {
    drawCanvas_ = (eye == ScreenSide::Right && rightCanvas_.has_value()) ? &rightCanvas_.value()
                                                                         : &gfxCanvas_;
    gfxClass_ = drawCanvas_;
}

void DisplayEinkET013TT1::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                        int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                        ScreenSide screen, SolRect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (mirrorToSerial) {
        sendBufferToSerial(gfxCanvas_.getBuffer());
    }
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }
    int adjustedVcom = driver_.getVcom() + vcomNudge;
    switch (updateType) {
    case UNSPECIFIED:
        if (getPartialRefresh()) {
            // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
            if (getInversion()) {
                if (!fullRefreshNext_) {
                    driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE,
                                 screen);
                } else {
                    driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE,
                                 screen);
                }
            } else {
                if (!fullRefreshNext_) {
                    driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE,
                                 screen);
                } else {
                    driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE,
                                 screen);
                }
            }
            // NOLINTEND(bugprone-branch-clone)
        } else {
            driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
            driverUpdate(FULL_REFRESH_WAVEFORM_REGULAR, adjustedVcom, BC_WAVEFORM_NONE, screen);
        }
        break;
    case FULL:
        firstPartialTime_ = 0;
        numPartials_ = 0;
        if (!fullRefreshNext_ && altRect.size.width > 0 && altRect.size.height > 0 &&
            altUpdateType == UNSPECIFIED) {
            driverUpdate(BC_WAVEFORM_NONE, adjustedVcom, FULL_BIG_ORIGINAL_WAVEFORM, screen,
                         {altRect.origin.x, altRect.right(), altRect.origin.y, altRect.bottom()});
            break;
        }
        if (altUpdateType != UNSPECIFIED) {
            switch (altUpdateType) {
            // right now these are the only two supported
            case FAST:
                driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom,
                             BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                             {altRect.origin.x, altRect.origin.x + altRect.size.width,
                              altRect.origin.y, altRect.origin.y + altRect.size.height});
                break;
            case ERASE:
                driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, ERASE_WAVEFORM_REGULAR,
                             screen,
                             {altRect.origin.x, altRect.origin.x + altRect.size.width,
                              altRect.origin.y, altRect.origin.y + altRect.size.height});
                break;
            default:
                log_e("UNSUPPORTED ALT UPDATE TYPE");
                break;
            }
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE, screen);
        }
        break;
    case PARTIAL:
        if (!forceRefreshType_ && (((numPartials_ > 2) && (Millis() - firstPartialTime_ < 5000)) ||
                                   (numPartials_ > 10) || (au4Count_ > 50))) {
            fullRefreshNext_ = true;
        }
        if (!fullRefreshNext_) {
            if (firstPartialTime_ == 0) {
                firstPartialTime_ = Millis();
            }
            numPartials_++;

            if (getInversion()) {
                driverUpdate(PARTIAL_REFRESH_WAVEFORM_INVERTED, adjustedVcom, BC_WAVEFORM_NONE,
                             screen);
            } else {
                if (altUpdateType != UNSPECIFIED) {
                    switch (altUpdateType) {
                    // right now these are the only two supported
                    case FAST:
                        driverUpdate(PARTIAL_REFRESH_WAVEFORM_REGULAR, adjustedVcom,
                                     BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                                     {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                      altRect.origin.y, altRect.origin.y + altRect.size.height});
                        break;
                    case ERASE:
                        driverUpdate(PARTIAL_REFRESH_WAVEFORM_REGULAR, adjustedVcom,
                                     ERASE_WAVEFORM_REGULAR, screen,
                                     {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                      altRect.origin.y, altRect.origin.y + altRect.size.height});
                        break;
                    default:
                        log_e("UNSUPPORTED ALT UPDATE TYPE");
                        break;
                    }
                } else {
                    driverUpdate(PARTIAL_REFRESH_WAVEFORM_REGULAR, adjustedVcom, BC_WAVEFORM_NONE,
                                 screen);
                }
            }
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
            au4Count_ = 0;
        }
        break;
    case FAST:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driverUpdate(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, adjustedVcom, BC_WAVEFORM_NONE,
                             screen);
            } else {
                driverUpdate(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, adjustedVcom, BC_WAVEFORM_NONE,
                             screen);
            }
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case ERASE:
        if (getInversion()) {
            if (altUpdateType != UNSPECIFIED) {
                switch (altUpdateType) {
                // right now these are the only two supported
                case FAST:
                    driverUpdate(ERASE_WAVEFORM_REGULAR, adjustedVcom,
                                 BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                                 {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                  altRect.origin.y, altRect.origin.y + altRect.size.height});
                    break;
                case ERASE:
                    driverUpdate(ERASE_WAVEFORM_REGULAR, adjustedVcom, ERASE_WAVEFORM_REGULAR,
                                 screen,
                                 {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                  altRect.origin.y, altRect.origin.y + altRect.size.height});
                    break;
                default:
                    log_e("UNSUPPORTED ALT UPDATE TYPE");
                    break;
                }
            } else {
                driverUpdate(ERASE_WAVEFORM_REGULAR, adjustedVcom, BC_WAVEFORM_NONE, screen);
            }
        } else {
            driverUpdate(ERASE_WAVEFORM_INVERTED, adjustedVcom, BC_WAVEFORM_NONE, screen);
        }
        numPartials_++;
        break;
    case FAST_ORIGINAL_AU4:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driverUpdate(AU4_ORIGINAL, adjustedVcom - 2000, BC_WAVEFORM_NONE, screen);
            } else {
                driverUpdate(AU4_ORIGINAL, adjustedVcom + 2000, BC_WAVEFORM_NONE, screen);
            }
            au4Count_++;
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case ORIGINAL_PARTIAL:
        if (!fullRefreshNext_) {
            driverUpdate(PARTIAL_REFRESH_ORIGINAL, 0, BC_WAVEFORM_NONE, screen);
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, 0, BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case FAST_INDICATOR:
        if (!fullRefreshNext_) {
            auto indicatorWaveform = getInversion() ? BC_INVERTED_EINK_10_13C_WAVEFORM_AU4
                                                    : BC_REGULAR_EINK_10_13C_WAVEFORM_AU4;
            int indicatorVcom = adjustedVcom + (getInversion() ? -2000 : 2000);
            if (altRect.size.width > 0 && altRect.size.height > 0) {
                // NOLINTNEXTLINE(readability-suspicious-call-argument) - only drive inside the box.
                driverUpdate(BC_WAVEFORM_NONE, indicatorVcom, indicatorWaveform, screen,
                             {altRect.origin.x, altRect.origin.x + altRect.size.width,
                              altRect.origin.y, altRect.origin.y + altRect.size.height});
                break;
            }
            if (getInversion()) {
                driverUpdate(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, adjustedVcom - 2000,
                             BC_WAVEFORM_NONE, screen);
            } else {
                driverUpdate(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, adjustedVcom + 2000,
                             BC_WAVEFORM_NONE, screen);
            }
        } else {
            driverUpdate(FULL_BIG_ORIGINAL_WAVEFORM, adjustedVcom, BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    }

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        this->clearFrameBuffer();
    }

    fullRefreshNext_ = false;

    // Uncomment for some TTF performance debugging
    // #if CONFIG_TINYFONT_TTF
    //     font_->getFontData()->cache.showStats();
    //     log_d("Free stack space: %u", uxTaskGetStackHighWaterMark(nullptr));
    // #endif
}

void DisplayEinkET013TT1::drawFullscreenBitmapFromFile(RendererStream &stream) {
    // data is packed 1 bit binary
    uint8_t *buffer = drawCanvas_->getBuffer();
    assert(buffer);
    int size = drawCanvas_->width() * drawCanvas_->height() / 8;

    if (stream.readBytes(buffer, size) != size) {
        log_e("failed to read pbm bytes");
        return;
    }

    // pbm files have 0 white and 1 black
    // invert to display properly (for now)
    for (int i = 0; i < size; i++) {
        buffer[i] = buffer[i] ^ 0xFF; // XOR to invert all bits
    }
}

void DisplayEinkET013TT1::sendBufferToSerial(const uint8_t *buffer, bool screenshot) const {
    constexpr size_t byteSize = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT / 8;

    std::string *prefix;
    if (screenshot) {
        static std::string screenshotPrefix;
        if (screenshotPrefix.empty()) {
            screenshotPrefix = StringFormat("SCREENSHOT %zu", byteSize);
        }
        prefix = &screenshotPrefix;
    } else {
        static std::string framePrefix;
        if (framePrefix.empty()) {
            framePrefix = StringFormat("FRAME %zu", byteSize);
        }
        prefix = &framePrefix;
    }
    USBSerial::getInstance().txSendLine(*prefix);
    USBSerial::getInstance().txSendData(buffer, byteSize);
    USBSerial::getInstance().txSendLine("");
}

void DisplayEinkET013TT1::sendCurrentScreenToSerial(bool screenshot) const {
    auto buffer = driver_.getBuffer();
    sendBufferToSerial(buffer, screenshot);
};

#endif
