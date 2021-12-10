#include "FileSystem.hpp"

#include <SPIFFS.h>

#include "Contents/Json.hpp"
#include "Storage/ExternalSPIFlash.hpp"
#include "sindarin-debug.h"

FileSystem *FileSystem::instance = nullptr;

auto FileSystem::getInstance() -> FileSystem * {
    if (instance == nullptr) {
        instance = new FileSystem();
    }
    return instance;
}

void FileSystem::getReadables(std::vector<Readable> *readables) {
#if !defined(DISABLE_SPIRAM)
    SpiRamJsonDocument *manifest = new SpiRamJsonDocument(2048);
#else
    DynamicJsonDocument *manifest = new DynamicJsonDocument(2048);
#endif

    File manifestFile = this->open("/manifest.json");
    if (!manifestFile) {
        log_e("Failed to open manifest file.");
    }
    deserializeJson(*manifest, manifestFile);
    manifestFile.close();

    JsonArray array = manifest->as<JsonArray>();
    readables->reserve(array.size());
    for (JsonVariant v : array) {
        auto readable = Readable(v["id"], v["name"], v["path"]);
        if (SPIFFS.exists(readable.path)) {
            log_d("Found readable %s %s %s", readable.id, readable.name, readable.path);
            readables->emplace_back(readable);
        } else {
            log_w("Couldn't find readable with path %s", readable.path);
        }
    }

    return;
}

void FileSystem::begin() {
    SHOW_HEAP();

#if INTERNAL_SPIFFS_FLASH
    const char *partitionLabel = nullptr;
#else
    const char *partitionLabel = "storage";

    // Set up SPI bus and initialize the external SPI Flash chip
    esp_flash_t *flash = SPIFlashInit();
    if (flash == NULL) {
        log_e("Failed to load external flash");
        return;
    }

    // Add the entire external flash chip as a partition
    SPIFlashAddPartition(flash, partitionLabel, ESP_PARTITION_SUBTYPE_DATA_SPIFFS);
#endif

    // Launch SPIFFS file system
    if (!SPIFFS.begin(false, "/spiffs", 255, partitionLabel)) {
        log_e("An Error has occurred while mounting SPIFFS");
    }

    SHOW_HEAP();
}

File FileSystem::open(const String &path) { return SPIFFS.open(path); }
