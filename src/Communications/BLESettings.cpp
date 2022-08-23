#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

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

void BLESettings::writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic) {
    wifi_ap_record_t info;
    if (!esp_wifi_sta_get_ap_info(&info)) {
        characteristic->setValue(info.ssid);
        log_i("Connected to wifi, sharing ssid: %s", info.ssid);
    } else {
        characteristic->setValue("");
        log_i("Not connected to wifi, could not share ssid");
    }
}

#endif
