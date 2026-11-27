#include <memory>
#include <string>

#include "DisplayUtil.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"

// Mock the time in SolClock
void MockClock();

void UnMockClock();

// NOTE: If not running app_main from here, you need to have at least one task already created
// before calling this API.
void StartFreeRtos(bool runAppMain = false);

void StopFreeRtos();

void StartAppMain(FONT_CHOICES font = FONT_CHOICES::PREFS_SOL_SMALL, int x = 42, int y = 30,
                  int lineSpacing = 0, std::string prefsDir = "test/CatchTests/TestData");

void SetImagesFolder(std::string folder);

void SendRemoteCommand(RemoteCommand command);

auto WaitForDraw(DisplayEinkET013TT1 &display, bool continueAfterDraw = true,
                 long timeoutSeconds = 10) -> bool;

auto HasQueuedDraw(DisplayEinkET013TT1 &display) -> bool;

#define WAIT_FOR_DRAW(display, ...) REQUIRE(WaitForDraw(display, ##__VA_ARGS__))

// TODO: This struct is probably overkill
struct ViewTestResult {
    bool passed;
    std::string acceptedImagePath;
    std::string renderedImagePath;
    std::string diffImagePath;
};

// Compares what is shown to the given eye (the left eye shows the same image as the right unless
// stereo is enabled).
auto CheckViewUnchanged(DisplayEinkET013TT1 &display, std::string key,
                        ScreenSide eye = ScreenSide::Left) -> ViewTestResult;

auto CheckViewUnchanged(std::shared_ptr<DisplayEinkET013TT1> display, std::string key,
                        ScreenSide eye = ScreenSide::Left) -> ViewTestResult;

#define ASSERT_VIEW_UNCHANGED(display, key) CHECK(CheckViewUnchanged(display, key).passed)

// Checks each eye separately against "<key>-left" and "<key>-right"
#define ASSERT_STEREO_VIEW_UNCHANGED(display, key)                                                 \
    do {                                                                                           \
        CHECK(CheckViewUnchanged(display, std::string(key) + "-left", ScreenSide::Left).passed);   \
        CHECK(CheckViewUnchanged(display, std::string(key) + "-right", ScreenSide::Right).passed); \
    } while (0)

auto GetRootDisplay() -> DisplayEinkET013TT1 &;

void SetupFileSystemMock(const std::string &directory);

auto LoadJsonDoc(FileSystem &fileSystem, const std::string &path)
    -> std::unique_ptr<SpiRamJsonDocument>;
