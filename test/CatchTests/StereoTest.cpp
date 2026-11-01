#include <catch_amalgamated.hpp>

#include "DisplayUtil.hpp"
#include "UI/StereoTestScreen.hpp"
#include "ViewTestCommon.hpp"

TEST_CASE("eyeShift splits disparity between the eyes", "[stereo]") {
    for (int disparity = -5; disparity <= 5; disparity++) {
        CAPTURE(disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Left, disparity) -
                  DisplaySystem::eyeShift(ScreenSide::Right, disparity) ==
              disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Both, disparity) == 0);
    }
}

TEST_CASE("StereoTestScreen renders a separate image for each eye", "[stereo]") {
    SetImagesFolder("Images");
    auto display = createDisplay();
    REQUIRE(display->supportsStereo());

    {
        auto screen = std::make_shared<StereoTestScreen>(*display);
        const std::array<const char *, 7> pages = {"EyeCheck",   "DepthLadder", "Menu", "Text",
                                                   "RandomDots", "Cube",        "Card"};
        for (size_t i = 0; i < pages.size(); i++) {
            if (i > 0) {
                screen->processCommand(RemoteCommand::RIGHT);
            }
            screen->render(*display);
            REQUIRE(display->stereoEnabled());
            ASSERT_STEREO_VIEW_UNCHANGED(display, std::string("Stereo/") + pages[i]);
        }

        // Swapping the eyes on the eye check page puts the right eye's shape in the left eye
        screen->processCommand(RemoteCommand::RIGHT);
        screen->processCommand(RemoteCommand::ENTER);
        screen->render(*display);
        ASSERT_STEREO_VIEW_UNCHANGED(display, "Stereo/EyeCheckSwapped");
    }

    // Leaving the screen goes back to a single image for both eyes
    REQUIRE_FALSE(display->stereoEnabled());
    display->fillRect(10, 10, 20, 20, display->fg);
    display->updateDisplay(DisplaySystem::UpdateType::FULL);
    size_t size = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT;
    CHECK(std::equal(display->buffer().get(), display->buffer().get() + size,
                     display->rightBuffer().get()));
}
