#include <Defer/Defer.hpp>
#include <algorithm>
#include <array>
#include <catch_amalgamated.hpp>
#include <filesystem>
#include <unistd.h>

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
#include "UI/Menu/GenericSettingsMenuScreen.hpp"

extern Telemetry telemetry;
extern int simulatorTestMinutes;

TEST_CASE("eyeShift splits disparity between the eyes", "[stereo]") {
    for (int disparity = -5; disparity <= 5; disparity++) {
        CAPTURE(disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Left, disparity) -
                  DisplaySystem::eyeShift(ScreenSide::Right, disparity) ==
              disparity);
        CHECK(DisplaySystem::eyeShift(ScreenSide::Both, disparity) == 0);
    }
}

TEST_CASE("Stereo depth selection applies and rerenders immediately", "[stereo]") {
    auto display = createDisplay();
    PreferencesStore prefs;
    REQUIRE(prefs.begin(""));
    auto menu = GenericSettingsMenuScreen::stereoDepthMenu(*display, prefs, "System");
    menu->render(*display);
    menu->clearNeedsRerender();
    menu->setSelectedIndex(DisplaySystem::MAX_STEREO_DISPARITY /
                               DisplaySystem::STEREO_DISPARITY_STEP + 1);
    menu->render(*display);
    menu->clearNeedsRerender();
    menu->processCommand(RemoteCommand::ENTER);
    CHECK(display->stereoDisparity() == 4);
    CHECK(menu->needsRerender());
    menu->render(*display);
    CHECK(prefs.stereoDisparity.get() == 4);
    prefs.stereoDisparity.remove();
    auto menuDepth = GenericSettingsMenuScreen::stereoDepthMenu(*display, prefs, "System", true);
    menuDepth->setSelectedIndex(4);
    menuDepth->processCommand(RemoteCommand::ENTER);
    CHECK(prefs.menuDisparity.get() == 4);
    prefs.menuDisparity.remove();
}

TEST_CASE("Global stereo offsets output without changing drawing canvases", "[stereo]") {
    auto display = createDisplay();
    bool darkMode = GENERATE(false, true);
    display->invertDisplay(darkMode);
    uint8_t foregroundPixel = darkMode ? 255 : 0;
    auto *canvas = display->gfxClass();
    canvas->drawPixel(20, 30, display->fg);
    canvas->drawPixel(0, 30, display->fg);
    canvas->drawPixel(CONFIG_DISPLAY_WIDTH - 1, 30, display->fg);
    for (int depth = -DisplaySystem::MAX_STEREO_DISPARITY;
         depth <= DisplaySystem::MAX_STEREO_DISPARITY; ++depth) {
        CAPTURE(depth, darkMode);
        REQUIRE(display->setStereoDisparity(depth));
        REQUIRE_FALSE(display->stereoEnabled());
        CHECK(display->stereoDisparity() == depth);
        for (int repeat = 0; repeat < 2; ++repeat) {
            display->updateDisplay(DisplaySystem::UpdateType::FULL, false);
            for (int x = 0; x < CONFIG_DISPLAY_WIDTH; ++x) {
                auto checkEye = [&](ScreenSide eye, const uint8_t *output) {
                    int sourceX = x - DisplaySystem::eyeShift(eye, depth);
                    bool foreground =
                        sourceX == 0 || sourceX == 20 || sourceX == CONFIG_DISPLAY_WIDTH - 1;
                    CHECK(output[30 * CONFIG_DISPLAY_WIDTH + x] ==
                          ((foreground ? display->fg : display->bg) != 0 ? 255 : 0));
                };
                checkEye(ScreenSide::Left, display->buffer().get());
                checkEye(ScreenSide::Right, display->rightBuffer().get());
            }
        }
        CHECK(canvas == display->gfxClass());
    }
    REQUIRE(display->setStereoDisparity(2));
    REQUIRE(display->setStereoEnabled(true));
    display->setDrawEye(ScreenSide::Right);
    display->fillRect(20, 30, 1, 1, display->bg);
    display->fillRect(40, 30, 1, 1, display->fg);
    display->setDrawEye(ScreenSide::Left);
    display->updateDisplay(DisplaySystem::UpdateType::FULL, false);
    CHECK(display->buffer()[30 * CONFIG_DISPLAY_WIDTH + 21] == foregroundPixel);
    CHECK(display->rightBuffer()[30 * CONFIG_DISPLAY_WIDTH + 19] == (display->bg != 0 ? 255 : 0));
    CHECK(display->rightBuffer()[30 * CONFIG_DISPLAY_WIDTH + 39] == foregroundPixel);
    display->setStereoEnabled(false);
    CHECK(display->stereoDisparity() == 2);
    REQUIRE(display->setStereoDisparity(0));
    display->updateDisplay(DisplaySystem::UpdateType::FULL, false);
    CHECK(display->buffer()[30 * CONFIG_DISPLAY_WIDTH + 20] == foregroundPixel);
    CHECK(display->rightBuffer()[30 * CONFIG_DISPLAY_WIDTH + 20] == foregroundPixel);
}

TEST_CASE("Global stereo follows screen rotation", "[stereo]") {
    auto display = createDisplay();
    int rotation = GENERATE(0, 1, 2, 3);
    display->setRotation(rotation);
    display->fillRect(20, 30, 1, 1, display->fg);
    REQUIRE(display->setStereoDisparity(2));
    display->updateDisplay(DisplaySystem::UpdateType::FULL);
    for (auto eye : {ScreenSide::Left, ScreenSide::Right}) {
        int x = 20 + DisplaySystem::eyeShift(eye, 2);
        int y = 30;
        switch (rotation) {
        case 1:
            std::swap(x, y);
            x = CONFIG_DISPLAY_WIDTH - 1 - x;
            break;
        case 2:
            x = CONFIG_DISPLAY_WIDTH - 1 - x;
            y = CONFIG_DISPLAY_HEIGHT - 1 - y;
            break;
        case 3:
            std::swap(x, y);
            y = CONFIG_DISPLAY_HEIGHT - 1 - y;
            break;
        default:
            break;
        }
        auto *output =
            eye == ScreenSide::Left ? display->buffer().get() : display->rightBuffer().get();
        CHECK(output[y * CONFIG_DISPLAY_WIDTH + x] == display->fg);
    }
}

TEST_CASE("Scoped fast indicator updates leave the background alone", "[stereo]") {
    auto display = createDisplay();
    int depth = GENERATE(0, -2, 2);
    REQUIRE(display->setStereoDisparity(depth));
    display->fillRect(20, 20, 1, 1, display->fg);
    display->updateDisplay(DisplaySystem::UpdateType::FULL);
    for (int repeat = 0; repeat < 5; ++repeat) {
        display->fillRect(100, 100, 10, 10, display->fg);
        display->updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR, true, 0,
                               DisplaySystem::UpdateType::UNSPECIFIED, ScreenSide::Both,
                               {{90, 90}, {30, 30}});
        for (auto eye : {ScreenSide::Left, ScreenSide::Right}) {
            int shift = DisplaySystem::eyeShift(eye, depth);
            auto *output =
                eye == ScreenSide::Left ? display->buffer().get() : display->rightBuffer().get();
            CHECK(output[20 * CONFIG_DISPLAY_WIDTH + 20 + shift] == display->fg);
            CHECK(output[100 * CONFIG_DISPLAY_WIDTH + 100 + shift] == display->fg);
        }
    }
}

TEST_CASE("Scoped indicator waveforms preserve faded background particles", "[stereo]") {
    auto display = createDisplay();
    display->setSkipWaveform(false);
    bool darkMode = GENERATE(false, true);
    display->invertDisplay(darkMode);
    display->fillRect(20, 20, 1, 1, display->fg);
    display->updateDisplay(DisplaySystem::UpdateType::FULL);
    display->fillRect(100, 100, 10, 10, display->fg);
    display->updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    auto faded = display->buffer()[20 * CONFIG_DISPLAY_WIDTH + 20];
    CHECK(faded > 0);
    CHECK(faded < 255);
    for (int repeat = 0; repeat < 4; ++repeat) {
        display->fillRect(100 + repeat, 100, 10, 10, display->fg);
        display->updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR, true, 0,
                               DisplaySystem::UpdateType::UNSPECIFIED, ScreenSide::Both,
                               {{90, 90}, {30, 30}});
        CHECK(display->buffer()[20 * CONFIG_DISPLAY_WIDTH + 20] == faded);
        CHECK(display->rightBuffer()[20 * CONFIG_DISPLAY_WIDTH + 20] == faded);
    }
}

TEST_CASE("Stereo depth persists in preferences", "[stereo]") {
    auto tempPath = (std::filesystem::temp_directory_path() / "stereo-prefs-XXXXXX").string();
    int fd = mkstemp(tempPath.data());
    REQUIRE(fd >= 0);
    REQUIRE(close(fd) == 0);
    defer { std::filesystem::remove(tempPath); };
    PreferencesStore prefs;
    REQUIRE(prefs.begin(tempPath.c_str()));
    prefs.stereoDisparity.set(-2);
    PreferencesStore reloaded;
    REQUIRE(reloaded.begin(tempPath.c_str()));
    CHECK(reloaded.stereoDisparity.get() == -2);
    reloaded.stereoDisparity.remove();
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
    class RecordingDisplay : public DisplayEinkET013TT1 {
    public:
        int backgroundRefreshes = 0;
        std::vector<uint8_t> leftBackground;
        std::vector<uint8_t> rightBackground;
        SolRect indicatorRect = SolRect::zero();

        void updateDisplay(UpdateType type = UNSPECIFIED, bool clear = true, int vcom = 0,
                           UpdateType altType = UNSPECIFIED, ScreenSide eye = ScreenSide::Both,
                           SolRect rect = SolRect::zero()) override {
            if (type == FULL && stereoEnabled()) {
                ++backgroundRefreshes;
                auto size = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT / 8;
                leftBackground.assign(gfxCanvas_.getBuffer(), gfxCanvas_.getBuffer() + size);
                auto *right =
                    rightCanvas_.has_value() ? rightCanvas_->getBuffer() : gfxCanvas_.getBuffer();
                rightBackground.assign(right, right + size);
            }
            if (type == FAST_INDICATOR) {
                indicatorRect = rect;
            }
            DisplayEinkET013TT1::updateDisplay(type, clear, vcom, altType, eye, rect);
        }
    };
    class TestReadableScreen : public ReadableScreen {
    public:
        using ReadableScreen::hidePopupMenu;
        using ReadableScreen::ReadableScreen;
        using ReadableScreen::setCurrentScreen;
        using ReadableScreen::showPopupMenu;
        auto popup() -> std::shared_ptr<NewReadablePopupMenuScreen> { return popupMenuScreen_; }
        auto popupOpen() const -> bool { return currentScreen_ == popupMenuScreen_; }
        void setAutopilotForTest(int speed, int lines) {
            pageSpeed_ = speed;
            speedLinesToSkip_ = lines;
        }
        auto skippedLines() const -> int { return speedLinesToSkip_; }
    };

    SetImagesFolder("Images");
    MockClock();
    defer { UnMockClock(); };
    auto display = std::make_shared<RecordingDisplay>();
    display->setSkipWaveform(false);
    display->setSkipSignalling(true);
    REQUIRE(display->setup());
    bool darkMode = GENERATE(false, true);
    display->invertDisplay(darkMode);
    std::string imagePrefix = darkMode ? "Stereo/BookMenuDark" : "Stereo/BookMenu";
    PreferencesStore prefs;
    REQUIRE(prefs.begin(""));
    prefs.cesMode.set(true);
    prefs.firstRead.set(true);
    prefs.autoPilotSpeed.set(0);
    prefs.noShowPartialChapterSummary.set(true);
    prefs.menuDisparity.set(1);
    prefs.altPageAdvance.set(0);
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
    CHECK(display->backgroundRefreshes == 1);
    CHECK(display->indicatorRect.size.width > 0);
    CHECK(display->indicatorRect.size.width < CONFIG_DISPLAY_WIDTH);
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "Selected");
    popup->processCommand(RemoteCommand::RIGHT);
    screen->render(*display);
    CHECK(display->backgroundRefreshes == 2);
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "NextTab");
    CHECK(popup->getCurrentTabTitle() == "Style");
    popup->processCommand(RemoteCommand::RIGHT);
    screen->render(*display);
    CHECK(popup->getCurrentTabTitle() == "System");
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "System");
    popup->processCommand(RemoteCommand::RIGHT);
    screen->render(*display);
    CHECK(popup->getCurrentTabTitle() == "Popup");
    CHECK_FALSE(popup->isMenuIDInTab("Popup", "Bookmark"));
    CHECK(popup->isMenuIDInTab("Popup", "History"));

    int refreshes = display->backgroundRefreshes;
    for (int tab = 0; tab < 8; ++tab) {
        popup->processCommand(RemoteCommand::RIGHT);
        screen->render(*display);
        CHECK(display->backgroundRefreshes == tab + refreshes + 1);
        // The top book margin lies outside every popup page and must be restored in both eyes.
        for (int x = 0; x < CONFIG_DISPLAY_WIDTH; ++x) {
            int y = 25;
            auto index = y * CONFIG_DISPLAY_WIDTH + x;
            auto mask = 1 << (7 - index % 8);
            CHECK(((display->leftBackground[index / 8] & mask) != 0) == (page[index] != 0));
            CHECK(((display->rightBackground[index / 8] & mask) != 0) == (page[index] != 0));
            CHECK(display->buffer()[index] == page[index]);
            CHECK(display->rightBuffer()[index] == page[index]);
        }
    }

    prefs.firstAutopilotRead.set(true);
    screen->setAutopilotForTest(1200, 4);
    popup->processCommand(RemoteCommand::RIGHT);
    screen->render(*display);
    CHECK(screen->skippedLines() == 4);
    screen->setAutopilotForTest(0, 0);

    popup->setTabWithName("Popup");
    simulatorTestMinutes = 22;
    screen->render(*display);
    ASSERT_STEREO_VIEW_UNCHANGED(display, imagePrefix + "Clock");
    simulatorTestMinutes = 21;

    for (int depth = 0; depth <= DisplaySystem::MAX_MENU_DISPARITY; ++depth) {
        prefs.menuDisparity.set(depth);
        screen->hidePopupMenu();
        screen->showPopupMenu();
        screen->render(*display);
        CHECK(display->stereoEnabled() == (depth != 0));
        popup->processCommand(RemoteCommand::DOWN);
        screen->render(*display);
        CHECK(display->stereoEnabled() == (depth != 0));
    }
    prefs.menuDisparity.set(1);

    screen->hidePopupMenu();
    REQUIRE_FALSE(display->stereoEnabled());
    screen->render(*display);
    CHECK(std::equal(page.begin(), page.end(), display->buffer().get()));
    CHECK(std::equal(page.begin(), page.end(), display->rightBuffer().get()));

    prefs.cesMode.set(false);
    screen->processCommand(RemoteCommand::UP);
    CHECK(screen->popupOpen());
    screen->hidePopupMenu();
    prefs.altPageAdvance.set(1);
    auto oldOffset = readable->place->getOffset();
    screen->processCommand(RemoteCommand::UP);
    CHECK_FALSE(screen->popupOpen());
    CHECK(readable->place->getOffset() != oldOffset);
    screen->render(*display);
    screen->processCommand(RemoteCommand::DOWN);
    CHECK(screen->popupOpen());
    screen->render(*display);
    screen->processCommand(RemoteCommand::UP);
    CHECK(screen->popupOpen());
    screen->hidePopupMenu();
    prefs.altPageAdvance.set(0);
    prefs.cesMode.set(true);

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
