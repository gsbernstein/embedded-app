#ifndef BLERemoteInput_H
#define BLERemoteInput_H

#include <NimBLEDevice.h>

#include "Communications/BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "sindarin-debug.h"

class BLERemoteInput : public RemoteInput, public BLECharacteristicProvider {

public:
    void configureCharacteristics(BLEService *service) override;

protected:
    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLERemoteInput *enclosed;

        CharacteristicCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                log_d("Page turner button: %.2x %.2x", value[0], value[1]);
                uint16_t processed = (value[0] << 8) | (value[1]);
                RemoteCommand command = DecodePageTurner(processed);

                if (command != RemoteCommand::NONE) {
                    this->enclosed->callback_(command);
                }
            }
        }
    };
};

#endif
