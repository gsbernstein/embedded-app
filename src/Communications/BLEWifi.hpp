#ifndef BLEWifi_H
#define BLEWifi_H

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <improv.h>

#include "Communications/BLEManager.hpp"
#include "sindarin-debug.h"

enum BLEWifiState : uint8_t {
    // Waiting for credentials
    STATE_WAITING = 0x00,
    // Received WiFi credentials and ready to start connecting
    STATE_READY_TO_CONNECT = 0x01,
    // Attempting to connect to WiFi
    STATE_CONNECTING = 0x02,
    // Connected to WiFi
    STATE_CONNECTED = 0x03,
};

class BLEWifi {
public:
    void startService();
    void loop();

protected:
    uint32_t last_state_change_millis;
    BLECharacteristic *current_state_characteristic;
    BLECharacteristic *error_state_characteristic;
    BLECharacteristic *rpc_result_characteristic;
    BLEService *service;
    BLEWifiState state;
    improv::ImprovCommand lastCommand;

    void setState(BLEWifiState state);
    void setError(improv::Error error, bool notify = true);
    void doneConnecting();
    void startOTA();

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLEWifi *enclosed;

        CharacteristicCallbacks(BLEWifi *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            log_d("onWrite for Characteristic %s", characteristic->getUUID().toString().c_str());
            if (characteristic->getUUID().equals(BLEUUID(improv::RPC_COMMAND_UUID))) {
                std::string value = characteristic->getValue();
                improv::ImprovCommand command =
                    improv::parse_improv_data((uint8_t *)value.c_str(), value.length());
                switch (command.command) {
                case improv::BAD_CHECKSUM: {
                    log_e("Error: Bad Checksum from Improv message");
                    break;
                }
                case improv::WIFI_SETTINGS: {
                    this->enclosed->lastCommand = command;
                    this->enclosed->setState(STATE_READY_TO_CONNECT);
                    break;
                }
                default: {
                    log_e("Error: Unhandled Improv message");
                }
                }
            } else {
                log_e("Error: Unhandled characteristic in BLEWifi");
            }
        }
    };
};

#endif
