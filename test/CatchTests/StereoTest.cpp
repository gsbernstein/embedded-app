#include <algorithm>
#include <catch_amalgamated.hpp>

#include "DisplayUtil.hpp"

TEST_CASE("eyeShift splits disparity between the eyes", "[stereo]") {
    for (int disparity = -5; disparity <= 5; disparity++) {
        CAPTURE(disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Left, disparity) -
                  DisplaySystem::eyeShift(ScreenSide::Right, disparity) ==
              disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Both, disparity) == 0);
    }
}

TEST_CASE("DisplayEinkET013TT1 preserves canvas contents when stereo is enabled", "[stereo]") {
    auto display = createDisplay();
    REQUIRE(display->supportsStereo());

    display->gfxClass()->drawPixel(10, 20, display->fg);
    display->gfxClass()->drawPixel(50, 60, display->fg);
    auto *monoCanvas = display->gfxClass();

    // Enable stereo
    REQUIRE(display->setStereoEnabled(true));
    REQUIRE(display->stereoEnabled());
    display->updateDisplay(DisplaySystem::UpdateType::FULL, false);

    size_t size = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT;
    CHECK(display->buffer()[20 * CONFIG_DISPLAY_WIDTH + 10] == 0);
    CHECK(display->buffer()[60 * CONFIG_DISPLAY_WIDTH + 50] == 0);
    CHECK(std::equal(display->buffer().get(), display->buffer().get() + size,
                     display->rightBuffer().get()));

    // Drawing to right eye affects only right canvas
    display->setDrawEye(ScreenSide::Right);
    CHECK(display->gfxClass() != monoCanvas);
    display->gfxClass()->drawPixel(30, 40, display->fg);
    display->updateDisplay(DisplaySystem::UpdateType::FULL, false);

    CHECK(display->buffer()[40 * CONFIG_DISPLAY_WIDTH + 30] == 255);
    CHECK(display->rightBuffer()[40 * CONFIG_DISPLAY_WIDTH + 30] == 0);
    CHECK_FALSE(std::equal(display->buffer().get(), display->buffer().get() + size,
                           display->rightBuffer().get()));

    display->setStereoEnabled(false);
    REQUIRE_FALSE(display->stereoEnabled());
    CHECK(display->gfxClass() == monoCanvas);
}
