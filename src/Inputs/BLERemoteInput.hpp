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

    void startRemote(int receivePinInput) override;
    void startRemote() override;
    auto checkPress() -> int override;
    auto checkRelease() -> int override;

protected:
    int wasCodeProcessed_ = 1;
    RemoteCommand bleCode_ = RemoteCommand::NONE;

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLERemoteInput *enclosed;

        CharacteristicCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                log_d("New value: %.2x", value[0]);
                enclosed->bleCode_ = (RemoteCommand)value.data()[0];
                enclosed->wasCodeProcessed_ = 0;
            }
        }
    };
};

#endif