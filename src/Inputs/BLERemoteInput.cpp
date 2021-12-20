
#include "BLERemoteInput.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

#define CHARACTERISTIC_UUID "ed0c3501-a3cb-499f-93ad-2acbaa971d63"

void BLERemoteInput::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    characteristic->setValue("Hello World");
}

void BLERemoteInput::start_remote(int receive_pin_input) {
    log_e("FAIL DUE TO BLE CONSTUCTOR NOT NEEDING THIS ONE");
    assert(0);
}

void BLERemoteInput::start_remote() {}

auto BLERemoteInput::check_press() -> int { return 0; }

auto BLERemoteInput::check_release() -> int {
    int found_release = 0;
    if (!was_code_processed) {
        was_code_processed = 1;
        last_code = (RemoteCommand)ble_code;
        found_release = 1;
        log_d("Got a BLE code: %s", codeToString(last_code));
    }
    return found_release;
}
