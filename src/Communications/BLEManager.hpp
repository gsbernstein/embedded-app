#pragma once
#ifndef BLEManager_H
#define BLEManager_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

class BLEManager
{
public:
    static BLEManager *getInstance();
    void startServiceAndAdvertising();
    BLEServer *server = nullptr;
    BLEService *service = nullptr;

protected:
    static BLEManager *instance;
    BLEManager();
};

#endif