#pragma once

#include "UI/Screen.hpp"

/*!
Screen that can contain other screens and switch between them.
*/
class ContainerScreen : public Screen {
public:
    void setCurrentScreen(Screen &screen);
    void render() override;
    void processCommand(RemoteCommand command) override;

protected:
    Screen *currentScreen_;
};
