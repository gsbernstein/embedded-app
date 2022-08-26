#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <utility>

#include "sindarin-debug.h"

#define SETTINGS_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-ef6773326268"

BLESettings::BLESettings(PreferencesStore *prefs) { this->prefs_ = prefs; }

void BLESettings::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(SETTINGS_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    this->settingsCharacteristic_ = characteristic;
};

void BLESettings::writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic,
                                                std::string key) {
    wifi_ap_record_t info;

    if (!esp_wifi_sta_get_ap_info(&info)) {
        key.append("=");
        key.append(reinterpret_cast<char *>(info.ssid));
        characteristic->setValue(key);

        log_i("Connected to wifi, sharing ssid: %s %d/%d", key.c_str(), key.length(),
              sizeof(key.c_str()));
    } else {
        key.append("=");
        characteristic->setValue(key);
        log_i("Not connected to wifi, share blank ssid: %s", key.c_str());
    }

    characteristic->notify();
}

#endif
