#include "Misc/Power.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(RawRenderer &renderer, PreferencesStore &prefs, Power &power)
        : renderer_(renderer), prefs_(prefs), power_(power),
          xOffset(prefs.localXOffset.get().value()), yOffset(prefs.localYOffset.get().value()) {}

    void render() override;
    void processCommand(RemoteCommand command) override;

    int xOffset;
    int yOffset;

protected:
    RawRenderer &renderer_;
    PreferencesStore &prefs_;
    Power &power_;
};
