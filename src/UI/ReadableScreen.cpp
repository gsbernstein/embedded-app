#include "UI/ReadableScreen.hpp"

#include "sindarin-debug.h"

void ReadableScreen::render() {
    if (fileRenderer_ != nullptr && fileRenderer_->getReadMode() == PAGE_READ) {
        readable_.placeScreed = fileRenderer_->renderContinueScreed();
    }

    if (!needsRerender) {
        return;
    }
    log_d("Rendering some text in READ MODE");
    newPlace_ = fileRenderer_->renderPageWithBox(readable_.place);
    log_d("%d is the new place in the text", newPlace_);
    needsRerender = false;
}

void ReadableScreen::processCommand(RemoteCommand command) {
    auto back = [this]() {
        if (fileRenderer_ != nullptr && fileRenderer_->getReadMode() == SCREED_READ) {
            readable_.placePrev = readable_.place;
            readable_.place = readable_.placeScreed;
            prefs_.readablePlace.set(readable_.id, readable_.place);
            prefs_.readablePreviousPlace.set(readable_.id, readable_.placePrev);
        }

        if (backCallback != nullptr) {
            backCallback();
        }
        needsRerender = true;
    };

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        readable_.place = fileRenderer_->getPreviousPlace();
        prefs_.readablePreviousPlace.set(readable_.id, readable_.place);
        needsRerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        readable_.placePrev = readable_.place;
        readable_.place = newPlace_;
        prefs_.readablePlace.set(readable_.id, readable_.place);
        prefs_.readablePreviousPlace.set(readable_.id, readable_.placePrev);
        needsRerender = true;
        break;
    }
    case RemoteCommand::PREV:
        if (fileRenderer_->getReadMode() == SCREED_READ) {
            fileRenderer_->increaseSpeed();
        }
        back();
        break;
    case RemoteCommand::NEXT:
        if (fileRenderer_->getReadMode() == SCREED_READ) {
            fileRenderer_->decreaseSpeed();
        }
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        if (fileRenderer_->getReadMode() == SCREED_READ) {
            readable_.placePrev = readable_.place;
            readable_.place = readable_.placeScreed;
            prefs_.readablePlace.set(readable_.id, readable_.place);
            prefs_.readablePreviousPlace.set(readable_.id, readable_.placePrev);
        }
        fileRenderer_->flipReadMode();
        needsRerender = true;
        break;
    default:
        break;
    }
}