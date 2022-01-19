#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"

class BitmapScreen : public Screen {
public:
    BitmapScreen(Renderer &renderer, const uint8_t *bitmap)
        : renderer_(renderer), bitmap_(bitmap) {}
    ~BitmapScreen() override = default;

    void render() override;
    void processCommand(RemoteCommand command) override;

protected:
    Renderer &renderer_;
    const uint8_t *bitmap_;
};
