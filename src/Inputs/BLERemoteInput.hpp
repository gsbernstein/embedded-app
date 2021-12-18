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

    void start_remote(int receive_pin_input) override;
    void start_remote() override;
    auto check_press() -> int override;
    auto check_release() -> int override;

protected:
    int was_code_processed = 1;
    RemoteCommand ble_code = RemoteCommand::NONE;

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLERemoteInput *enclosed;

        CharacteristicCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                log_d("New value: %.2x", value[0]);
                enclosed->ble_code = (RemoteCommand)value.data()[0];
                enclosed->was_code_processed = 0;
            }
        }
    };
};

#endif