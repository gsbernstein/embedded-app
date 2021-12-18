#include "BLEImprov.hpp"

#include <Arduino.h>

#include "config.h"

#include "sindarin-debug.h"

void BLEImprov::configureCharacteristics(BLEService *service) {
    auto *characteristic_callbacks = new CharacteristicCallbacks(this);

    BLECharacteristic *capabilities_characteristic =
        service->createCharacteristic(BLEUUID(improv::CAPABILITIES_UUID), NIMBLE_PROPERTY::READ);
    int supports_identify = 0;
    capabilities_characteristic->setValue(supports_identify);

    BLECharacteristic *current_state_characteristic = service->createCharacteristic(
        BLEUUID(improv::STATUS_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int state = improv::STATE_AUTHORIZED;
    current_state_characteristic->setValue(state);
    this->current_state_characteristic = current_state_characteristic;

    BLECharacteristic *error_state_characteristic = service->createCharacteristic(
        BLEUUID(improv::ERROR_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int no_error = improv::ERROR_NONE;
    error_state_characteristic->setValue(no_error);
    this->error_state_characteristic = error_state_characteristic;

    BLECharacteristic *rpc_command_characteristic =
        service->createCharacteristic(BLEUUID(improv::RPC_COMMAND_UUID), NIMBLE_PROPERTY::WRITE);
    rpc_command_characteristic->setCallbacks(characteristic_callbacks);

    BLECharacteristic *rpc_result_characteristic = service->createCharacteristic(
        BLEUUID(improv::RPC_RESULT_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    this->rpc_result_characteristic = rpc_result_characteristic;
}

BLEService *BLEImprov::configureService(BLEServer *server) {
    service = server->createService(BLEUUID(improv::SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service->getUUID());
    configureCharacteristics(service);
    return service;
}

void BLEImprov::start() {
    service->start();
    SHOW_HEAP();
}

void BLEImprov::setState(BLEImprovState state) {
    if (this->state == state) {
        return;
    }
    this->state = state;
    // Translate BLEImprov state to the most relevant Improv state
    int improv_state;
    if (state == STATE_WAITING) {
        improv_state = improv::STATE_AUTHORIZED;
    } else if (state == STATE_READY_TO_CONNECT) {
        this->setError(improv::ERROR_NONE, false);
        improv_state = improv::STATE_PROVISIONING;
    } else if (state == STATE_CONNECTING) {
        this->setError(improv::ERROR_NONE, false);
        improv_state = improv::STATE_PROVISIONING;
    } else if (state == STATE_CONNECTED) {
        this->setError(improv::ERROR_NONE, false);
        improv_state = improv::STATE_PROVISIONED;
    }
    log_d("Switched improv state to %d (internal state = %d)", improv_state, state);
#if !defined(DISABLE_IMPROV)
    this->current_state_characteristic->setValue(improv_state);
    this->current_state_characteristic->notify();
#endif
    this->last_state_change_millis = millis();
}

void BLEImprov::setError(improv::Error error, bool notify) {
#if !defined(DISABLE_IMPROV)
    int error_int = error;
    this->error_state_characteristic->setValue(error_int);
    if (notify) {
        this->error_state_characteristic->notify();
    }
#endif
}

void BLEImprov::doneConnecting() {
    this->setState(STATE_CONNECTED);
    log_i("Connected, IP address: %s", WiFi.localIP().toString().c_str());
    SHOW_HEAP();
}

void BLEImprov::loop() {
    if (this->state == STATE_WAITING) {
        if (WiFi.isConnected()) {
            this->doneConnecting();
        }
    } else if (this->state == STATE_READY_TO_CONNECT) {
        improv::ImprovCommand command = this->lastCommand;
        log_d("Attempting connection to: %s Password: %s (WiFi status = %d)", command.ssid.c_str(),
              command.password.c_str(), WiFi.status());
        WiFi.begin(command.ssid.c_str(), command.password.c_str());
        this->setState(STATE_CONNECTING);
        return;
    } else if (this->state == STATE_CONNECTING) {
        uint32_t now = millis();
        if (WiFi.status() == WL_CONNECTED) {
            std::string ip = WiFi.localIP().toString().c_str();
            std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {ip});
            log_d("Reporting RPC result to client");
            this->rpc_result_characteristic->setValue(response.data(), response.size());
            this->rpc_result_characteristic->notify();
            this->doneConnecting();
        } else if (now - this->last_state_change_millis > 30000) {
            log_w("Timed out connecting to WiFi %d", WiFi.status());
            this->setState(STATE_WAITING);
            this->setError(improv::ERROR_UNABLE_TO_CONNECT);
            WiFi.disconnect();
        }
    } else if (this->state == STATE_CONNECTED) {
        if (!WiFi.isConnected()) {
            this->setState(STATE_WAITING);
        }
    }
}
