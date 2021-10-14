#pragma once
#ifndef BLEManager_H
#define BLEManager_H

#include <NimBLEDevice.h>

class BLEManager {
public:
    static BLEManager *getInstance();
    void startServiceAndAdvertising();
    BLEClient *client = nullptr;
    BLEServer *server = nullptr;
    BLEService *service = nullptr;

protected:
    static BLEManager *instance;
    BLEManager();
};

#endif