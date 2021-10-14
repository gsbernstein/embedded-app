#ifndef BLERemoteInput_H
#define BLERemoteInput_H

#include <NimBLEDevice.h>

#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "sindarin-debug.h"

class BLERemoteInput : public RemoteInput {

public:
    void start_remote(int receive_pin_input);
    void start_remote();
    int check_press();
    int check_release();

protected:
    bool wasDeviceConnected;
    bool deviceConnected;
    int was_code_processed;
    int ble_code;

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        BLERemoteInput *enclosed;

        void onConnect(BLEServer *server) {
            DEBUG_LN("BLE client connected to our server!");
            enclosed->deviceConnected = true;
        }

        void onDisconnect(BLEServer *server) {
            DEBUG_LN("BLE client disconnected from our server!");
            enclosed->deviceConnected = false;
        }
    };

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLERemoteInput *enclosed;

        CharacteristicCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                DEBUG_LN("*********");
                DEBUG_F("New value: %.2x \n", value[0]);
                DEBUG_LN("*********");
                enclosed->ble_code = value.data()[0];
                enclosed->was_code_processed = 0;
            }
        }
    };
};

#endif