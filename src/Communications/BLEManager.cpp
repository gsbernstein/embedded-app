#include "BLEManager.hpp"

#include "config.h"

BLEManager *BLEManager::instance = nullptr;

auto BLEManager::getInstance() -> BLEManager * {
    if (instance == nullptr) {
        instance = new BLEManager();
    }
    return instance;
}

BLEManager::BLEManager() { BLEDevice::init("Sindarin"); }

BLEServer *BLEManager::server() {
    if (_server == nullptr) {
        _server = BLEDevice::createServer();
        _server->setCallbacks(new ServerCallbacks());
    }
    return _server;
}

void BLEManager::startAdvertisingIfNecessary() {
    if (_server != nullptr) {
        auto advertising = _server->getAdvertising();
        advertising->start();
    }
}
