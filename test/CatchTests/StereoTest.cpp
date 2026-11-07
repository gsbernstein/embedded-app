#include <Defer/Defer.hpp>
#include <algorithm>
#include <array>
#include <catch_amalgamated.hpp>

#include "DisplayUtil.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/StereoTestScreen.hpp"
#include "ViewTestCommon.hpp"

#define protected public
#define private public
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#undef protected
#undef private

#include "UI/ReadableScreen.hpp"

extern Telemetry telemetry;

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

TEST_CASE("MenuView shifts selected item and indicator by selectedItemShift", "[stereo]") {
    auto display = createDisplay();
    MenuView menuView(*display);

    class TrackingMenuItemView : public MenuItemView {
    public:
        using MenuItemView::MenuItemView;
        SolRect drawnRect;
        SolRect fastDrawnRect;

        void drawInRect(SolRect rect) override { drawnRect = rect; }

        void drawFastOnlyElements() override { fastDrawnRect = layoutContext_.screenFrame(); }

        auto layout(LayoutContext &context, SolSize size) -> SolSize override {
            return {size.width, 20};
        }
    };

    auto item0 = std::make_shared<TrackingMenuItemView>(*display);
    auto item1 = std::make_shared<TrackingMenuItemView>(*display);

    menuView.addMenuItem(std::make_shared<MenuItemSingleLine>("First", []() {}));
    menuView.addMenuItem(std::make_shared<MenuItemSingleLine>("Second", []() {}));
    menuView.menuItemViews_[0] = item0;
    menuView.menuItemViews_[1] = item1;
    menuView.setFrame({{0, 0}, {100, 50}});
    menuView.setSelectedIndex(1);
    REQUIRE(menuView.getSelectedIndex() == 1);
    menuView.layoutIfNeeded();
    auto indicatorFrame = menuView.indicatorView_.frame();

    int16_t shift = 3;
    menuView.setSelectedItemShift(shift);

    menuView.draw();

    // Item 0 (unselected) remains unshifted
    CHECK(item0->drawnRect.origin.x == 10);
    // Item 1 (selected) is shifted during draw
    CHECK(item1->drawnRect.origin.x == 10 + shift);

    // After draw completes, frame is restored
    CHECK(item0->frame().origin.x == 10);
    CHECK(item1->frame().origin.x == 10);

    menuView.drawFastElements();

    // Fast drawn element for selected item is shifted
    CHECK(item1->fastDrawnRect.origin.x == 10 + shift);
    CHECK(item1->frame().origin.x == 10);
    CHECK(menuView.indicatorView_.frame() == indicatorFrame);
    display->updateDisplay(DisplaySystem::UpdateType::FULL);
    auto indicatorX = indicatorFrame.origin.x + shift + indicatorFrame.size.width / 2;
    auto indicatorY = indicatorFrame.origin.y + indicatorFrame.size.height / 2;
    CHECK(display->buffer()[indicatorY * CONFIG_DISPLAY_WIDTH + indicatorX] == 0);
}

TEST_CASE("In-book menu preserves the page and restores mono on exit", "[stereo][book-menu]") {
    class TestReadableScreen : public ReadableScreen {
    public:
        using ReadableScreen::hidePopupMenu;
        using ReadableScreen::ReadableScreen;
        using ReadableScreen::setCurrentScreen;
        using ReadableScreen::showPopupMenu;
        auto popup() -> std::shared_ptr<NewReadablePopupMenuScreen> { return popupMenuScreen_; }
    };

    SetImagesFolder("Images");
    MockClock();
    defer { UnMockClock(); };
    auto display = createDisplay();
    bool darkMode = GENERATE(false, true);
    display->invertDisplay(darkMode);
    std::string imagePrefix = darkMode ? "Stereo/BookMenuDark" : "Stereo/BookMenu";
    PreferencesStore prefs;
    REQUIRE(prefs.begin(""));
    prefs.cesMode.set(true);
    prefs.firstRead.set(true);
    prefs.autoPilotSpeed.set(0);
    prefs.noShowPartialChapterSummary.set(true);
    PosixFileSystem fileSystem("./test/CatchTests/TestData");
    // Use the app's persistent telemetry instance: callbacks can run when a later test starts
    // FreeRTOS, after this synchronous rendering fixture has been destroyed.
    LightingController lightingController(prefs);
    Battery battery(fileSystem, prefs, telemetry);
    auto solDrm = std::make_shared<SolDrm>(prefs);
    LocalStore localStore(fileSystem, prefs, battery, telemetry, solDrm);
    auto readable = std::make_shared<Readable>();
    auto doc = LoadJsonDoc(fileSystem, "/readables.json");
    readable->fromJson((*doc)["readables"][0]);
    auto [path, fileSize] = LocalStore::readableFilePathForReading(fileSystem, readable);
    auto stream = fileSystem.newGzipRendererStream(path.c_str());
    auto screen = std::make_unique<TestReadableScreen>(stream, *display, readable, prefs, telemetry,
                                                       fileSystem, lightingController, localStore,
                                                       &fontFace0);

    screen->render(*display);
    size_t pixelCount = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT;
    std::vector<uint8_t> page(display->buffer().get(), display->buffer().get() + pixelCount);
    screen->showPopupMenu();
    screen->render(*display);
    REQUIRE(display->stereoEnabled());
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix);
    auto popup = screen->popup();
    auto baseFrame = popup->frame();

    popup->processCommand(RemoteCommand::DOWN);
    screen->render(*display);
    CHECK(popup->frame() == baseFrame);
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "Selected");
    popup->processCommand(RemoteCommand::RIGHT);
    screen->render(*display);
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "NextTab");

    screen->hidePopupMenu();
    REQUIRE_FALSE(display->stereoEnabled());
    screen->render(*display);
    CHECK(std::equal(page.begin(), page.end(), display->buffer().get()));
    CHECK(std::equal(page.begin(), page.end(), display->rightBuffer().get()));

    screen->showPopupMenu();
    screen->render(*display);
    REQUIRE(display->stereoEnabled());
    auto otherScreen = std::make_shared<StereoTestScreen>(*display);
    screen->setCurrentScreen(otherScreen);
    REQUIRE_FALSE(display->stereoEnabled());
    screen->render(*display);
    REQUIRE(display->stereoEnabled());

    // Destroying an inactive cached popup must not disable another screen's stereo.
    popup.reset();
    screen.reset();
    REQUIRE(display->stereoEnabled());
    otherScreen.reset();
    REQUIRE_FALSE(display->stereoEnabled());
}
