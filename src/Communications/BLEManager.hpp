#pragma once
#ifndef BLEManager_H
#define BLEManager_H

#include <NimBLEDevice.h>

#include "sindarin-debug.h"

class BLEManager {
public:
    static auto getInstance() -> BLEManager *;
    void startAdvertisingIfNecessary();
    BLEServer *server();

protected:
    static BLEManager *instance;
    BLEServer *_server = nullptr;
    BLEManager();

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks() {}
        void onConnect(BLEServer *server) override { log_i("BLE client connected to our server!"); }
        void onDisconnect(BLEServer *server) override {
            log_i("BLE client disconnected from our server!");
        }
    };
};

#endif