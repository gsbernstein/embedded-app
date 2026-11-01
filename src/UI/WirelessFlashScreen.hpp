#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Communications/WirelessFlashServer.hpp"
#include "Misc/CancelFlag.hpp"
#include "UI/Components/ProgressBarView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

// Debug-only screen that keeps WirelessFlashManager listening and shows push progress.
class WirelessFlashScreen : public Screen, public EinkView {
public:
    explicit WirelessFlashScreen(DisplaySystem &display);
    ~WirelessFlashScreen() override;

    void begin();

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

private:
    std::shared_ptr<CancelFlag> cancel_;
    std::function<void()> resumeBLE_;
    DisplaySystem::UpdateType nextUpdate_ = DisplaySystem::UpdateType::FULL;

    SingleLineTextView titleView_;
    SingleLineTextView addressView_;
    SingleLineTextView statusView_;
    ProgressBarView progressBarView_;
    SingleLineTextView cancelView_;

    void setStatus(const std::string &status);
    void handleEvent(const WirelessFlashEvent &event);
    void shutdown();
    void exit(const std::string &message);
};
