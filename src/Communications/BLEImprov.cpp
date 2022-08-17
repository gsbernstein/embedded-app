#if USE_BLE_SERVER_IMPROV

#include "BLEImprov.hpp"

#include <ArduinoOTA.h>

#include "config.h"
#include "esp_wifi.h"
#include "sindarin-debug.h"

// Time to wait for WiFi scanning to complete. Should not trigger if the event handler fires a
// `WIFI_EVENT_SCAN_DONE` event.
#define WIFI_SCANNING_TIMEOUT_MS 10000

// Max number of WiFi APs to scan for.
#define WIFI_SCANNING_MAX_AP_RECORDS 20

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

// This event handler is unused due to `esp_event_handler_register` not able to register a class
// method
void BLEImprov::wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                   void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        log_d("WIFI_EVENT_SCAN_DONE");
        BLEImprov::setState(STATE_SCAN_COMPLETE);
    }
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data) {
    log_d("WIFI_EVENT_SCAN_DONE? %d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        log_d("WIFI_EVENT_SCAN_DONE!");
        // BLEImprov::setState(STATE_SCAN_COMPLETE);
    }
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
        this->lastAPsent_ = 0;

        // Initialize Wifi in case it has not yet started
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);

        // esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &this->wifi_event_handler,
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

        // Initialize the configuration struct for active scanning
        wifi_scan_config_t scanConfig;
        scanConfig.ssid = 0;
        scanConfig.bssid = 0;
        scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        esp_wifi_scan_start(&scanConfig, blockingScan);

        this->setState(STATE_SCANNING);
    } else if (this->state_ == STATE_SCANNING) {
        uint32_t now = millis();
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);

        if (apCount > this->lastAPsent_) {
            this->lastAPsent_ = apCount;
            log_d("Wifi scanning... %d APs found so far", apCount);
        }

        if (now - this->lastStateChangeMillis_ > WIFI_SCANNING_TIMEOUT_MS) {
            log_w("Timed out scanning for WiFi APs, %d found", apCount);
            this->setState(STATE_SCAN_COMPLETE);
        }
    } else if (this->state_ == STATE_SCAN_COMPLETE) {
        uint16_t maxRecords = WIFI_SCANNING_MAX_AP_RECORDS;
        wifi_ap_record_t apRecords[WIFI_SCANNING_MAX_AP_RECORDS];
        esp_wifi_scan_get_ap_records(&maxRecords, apRecords);
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        log_i("Wifi scan complete, %d APs found", apCount);

        for (int i = 0; (i < apCount) && (i < maxRecords); i++) {
            wifi_ap_record_t ap = apRecords[i];
            std::string ssid;
            for (unsigned char s : ap.ssid) {
                ssid += s;
            }
            std::string rssi = std::to_string(ap.rssi);
            std::string auth(std::string(ap.authmode == WIFI_AUTH_OPEN ? "NO " : "YES"));
            std::vector<uint8_t> response =
                improv::build_rpc_response(improv::GET_WIFI_NETWORKS, {ssid, rssi, auth});

            if (ssid.length() > 0) {
                log_d("Sending latest AP scan result to client: %s %s %s", ssid.c_str(),
                      rssi.c_str(), auth.c_str());
                this->rpcResultCharacteristic_->setValue(response.data(), response.size());
                this->rpcResultCharacteristic_->notify();
            }
        }

        this->setState(STATE_WAITING);
    } else if (this->state_ == STATE_CONNECTED) {
        if (!WiFi.isConnected()) {
            this->setState(STATE_WAITING);
        }
    }
}

#endif
