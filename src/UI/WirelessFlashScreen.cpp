#include "WirelessFlashScreen.hpp"

#if CONFIG_DEV_WIRELESS_FLASH

#include <StringUtil/StringUtil.hpp>

#if USE_BLE_SERVER
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/WirelessFlashManager.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/RateLimiter.hpp"
#include "Version.hpp"

WirelessFlashScreen::WirelessFlashScreen(DisplaySystem &display)
    : EinkView(display), cancel_(std::make_shared<CancelFlag>()),
      titleView_(display, "Wireless Flash", &fontFace0), addressView_(display, "", &fontFace0),
      statusView_(display, "", &fontFace0), progressBarView_(display),
      cancelView_(display, "Press \ue000 to cancel", &fontFace0) {
    progressBarView_.setHidden(true);
}

WirelessFlashScreen::~WirelessFlashScreen() { shutdown(); }

auto WirelessFlashScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setSublayoutFrame(
        SolRect::fromSize(size), LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, SolSize size) -> SolSize {
            int y = 0;
            for (SingleLineTextView *view : {&titleView_, &addressView_, &statusView_}) {
                auto rect = context.setViewFrame(*view, {{0, y}, size},
                                                 LayoutOptionsSizeToFit |
                                                     LayoutOptionsAlignCenterHorizontal);
                y += rect.size.height + 10;
            }
            auto rect = context.setViewFrame(progressBarView_, {{0, y}, size},
                                             LayoutOptionsSizeToFitVertical);
            y += rect.size.height;
            return {size.width, y};
        });

    context.setViewFrame(cancelView_, SolRect::fromSize(size).inset(10),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void WirelessFlashScreen::drawInRect(SolRect rect) {
    titleView_.draw();
    addressView_.draw();
    statusView_.draw();
    if (!progressBarView_.isHidden()) {
        progressBarView_.draw();
    }
    cancelView_.draw();
}

void WirelessFlashScreen::render(DisplaySystem &display) {
    setFrame(SolRect::fromSize({display.width(), display.height()}).inset(42, 10));
    draw();
    display.updateDisplay(nextUpdate_);
    nextUpdate_ = DisplaySystem::UpdateType::FAST_INDICATOR;
}

void WirelessFlashScreen::setStatus(const std::string &status) {
    if (statusView_.getText() != status) {
        statusView_.setText(status);
        setNeedsLayout();
        nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
    }
}

void WirelessFlashScreen::processCommand(RemoteCommand command) {
    if (command == RemoteCommand::ENTER || command == RemoteCommand::BACK) {
        exit("Cancelled");
    }
}

void WirelessFlashScreen::shutdown() {
    CancelFlag::cancel(cancel_);
    auto &manager = WirelessFlashManager::getInstance();
    manager.setEventCallback(nullptr);
    manager.stop();
    if (resumeBLE_) {
        resumeBLE_();
        resumeBLE_ = nullptr;
    }
}

void WirelessFlashScreen::exit(const std::string &message) {
    shutdown();
    setStatus(message);
    render(display_);
    vTaskDelay(pdMS_TO_TICKS(2000));
    back();
}

void WirelessFlashScreen::begin() {
#if USE_BLE_SERVER
    // Stop BLE to avoid radio coexistence slowing down the transfer.
    auto bleManager = BLEManager::getInstance();
    bleManager->stopAdvertising();
    bleManager->disconnectAllClients();
    resumeBLE_ = [bleManager] { bleManager->startAdvertisingIfNecessary(); };
#endif

    setStatus("Connecting WiFi...");
    render(display_);

    auto cancel = cancel_;
    auto rateLimiter = std::make_shared<RateLimiter>(1, 20, 5);
    auto &manager = WirelessFlashManager::getInstance();
    manager.setEventCallback([this, cancel, rateLimiter](const WirelessFlashEvent &event) {
        if (event.state == WirelessFlashEvent::State::Receiving && event.percentage > 0 &&
            event.percentage < 100 && !rateLimiter->shouldExecute(event.percentage)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, cancel, event]() {
            if (!CancelFlag::isCancelled(cancel)) {
                handleEvent(event);
            }
        });
    });

    std::string error;
    auto address = manager.start(0, &error);
    if (address.empty()) {
        exit(error);
        return;
    }

    addressView_.setText(address);
    setStatus(StringFormat("Ready (v%s)", SemanticVersionString()));
    render(display_);
}

void WirelessFlashScreen::handleEvent(const WirelessFlashEvent &event) {
    switch (event.state) {
    case WirelessFlashEvent::State::Receiving:
        if (progressBarView_.isHidden()) {
            progressBarView_.setHidden(false);
            nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
        }
        progressBarView_.setProgress(event.percentage);
        setStatus("Receiving firmware...");
        break;
    case WirelessFlashEvent::State::Failed:
        progressBarView_.setHidden(true);
        setStatus(event.message);
        nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
        break;
    case WirelessFlashEvent::State::Installed:
        progressBarView_.setProgress(100);
        setStatus("Installed! Restarting...");
        break;
    }
    render(display_);
}

#endif
