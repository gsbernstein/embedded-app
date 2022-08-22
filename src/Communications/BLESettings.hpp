#pragma once

#include <NimBLEDevice.h>
#include <Preferences.h>

#include "BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/PocketAPI.hpp"
#include "Storage/PreferencesStore.hpp"
#include "esp_wifi.h"
#include "sindarin-debug.h"

class BLESettings : BLECharacteristicProvider {

public:
    BLESettings(PreferencesStore *prefs);
    void configureCharacteristics(BLEService *service) override;

protected:
    PreferencesStore *prefs_;
    BLECharacteristic *settingsCharacteristic_;

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLESettings *enclosed;

        CharacteristicCallbacks(BLESettings *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() == 0) {
                return;
            }

            // If value contains an '=' character, it's setting a value to a key.
            // Otherwise, get and write the key's value back to the characteristic.
            size_t equalsIndex = value.find('=');
            if (equalsIndex == std::string::npos) {
                if (value == "wifi-ssid") {
                    wifi_ap_record_t info;
                    if (!esp_wifi_sta_get_ap_info(&info)) {
                        characteristic->setValue(info.ssid);
                        log_i("Connected to wifi, sharing ssid: %s", info.ssid);
                    } else {
                        characteristic->setValue("");
                        log_i("Not connected to wifi, could not share ssid");
                    }
                } else {
                    log_w("GATT client asking to read unknown setting: %s", value.c_str());
                }
            } else {
                std::string settingKey = value.substr(0, equalsIndex);
                std::string settingValue = value.substr(equalsIndex + 1);
                log_i("GATT client writing setting: %s=%s", settingKey.c_str(),
                      settingValue.c_str());

                if (settingKey == "pocket-token") {
                    this->enclosed->prefs_->pocketApiToken.set(settingValue);
                } else if (settingKey == "cloud-token") {
                    this->enclosed->prefs_->cloudToken.set(settingValue);
                } else {
                    log_w("GATT client asking to write unknown setting: %s=%s", settingKey.c_str(),
                          settingValue.c_str());
                }
            }
        }
    };
};
