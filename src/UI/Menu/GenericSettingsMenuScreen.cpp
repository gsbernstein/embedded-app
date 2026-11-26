#include "GenericSettingsMenuScreen.hpp"

#include <Renderer/Renderer.hpp>

#include "UI/Menu/MenuItemCheckable.hpp"
#include "UI/RootUI.hpp"

auto GenericSettingsMenuScreen::stereoDepthMenu(DisplaySystem &display, PreferencesStore &prefs,
                                                const std::string &backTitle, bool menuDepth)
    -> std::shared_ptr<GenericSettingsMenuScreen> {
    auto menu = std::make_shared<GenericSettingsMenuScreen>(display, backTitle);
    std::weak_ptr<GenericSettingsMenuScreen> weakMenu = menu;
    int minDepth = menuDepth ? 0 : -DisplaySystem::MAX_STEREO_DISPARITY;
    int maxDepth =
        menuDepth ? DisplaySystem::MAX_MENU_DISPARITY : DisplaySystem::MAX_STEREO_DISPARITY;
    int step = menuDepth ? 1 : DisplaySystem::STEREO_DISPARITY_STEP;
    for (int depth = minDepth; depth <= maxDepth; depth += step) {
        auto title = depth == 0
                         ? std::string("0 px (off)")
                         : StringFormat("%+d px (%s)", depth, depth > 0 ? "forward" : "back");
        menu->addMenuItem(std::make_shared<MenuItemCheckable>(
            title,
            [&display, &prefs, weakMenu, menuDepth, depth]() {
            if (!menuDepth && !display.setStereoDisparity(depth)) {
                RootUI::getInstance().showTemporaryMessage("Could not change stereo depth.");
                return;
            }
            (menuDepth ? prefs.menuDisparity : prefs.stereoDisparity).set(depth);
            if (auto screen = weakMenu.lock()) {
                screen->setFirstLoad();
                screen->setNeedsRerender();
            }
            },
            [&display, &prefs, menuDepth, depth]() {
            return (menuDepth ? prefs.menuDisparity.get().value_or(1)
                              : display.stereoDisparity()) == depth;
        }));
    }
    int depth = menuDepth ? prefs.menuDisparity.get().value_or(1) : display.stereoDisparity();
    menu->setSelectedIndex((std::clamp(depth, minDepth, maxDepth) - minDepth) / step);
    return menu;
}

GenericSettingsMenuScreen::GenericSettingsMenuScreen(DisplaySystem &display,
                                                     const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 12;
}

void GenericSettingsMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void GenericSettingsMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        setNeedsRerender();
        back();
        return;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto GenericSettingsMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto GenericSettingsMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    // Layout back label
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    context.setViewFrame(menuView_, SolRect::fromSize(size).inset(size.width / 7, 0),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void GenericSettingsMenuScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    menuView_.draw();
}

void GenericSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void GenericSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
