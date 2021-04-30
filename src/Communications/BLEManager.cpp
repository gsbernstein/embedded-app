#include "BLEManager.hpp"

#define SERVICE_UUID "db65598c-04b1-469f-833f-517f285db256"

BLEManager *BLEManager::instance = nullptr;

BLEManager *BLEManager::getInstance()
{
    if (instance == nullptr)
    {
        instance = new BLEManager();
    }
    return instance;
}

BLEManager::BLEManager()
{
    BLEDevice::init("Sindarin (April)");
    server = BLEDevice::createServer();
    service = server->createService(BLEUUID(SERVICE_UUID));
}

void BLEManager::startServiceAndAdvertising()
{
    service->start();

    BLEAdvertising *advertising = server->getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->start();
}