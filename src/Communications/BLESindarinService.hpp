#ifndef BLESindarinService_H
#define BLESindarinService_H

#include <NimBLEDevice.h>

#include "Communications/BLEAbstractions.hpp"

//! Stubbed service used as a container for other Sindarin services
class BLESindarinService : public BLEServiceProvider {

public:
    BLEService *configureService(BLEServer *server) override;
    void start() override;
};

#endif
