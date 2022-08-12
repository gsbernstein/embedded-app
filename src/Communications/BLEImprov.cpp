#if USE_BLE_SERVER_IMPROV

#include "BLEImprov.hpp"

#include <ArduinoOTA.h>

#include "config.h"
#include "esp_wifi.h"
#include "sindarin-debug.h"

void BLEImprov::configureCharacteristics(BLEService *service) {
    auto *characteristicCallbacks = new CharacteristicCallbacks(this);

    BLECharacteristic *capabilitiesCharacteristic =
        service->createCharacteristic(BLEUUID(improv::CAPABILITIES_UUID), NIMBLE_PROPERTY::READ);
    int supportsIdentify = 0;
    capabilitiesCharacteristic->setValue(supportsIdentify);

    BLECharacteristic *currentStateCharacteristic = service->createCharacteristic(
        BLEUUID(improv::STATUS_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int state = improv::STATE_AUTHORIZED;
    currentStateCharacteristic->setValue(state);
    this->currentStateCharacteristic_ = currentStateCharacteristic;

    BLECharacteristic *errorStateCharacteristic = service->createCharacteristic(
        BLEUUID(improv::ERROR_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int noError = improv::ERROR_NONE;
    errorStateCharacteristic->setValue(noError);
    this->errorStateCharacteristic_ = errorStateCharacteristic;

    BLECharacteristic *rpcCommandCharacteristic =
        service->createCharacteristic(BLEUUID(improv::RPC_COMMAND_UUID), NIMBLE_PROPERTY::WRITE);
    rpcCommandCharacteristic->setCallbacks(characteristicCallbacks);

    BLECharacteristic *rpcResultCharacteristic = service->createCharacteristic(
        BLEUUID(improv::RPC_RESULT_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    this->rpcResultCharacteristic_ = rpcResultCharacteristic;
}

auto BLEImprov::configureService(BLEServer *server) -> BLEService * {
    service_ = server->createService(BLEUUID(improv::SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service_->getUUID());
    configureCharacteristics(service_);
    return service_;
}

auto BLEImprov::start() -> bool { return service_->start(); }

void BLEImprov::setState(BLEImprovState state) {
    if (this->state_ == state) {
        return;
    }
    this->state_ = state;
    // Translate BLEImprov state to the most relevant Improv state
    int improvState;
    if (state == STATE_WAITING) {
        improvState = improv::STATE_AUTHORIZED;
    } else if (state == STATE_READY_TO_CONNECT || state == STATE_CONNECTING) {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONING;
    } else if (state == STATE_SCANNING) {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_SCANNING;
    } else {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONED;
    }
    log_d("Switched improv state to %d (internal state = %d)", improvState, state);
#if !defined(DISABLE_IMPROV)
    this->currentStateCharacteristic_->setValue(improvState);
    this->currentStateCharacteristic_->notify();
#endif
    this->lastStateChangeMillis_ = millis();
}

void BLEImprov::setError(improv::Error error, bool notify) {
#if !defined(DISABLE_IMPROV)
    int errorInt = error;
    this->errorStateCharacteristic_->setValue(errorInt);
    if (notify) {
        this->errorStateCharacteristic_->notify();
    }
#endif
}

void BLEImprov::doneConnecting() {
    this->setState(STATE_CONNECTED);
    log_i("Connected, IP address: %s", WiFi.localIP().toString().c_str());
    SHOW_HEAP();
}

void BLEImprov::loop() {
    if (this->state_ == STATE_WAITING) {
        if (WiFi.isConnected()) {
            this->doneConnecting();
        }
    } else if (this->state_ == STATE_READY_TO_CONNECT) {
        improv::ImprovCommand command = this->lastCommand_;
        log_d("Attempting connection to: %s Password: %s (WiFi status = %d)", command.ssid.c_str(),
              command.password.c_str(), WiFi.status());
        WiFi.setHostname(HOSTNAME);
        WiFi.begin(command.ssid.c_str(), command.password.c_str());
        ArduinoOTA.setHostname(HOSTNAME);
        ArduinoOTA.begin();
        this->setState(STATE_CONNECTING);
        return;
    } else if (this->state_ == STATE_CONNECTING) {
        uint32_t now = millis();
        if (WiFi.status() == WL_CONNECTED) {
            std::string ip = WiFi.localIP().toString().c_str();
            std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {ip});
            log_d("Reporting RPC result to client");
            this->rpcResultCharacteristic_->setValue(response.data(), response.size());
            this->rpcResultCharacteristic_->notify();
            this->doneConnecting();
        } else if (now - this->lastStateChangeMillis_ > 30000) {
            log_w("Timed out connecting to WiFi %d", WiFi.status());
            this->setState(STATE_WAITING);
            this->setError(improv::ERROR_UNABLE_TO_CONNECT);
            WiFi.disconnect();
        }
    } else if (this->state_ == STATE_READY_TO_SCAN) {
        bool blockingScan = false;
        bool passiveScan = false;
        uint32_t scanTimePerChannelMillis = 300;
        this->lastAPsent_ = 0;

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);

        // Initialize the configuration struct for passive or active scanning
        wifi_scan_config_t scanConfig;
        scanConfig.ssid = 0;
        scanConfig.bssid = 0;
        // scanConfig.channel = channel;
        // scanConfig.show_hidden = show_hidden;
        if (passiveScan) {
            scanConfig.scan_type = WIFI_SCAN_TYPE_PASSIVE;
            scanConfig.scan_time.passive = scanTimePerChannelMillis;
        } else {
            scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;
            scanConfig.scan_time.active.min = 100;
            scanConfig.scan_time.active.max = scanTimePerChannelMillis;
        }
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        esp_wifi_scan_start(&scanConfig, blockingScan);

        this->setState(STATE_SCANNING);
    } else if (this->state_ == STATE_SCANNING) {
        uint32_t now = millis();
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (now - this->lastStateChangeMillis_ > 10000) {
            log_w("Timed out scanning for WiFi APs, %d found", apCount);
            this->setState(STATE_SCAN_COMPLETE);
        } else if (apCount > this->lastAPsent_) {
            // Check if new results since last, send them

            wifi_ap_record_t *ap = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t));
            esp_wifi_scan_get_ap_records(&this->lastAPsent_, ap);

            std::string ssid = String((char *)ap->ssid).c_str();
            std::string rssi = String((char)ap->rssi).c_str();
            std::string auth = String((char)(ap->authmode != WIFI_AUTH_OPEN)).c_str();
            std::vector<uint8_t> response =
                improv::build_rpc_response(improv::GET_WIFI_NETWORKS, {ssid, rssi, auth});
            log_d("Sending latest AP scan result to client: %s %s %s", ssid.c_str(), rssi.c_str(),
                  auth.c_str());
            this->rpcResultCharacteristic_->setValue(response.data(), response.size());
            this->rpcResultCharacteristic_->notify();

            this->lastAPsent_ += 1;
        }
    } else if (this->state_ == STATE_SCAN_COMPLETE) {
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        log_w("Wifi scan complete, %d APs found", apCount);
        this->setState(STATE_WAITING);
    } else if (this->state_ == STATE_CONNECTED) {
        if (!WiFi.isConnected()) {
            this->setState(STATE_WAITING);
        }
    }
}

#endif
