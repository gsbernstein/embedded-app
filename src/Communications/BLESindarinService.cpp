#include "BLESindarinService.hpp"

#define SERVICE_UUID "db65598c-04b1-469f-833f-517f285db256"

BLEService *BLESindarinService::configureService(BLEServer *server) {
    service = server->createService(BLEUUID(SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service->getUUID());
    return service;
};

void BLESindarinService::start() { service->start(); }
