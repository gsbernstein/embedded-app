#pragma once

#include <optional>

#include "DisplaySystem.hpp"
#if SIMULATOR
#include "Displays/FakeEinkET013TT1.hpp"
#else
#include "Displays/EinkET013TT1.hpp"
#endif
#include "sindarin-debug.h"

static_assert(CONFIG_DISPLAY_WIDTH % 8 == 0, "CONFIG_DISPLAY_WIDTH must be a multiple of 8");
static_assert(CONFIG_DISPLAY_HEIGHT % 8 == 0, "CONFIG_DISPLAY_HEIGHT must be a multiple of 8");

// GFXcanvas1 has virtual methods but a non-virtual destructor, so it can only be owned by value
// safely through a final subclass.
class EyeCanvas final : public GFXcanvas1 {
public:
    using GFXcanvas1::GFXcanvas1;
};

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1() noexcept : gfxCanvas_(CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayEinkET013TT1(DisplayEinkET013TT1 const &) = delete;

    ~DisplayEinkET013TT1() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {30, 30, 20, 20};
    };
    [[nodiscard]] auto minTextInsets() const -> DisplayInset override { return {5, 5, 5, 5}; };
    [[nodiscard]] auto minTextBoxWidth() const -> int16_t override { return 125; };
    [[nodiscard]] auto minTextBoxHeight() const -> int16_t override { return 45; };

    auto setup() -> bool override;

    // TODO: Probably this is a good candidate for a struct for params since there are a lot of
    // values that can be passed and some may be unset defaults.
    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       SolRect altRect = {{0, 0}, {0, 0}}) override;

    void beginPowerOn() override { driver_.beginPowerOn(); };

    void clearDisplay() override { fullRefreshNext_ = true; };

    void clearFrameBuffer() override;

    [[nodiscard]] auto supportsStereo() const -> bool override { return true; }
    auto setStereoEnabled(bool enabled) -> bool override;
    [[nodiscard]] auto stereoEnabled() const -> bool override { return rightCanvas_.has_value(); }
    void setDrawEye(ScreenSide eye) override;
    void invertDisplay(bool newInvert) override;

    // For those moments where you need to expose the driver directly
    auto getDriver() -> EinkET013TT1 * { return &driver_; }

    void drawFullscreenBitmapFromFile(RendererStream &stream) override;

    [[nodiscard]] auto darkPictureVComAdj(uint8_t darkPixelsPercent) const -> int override {
        return darkPixelsPercent > 40 ? -2500 : 0;
    }

#if SIMULATOR
    auto renderQueue() -> QueueHandle_t override { return driver_.renderQueue(); }

    auto buffer() -> std::unique_ptr<uint8_t[]> & override { return driver_.buffer(); }

    // Particle positions for the right eye
    auto rightBuffer() -> std::unique_ptr<uint8_t[]> & { return driver_.rightBuffer(); }

    auto getPixel(int x, int y) -> uint8_t {
        return driver_.getPixel(gfxCanvas_.getBuffer(), x, y);
    };

    // For the simulator to know if it should skip the waveform processing
    auto getSkipWaveform() -> bool override { return driver_.getSkipWaveform(); }

    void setSkipWaveform(bool skip) override { driver_.setSkipWaveform(skip); }

    // For the simulator to know if it should skip the signalling queue
    auto getSkipSignalling() -> bool override { return driver_.getSkipSignalling(); }

    void setSkipSignalling(bool skip) override { driver_.setSkipSignalling(skip); }
#endif

    void sendCurrentScreenToSerial(bool screenshot = false) const override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return CONFIG_DISPLAY_WIDTH; }
    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return CONFIG_DISPLAY_HEIGHT; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return drawCanvas_->getBuffer(); }

    // Send a frame buffer out the serial port.
    void sendBufferToSerial(const uint8_t *buffer, bool screenshot = false) const;

    // Send the frame buffer(s) to the driver, with a separate right eye image when in stereo
    void driverUpdate(const EinkET013TT1Waveform &waveform, int32_t vcom,
                      const EinkET013TT1Waveform &altWaveform, ScreenSide screen,
                      EinkBox box = EinkBox::zero());

    // Frame buffer for the left eye, or both eyes when not in stereo
    GFXcanvas1 gfxCanvas_;
    // Frame buffer for the right eye, only allocated while in stereo
    std::optional<EyeCanvas> rightCanvas_;
    // The canvas currently being drawn to (gfxClass_ points at it as well)
    GFXcanvas1 *drawCanvas_ = &gfxCanvas_;
    EinkET013TT1 driver_;
    bool fullRefreshNext_ = false;
    uint16_t au4Count_ = 0;
    uint16_t numPartials_ = 0;
    unsigned long firstPartialTime_ = 0;
};
