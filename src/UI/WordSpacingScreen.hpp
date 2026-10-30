#ifndef WORD_SPACING_SCREEN_HPP
#define WORD_SPACING_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

class WordSpacingScreen : public Screen, public EinkView {
public:
    WordSpacingScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);
    WordSpacingScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry,
                      Renderer &renderer, std::shared_ptr<Place> place);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    std::shared_ptr<Place> place_{nullptr};
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    int wordSpacing_{};

    Renderer &rendererRef_;

    BoxView outerBoxView_;
    BoxView backgroundBoxView_;

    MultiLineTextView explanationView_;

    auto setupInstructions() -> void;
};

#endif
