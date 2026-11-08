#pragma once

#include <cstdint>
#include <string>

#include "Displays/DisplaySystem.hpp"
#include "UI/Fonts.hpp"
#include "UI/Screen.hpp"

// Debug screen with static stereoscopic test patterns. On displays that support stereo each eye is
// drawn separately; elsewhere everything is drawn flat.
//
// Controls: LEFT/RIGHT change page, UP/DOWN adjust the current page, ENTER swaps the eyes, BACK
// exits.
class StereoTestScreen : public Screen {
public:
    explicit StereoTestScreen(DisplaySystem &display) : display_(display) {}
    ~StereoTestScreen() override;

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

private:
    enum class Page : uint8_t {
        EyeCheck,
        DepthLadder,
        Menu,
        Text,
        RandomDots,
        Cube,
        Card,
        Count,
    };

    void drawPage(ScreenSide eye);
    void drawHeader(const std::string &title, const std::string &detail);
    void drawEyeCheck(ScreenSide eye);
    void drawDepthLadder(ScreenSide eye);
    void drawMenu(ScreenSide eye);
    void drawText(ScreenSide eye);
    void drawRandomDots(ScreenSide eye);
    void drawCube(ScreenSide eye);
    void drawCard(ScreenSide eye);

    void adjust(int direction);

    // Draw text so it appears `disparity` pixels in front of the screen
    void drawTextAtDepth(ScreenSide eye, const std::string &text, int x, int y, int disparity,
                         Font *font = &fontFace0, bool inverted = false);
    void fillHeart(int16_t cx, int16_t cy, int16_t size);

    // What UP/DOWN do on the current page, or nullptr if nothing
    [[nodiscard]] auto adjustHint() const -> const char *;
    [[nodiscard]] auto contentTop() const -> int16_t;
    [[nodiscard]] auto contentBottom() const -> int16_t;

    DisplaySystem &display_;
    Page page_ = Page::EyeCheck;
    bool stereo_ = false;
    bool swapEyes_ = false;
    bool fullRefreshNext_ = true;
    int depthScale_ = 1;
    int menuSelection_ = 1;
    int dotDisparity_ = 4;
    int cubeAngle_ = 30;
};
