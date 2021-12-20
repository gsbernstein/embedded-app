#pragma once

#include <NimBLEDevice.h>

//! Abstract class that can add one or more BLECharacteristics to a BLEService
class BLECharacteristicProvider {
public:
    virtual void configureCharacteristics(BLEService *service) = 0;
};

//! Abstract class that provides a BLEService
class BLEServiceProvider {
protected:
    BLEService *service;

public:
    virtual BLEService *configureService(BLEServer *server) = 0;
    virtual void start() = 0;
};
