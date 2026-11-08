#include "StereoTestScreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <vector>

namespace {

constexpr int PAGE_COUNT = 7;
constexpr int MAX_DEPTH_SCALE = 3;
constexpr int MAX_DOT_DISPARITY = 8;
constexpr int CUBE_ANGLE_STEP = 15;
constexpr int16_t MARGIN = 4;
// Keeps the bottom line of text clear of the edge, which the optics crop slightly
constexpr int16_t BOTTOM_PADDING = 6;
constexpr float DEGREES_TO_RADIANS = static_cast<float>(M_PI) / 180.0F;

auto otherEye(ScreenSide eye) -> ScreenSide {
    switch (eye) {
    case ScreenSide::Left:
        return ScreenSide::Right;
    case ScreenSide::Right:
        return ScreenSide::Left;
    case ScreenSide::Both:
        return ScreenSide::Both;
    }
    return eye;
}

auto floorDiv(int value, int divisor) -> int {
    int quotient = value / divisor;
    return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

// Deterministic pseudo-random dot for random-dot stereograms
auto dotAt(int x, int y, uint32_t seed) -> bool {
    uint32_t h = static_cast<uint32_t>(x) * 374761393U + static_cast<uint32_t>(y) * 668265263U +
                 seed * 2246822519U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return ((h ^ (h >> 16)) & 1U) != 0;
}

struct Vec3 {
    float x;
    float y;
    float z;
};

} // namespace

StereoTestScreen::~StereoTestScreen() { display_.setStereoEnabled(false); }

void StereoTestScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        page_ = static_cast<Page>((static_cast<int>(page_) + PAGE_COUNT - 1) % PAGE_COUNT);
        fullRefreshNext_ = true;
        break;
    case RemoteCommand::RIGHT:
        page_ = static_cast<Page>((static_cast<int>(page_) + 1) % PAGE_COUNT);
        fullRefreshNext_ = true;
        break;
    case RemoteCommand::UP:
        adjust(1);
        break;
    case RemoteCommand::DOWN:
        adjust(-1);
        break;
    case RemoteCommand::ENTER:
        swapEyes_ = !swapEyes_;
        break;
    case RemoteCommand::BACK:
        display_.setStereoEnabled(false);
        back();
        return;
    default:
        return;
    }
    setNeedsRerender();
}

void StereoTestScreen::adjust(int direction) {
    switch (page_) {
    case Page::Menu:
        menuSelection_ = std::clamp(menuSelection_ - direction, 0, 3);
        break;
    case Page::RandomDots:
        dotDisparity_ =
            std::clamp(dotDisparity_ + direction, -MAX_DOT_DISPARITY, MAX_DOT_DISPARITY);
        break;
    case Page::Cube:
        cubeAngle_ = (cubeAngle_ + direction * CUBE_ANGLE_STEP + 360) % 360;
        break;
    case Page::DepthLadder:
    case Page::Text:
    case Page::Card:
        depthScale_ = std::clamp(depthScale_ + direction, 0, MAX_DEPTH_SCALE);
        break;
    case Page::EyeCheck:
    case Page::Count:
        break;
    }
}

void StereoTestScreen::render(DisplaySystem &display) {
    stereo_ = display.setStereoEnabled(true);
    display.clearFrameBuffer();
    display.drawForEachEye([this](ScreenSide eye) { drawPage(swapEyes_ ? otherEye(eye) : eye); });
    display.updateDisplay(fullRefreshNext_ ? DisplaySystem::UpdateType::FULL
                                           : DisplaySystem::UpdateType::PARTIAL);
    fullRefreshNext_ = false;
}

void StereoTestScreen::drawPage(ScreenSide eye) {
    switch (page_) {
    case Page::EyeCheck:
        drawEyeCheck(eye);
        break;
    case Page::DepthLadder:
        drawDepthLadder(eye);
        break;
    case Page::Menu:
        drawMenu(eye);
        break;
    case Page::Text:
        drawText(eye);
        break;
    case Page::RandomDots:
        drawRandomDots(eye);
        break;
    case Page::Cube:
        drawCube(eye);
        break;
    case Page::Card:
        drawCard(eye);
        break;
    case Page::Count:
        break;
    }
}

auto StereoTestScreen::adjustHint() const -> const char * {
    switch (page_) {
    case Page::DepthLadder:
    case Page::Text:
    case Page::Card:
        return "depth";
    case Page::Menu:
        return "select";
    case Page::RandomDots:
        return "disparity";
    case Page::Cube:
        return "spin";
    case Page::EyeCheck:
    case Page::Count:
        break;
    }
    return nullptr;
}

auto StereoTestScreen::contentTop() const -> int16_t {
    return static_cast<int16_t>(display_.defaultLineHeight() + MARGIN);
}

auto StereoTestScreen::contentBottom() const -> int16_t {
    return static_cast<int16_t>(display_.height() - display_.defaultLineHeight() - BOTTOM_PADDING -
                                MARGIN);
}

void StereoTestScreen::drawHeader(const std::string &title, const std::string &detail) {
    int16_t w = display_.width();
    int16_t h = display_.height();
    int lineHeight = display_.defaultLineHeight();
    display_.fillRect(0, 0, w, contentTop(), display_.bg);
    display_.fillRect(0, contentBottom(), w, h - contentBottom(), display_.bg);

    auto heading = StringFormat("%d/%d %s", static_cast<int>(page_) + 1, PAGE_COUNT, title.c_str());
    display_.drawSingleLineOfText(heading, MARGIN, 1);
    if (!detail.empty()) {
        display_.drawSingleLineOfText(detail, w - MARGIN - display_.getTextWidth(detail), 1);
    }

    std::string hint = "<> page";
    if (const char *adjustHint = this->adjustHint(); adjustHint != nullptr) {
        hint += StringFormat("  ^v %s", adjustHint);
    }
    hint += stereo_ ? "  o swap" : "  (no stereo)";
    display_.drawSingleLineOfText(hint, (w - display_.getTextWidth(hint)) / 2,
                                  h - lineHeight - BOTTOM_PADDING);
}

void StereoTestScreen::drawTextAtDepth(ScreenSide eye, const std::string &text, int x, int y,
                                       int disparity, Font *font, bool inverted) {
    display_.drawSingleLineOfText(text, x + DisplaySystem::eyeShift(eye, disparity), y, inverted,
                                  font);
}

void StereoTestScreen::fillHeart(int16_t cx, int16_t cy, int16_t size) {
    // Two circles on top of a downward-pointing triangle
    auto r = static_cast<int16_t>(std::max(1, size / 4));
    display_.fillCircle(cx - r, cy, r, display_.fg);
    display_.fillCircle(cx + r, cy, r, display_.fg);
    display_.fillTriangle(cx - 2 * r, cy + r / 3, cx + 2 * r, cy + r / 3, cx, cy + 2 * r + r / 2,
                          display_.fg);
}

// Each eye sees a shape only it should see, plus a shared fixation cross. Closing one eye at a time
// shows whether the left and right screens are wired the way the code expects.
void StereoTestScreen::drawEyeCheck(ScreenSide eye) {
    int16_t w = display_.width();
    int16_t top = contentTop();
    int16_t bottom = contentBottom();
    int16_t cy = (top + bottom) / 2;
    int16_t size = static_cast<int16_t>(std::min<int>(w / 5, (bottom - top) / 3));

    if (eye == ScreenSide::Left || eye == ScreenSide::Both) {
        display_.fillCircle(w / 4, cy, size / 2, display_.fg);
        drawTextAtDepth(eye, "LEFT", w / 4 - display_.getTextWidth("LEFT") / 2,
                        cy + size / 2 + MARGIN, 0);
    }
    if (eye == ScreenSide::Right || eye == ScreenSide::Both) {
        display_.fillRect(3 * w / 4 - size / 2, cy - size / 2, size, size, display_.fg);
        drawTextAtDepth(eye, "RIGHT", 3 * w / 4 - display_.getTextWidth("RIGHT") / 2,
                        cy + size / 2 + MARGIN, 0);
    }

    // Fixation cross seen by both eyes
    display_.drawLine(w / 2 - 6, cy, w / 2 + 6, cy, display_.fg);
    display_.drawLine(w / 2, cy - 6, w / 2, cy + 6, display_.fg);

    std::string note = "Close each eye in turn";
    drawTextAtDepth(eye, note, (w - display_.getTextWidth(note)) / 2, top + MARGIN, 0);
    if (swapEyes_) {
        std::string swapped = "(eyes swapped)";
        drawTextAtDepth(eye, swapped, (w - display_.getTextWidth(swapped)) / 2,
                        bottom - display_.defaultLineHeight() - MARGIN, 0);
    }
    drawHeader("Eye check", "");
}

// Bars at disparities from +4 to -4 (times the depth scale), framed by a box at zero disparity
void StereoTestScreen::drawDepthLadder(ScreenSide eye) {
    constexpr int rows = 9;
    int16_t w = display_.width();
    int16_t top = contentTop() + MARGIN;
    int16_t bottom = contentBottom() - MARGIN;
    auto rowHeight = static_cast<int16_t>((bottom - top) / rows);
    int16_t labelX = MARGIN * 3;
    auto barX = static_cast<int16_t>(w / 4);
    auto barWidth = static_cast<int16_t>(w / 2);

    display_.drawRect(MARGIN, top - 2, w - 2 * MARGIN, rowHeight * rows + 4, display_.fg);
    for (int row = 0; row < rows; row++) {
        int depth = 4 - row;
        int disparity = depth * depthScale_;
        auto y = static_cast<int16_t>(top + row * rowHeight);
        int16_t shift = DisplaySystem::eyeShift(eye, disparity);
        auto label = depth > 0 ? StringFormat("+%d", depth) : StringFormat("%d", depth);
        drawTextAtDepth(eye, label, labelX, y + (rowHeight - display_.defaultLineHeight()) / 2,
                        disparity);
        if (depth == 0) {
            display_.drawRect(barX + shift, y + 2, barWidth, rowHeight - 4, display_.fg);
        } else {
            display_.fillRect(barX + shift, y + 2, barWidth, rowHeight - 4, display_.fg);
        }
    }
    drawHeader("Depth ladder", StringFormat("x%d", depthScale_));
}

// A menu panel at the screen plane over a receding dotted background, with the selected item
// popping out
void StereoTestScreen::drawMenu(ScreenSide eye) {
    static const std::array<const char *, 4> items = {"Library", "Bookmarks", "Settings", "Sleep"};
    int16_t w = display_.width();
    int16_t top = contentTop();
    int16_t bottom = contentBottom();

    // Background dot grid, behind the screen
    int16_t backgroundShift = DisplaySystem::eyeShift(eye, -2);
    for (int16_t y = top + 2; y < bottom; y += 6) {
        for (int16_t x = -6; x < w + 6; x += 6) {
            int16_t dotX = x + backgroundShift;
            if (dotX >= 0 && dotX < w) {
                display_.fillRect(dotX, y, 1, 1, display_.fg);
            }
        }
    }

    // Menu panel at the screen plane
    auto itemHeight = static_cast<int16_t>(display_.defaultLineHeight() + 10);
    auto panelWidth = static_cast<int16_t>(w * 2 / 3);
    auto panelHeight = static_cast<int16_t>(itemHeight * items.size() + 12);
    auto panelX = static_cast<int16_t>((w - panelWidth) / 2);
    auto panelY = static_cast<int16_t>((top + bottom - panelHeight) / 2);
    display_.fillRoundRect(panelX, panelY, panelWidth, panelHeight, 6, display_.bg);
    display_.drawRoundRect(panelX, panelY, panelWidth, panelHeight, 6, display_.fg);

    for (size_t i = 0; i < items.size(); i++) {
        auto y = static_cast<int16_t>(panelY + 6 + i * itemHeight);
        auto textX = static_cast<int16_t>(panelX + 14);
        auto textY = static_cast<int16_t>(y + (itemHeight - display_.defaultLineHeight()) / 2);
        if (static_cast<int>(i) == menuSelection_) {
            int disparity = 2 * std::max(depthScale_, 1);
            int16_t shift = DisplaySystem::eyeShift(eye, disparity);
            // A depth-only highlight: inverting the row made the text look too thin
            display_.drawRoundRect(panelX + 6 + shift, y, panelWidth - 12, itemHeight, 4,
                                   display_.fg);
            display_.drawRoundRect(panelX + 7 + shift, y + 1, panelWidth - 14, itemHeight - 2, 3,
                                   display_.fg);
            drawTextAtDepth(eye, items[i], textX, textY, disparity);
        } else {
            drawTextAtDepth(eye, items[i], textX, textY, 0);
        }
    }
    drawHeader("Menu", "");
}

// Body text where *marked* words pop forward and _marked_ words sink back
void StereoTestScreen::drawText(ScreenSide eye) {
    static const char *const paragraph =
        "Words like *these* float in front of the page, while words like _these_ sink behind "
        "it. A whole *emphasized phrase* can lift together, and _a quiet aside_ can recede. "
        "Does one pixel of disparity read as depth, or only as blur?";
    int16_t w = display_.width();
    int16_t x0 = MARGIN * 3;
    auto maxX = static_cast<int16_t>(w - MARGIN * 3);
    Font *font = &fontFace1;
    int lineHeight = font->lineHeight() + 3;
    int spaceWidth = display_.getTextWidth("a a", font) - display_.getTextWidth("aa", font);

    std::string title = "Stereo Typography";
    drawTextAtDepth(eye, title, (w - display_.getTextWidth(title, &fontFace2)) / 2,
                    contentTop() + MARGIN, 2 * depthScale_, &fontFace2);

    int x = x0;
    int y = contentTop() + MARGIN + fontFace2.lineHeight() + 8;
    int depth = 0;
    std::istringstream words(paragraph);
    std::string word;
    while (words >> word) {
        bool closes = false;
        if (word.front() == '*' || word.front() == '_') {
            depth = word.front() == '*' ? 1 : -1;
            word.erase(0, 1);
        }
        auto marker = word.find_first_of("*_");
        if (marker != std::string::npos) {
            word.erase(marker, 1);
            closes = true;
        }

        int wordWidth = display_.getTextWidth(word, font);
        if (x + wordWidth > maxX && x > x0) {
            x = x0;
            y += lineHeight;
        }
        drawTextAtDepth(eye, word, x, y, depth * depthScale_, font);
        x += wordWidth + spaceWidth;
        if (closes) {
            depth = 0;
        }
    }
    drawHeader("Text", StringFormat("x%d", depthScale_));
}

// Random-dot stereogram: a circle in front of and a square behind a random-dot background. Each is
// invisible to a single eye.
void StereoTestScreen::drawRandomDots(ScreenSide eye) {
    constexpr int dotSize = 2;
    int16_t w = display_.width();
    int16_t top = contentTop();
    int16_t bottom = contentBottom();
    int cy = (top + bottom) / 2;
    int radius = (bottom - top) / 4;
    int circleCx = w / 3;
    int squareCx = 2 * w / 3;
    int squareHalf = radius * 4 / 5;

    int circleShift = DisplaySystem::eyeShift(eye, dotDisparity_);
    int squareShift = DisplaySystem::eyeShift(eye, -dotDisparity_);
    for (int y = top; y < bottom; y++) {
        for (int x = 0; x < w; x++) {
            bool dot = false;
            int cx = x - circleShift;
            int sx = x - squareShift;
            if ((cx - circleCx) * (cx - circleCx) + (y - cy) * (y - cy) <= radius * radius) {
                dot = dotAt(floorDiv(cx, dotSize), floorDiv(y, dotSize), 1);
            } else if (std::abs(sx - squareCx) <= squareHalf && std::abs(y - cy) <= squareHalf) {
                dot = dotAt(floorDiv(sx, dotSize), floorDiv(y, dotSize), 2);
            } else {
                dot = dotAt(floorDiv(x, dotSize), floorDiv(y, dotSize), 3);
            }
            if (dot) {
                display_.fillRect(static_cast<int16_t>(x), static_cast<int16_t>(y), 1, 1,
                                  display_.fg);
            }
        }
    }
    drawHeader("Random dots", StringFormat("%+d px", dotDisparity_));
}

// Wireframe cube with a small orbiting "comet", projected separately for each eye
void StereoTestScreen::drawCube(ScreenSide eye) {
    static const std::array<Vec3, 8> vertices = {{
        {-1, -1, -1},
        {1, -1, -1},
        {1, 1, -1},
        {-1, 1, -1},
        {-1, -1, 1},
        {1, -1, 1},
        {1, 1, 1},
        {-1, 1, 1},
    }};
    static const std::array<std::array<uint8_t, 2>, 12> edges = {{
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    }};
    constexpr float screenDistance = 5.0F;
    constexpr float tilt = 25.0F * DEGREES_TO_RADIANS;

    int16_t w = display_.width();
    int16_t top = contentTop();
    int16_t bottom = contentBottom();
    float cx = w / 2.0F;
    float cy = (top + bottom) / 2.0F;
    float focal = 0.22F * (bottom - top) * screenDistance;
    float eyeSeparation = 0.12F * static_cast<float>(depthScale_);
    float eyeX = 0;
    if (eye == ScreenSide::Left) {
        eyeX = -eyeSeparation / 2;
    } else if (eye == ScreenSide::Right) {
        eyeX = eyeSeparation / 2;
    }

    float yaw = static_cast<float>(cubeAngle_) * DEGREES_TO_RADIANS;
    auto transform = [yaw](Vec3 v) -> Vec3 {
        float x = v.x * std::cos(yaw) + v.z * std::sin(yaw);
        float z = -v.x * std::sin(yaw) + v.z * std::cos(yaw);
        float y = v.y * std::cos(tilt) - z * std::sin(tilt);
        z = v.y * std::sin(tilt) + z * std::cos(tilt);
        return {x, y, z + screenDistance};
    };
    // Points at SCREEN_DISTANCE have zero disparity; nearer points pop out
    auto project = [&](Vec3 v, int16_t *outX, int16_t *outY) {
        *outX = static_cast<int16_t>(
            std::lround(cx + focal * (v.x - eyeX) / v.z + focal * eyeX / screenDistance));
        *outY = static_cast<int16_t>(std::lround(cy - focal * v.y / v.z));
    };

    std::array<std::array<int16_t, 2>, 8> points{};
    for (size_t i = 0; i < vertices.size(); i++) {
        project(transform(vertices[i]), points[i].data(), &points[i][1]);
    }
    for (const auto &edge : edges) {
        display_.drawLine(points[edge[0]][0], points[edge[0]][1], points[edge[1]][0],
                          points[edge[1]][1], display_.fg);
    }

    float orbit = static_cast<float>(cubeAngle_) * 2.0F * DEGREES_TO_RADIANS;
    Vec3 comet = transform({1.9F * std::cos(orbit), 0.4F, 1.9F * std::sin(orbit)});
    int16_t cometX = 0;
    int16_t cometY = 0;
    project(comet, &cometX, &cometY);
    display_.fillCircle(cometX, cometY, 3, display_.fg);

    drawHeader("Cube", StringFormat("%d deg x%d", cubeAngle_, depthScale_));
}

// Playing card with the rank and suit popping out and the face sunken into the card
void StereoTestScreen::drawCard(ScreenSide eye) {
    int16_t w = display_.width();
    int16_t top = contentTop() + MARGIN;
    int16_t bottom = contentBottom() - MARGIN;
    auto cardHeight = static_cast<int16_t>(bottom - top);
    auto cardWidth = static_cast<int16_t>(cardHeight * 5 / 7);
    auto cardX = static_cast<int16_t>((w - cardWidth) / 2);
    int16_t cardY = top;
    int raised = 2 * depthScale_;
    int sunken = -2 * depthScale_;

    display_.drawRoundRect(cardX, cardY, cardWidth, cardHeight, 8, display_.fg);
    display_.drawRoundRect(cardX + 1, cardY + 1, cardWidth - 2, cardHeight - 2, 7, display_.fg);

    // Sunken face
    int16_t inset = 24;
    int16_t faceShift = DisplaySystem::eyeShift(eye, sunken);
    display_.drawRect(cardX + inset + faceShift, cardY + inset, cardWidth - 2 * inset,
                      cardHeight - 2 * inset, display_.fg);
    for (int y = cardY + inset + 4; y < cardY + cardHeight - inset - 2; y += 4) {
        for (int x = cardX + inset + 4; x < cardX + cardWidth - inset - 2; x += 4) {
            display_.fillRect(x + faceShift, y, 1, 1, display_.fg);
        }
    }
    auto heartSize = static_cast<int16_t>(cardWidth / 3);
    auto faceCx = static_cast<int16_t>(cardX + cardWidth / 2 + faceShift);
    auto faceCy = static_cast<int16_t>(cardY + cardHeight / 2 - heartSize / 4);
    display_.fillRect(faceCx - heartSize / 2 - 3, faceCy - heartSize / 3 - 3, heartSize + 6,
                      heartSize + 6, display_.bg);
    fillHeart(faceCx, faceCy, heartSize);

    // Raised corner indices
    int16_t raisedShift = DisplaySystem::eyeShift(eye, raised);
    auto indexX = static_cast<int16_t>(cardX + 7);
    auto indexY = static_cast<int16_t>(cardY + 5);
    drawTextAtDepth(eye, "A", indexX, indexY, raised, &fontFace2);
    fillHeart(static_cast<int16_t>(indexX + 5 + raisedShift),
              static_cast<int16_t>(indexY + fontFace2.lineHeight() + 1), 10);
    int16_t cornerX =
        static_cast<int16_t>(cardX + cardWidth - 7 - display_.getTextWidth("A", &fontFace2));
    auto cornerY = static_cast<int16_t>(cardY + cardHeight - 5 - fontFace2.lineHeight());
    drawTextAtDepth(eye, "A", cornerX, cornerY, raised, &fontFace2);
    fillHeart(static_cast<int16_t>(cornerX + 5 + raisedShift), static_cast<int16_t>(cornerY - 9),
              10);

    drawHeader("Card", StringFormat("x%d", depthScale_));
}
