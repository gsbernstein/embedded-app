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

void BLESettings::writeDeviceSerialToCharacteristic(BLECharacteristic *characteristic,
                                                    std::string key) {
    uint8_t serial_number[8] = {0};
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, &serial_number,
                                              sizeof(serial_number) * 8));
    log_i("Read efuse serial number, sending over ble: %02x%02x%02x%02x%02x%02x%02x%02x",
          serial_number[0], serial_number[1], serial_number[2], serial_number[3], serial_number[4],
          serial_number[5], serial_number[6], serial_number[7]);

    std::string value = key + "=" + std::string((char *)serial_number, sizeof(serial_number));

    characteristic->setValue(value);
    characteristic->notify();
}

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
