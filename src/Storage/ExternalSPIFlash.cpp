#include "Storage/ExternalSPIFlash.hpp"

#include <driver/spi_common.h>
#include <esp32-hal-log.h>
#include <esp_flash.h>
#include <esp_flash_spi_init.h>
#include <esp_partition.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>

esp_flash_t *SPIFlashInit(void) {
    // HSPI / SPI2
    const spi_bus_config_t bus_config = {
        .mosi_io_num = HSPI_IOMUX_PIN_NUM_MOSI, // IO13
        .miso_io_num = HSPI_IOMUX_PIN_NUM_MISO, // IO12
        .sclk_io_num = HSPI_IOMUX_PIN_NUM_CLK,  // IO14
        // .quadwp_io_num = HSPI_IOMUX_PIN_NUM_WP, // IO2
        // .quadhd_io_num = HSPI_IOMUX_PIN_NUM_HD, // IO4
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    const esp_flash_spi_device_config_t device_config = {.host_id = HSPI_HOST,
                                                         .cs_io_num = HSPI_IOMUX_PIN_NUM_CS, // IO15
                                                         .io_mode = SPI_FLASH_DIO,
                                                         // These modes should work with our flash
                                                         //  .io_mode = SPI_FLASH_QOUT,
                                                         //  .io_mode = SPI_FLASH_QIO,
                                                         .speed = ESP_FLASH_40MHZ};
    log_i("Initializing external SPI Flash");
    log_i("Pin assignments:\nSCLK: %2d MISO: %2d MOSI: %2d CS: %2d", bus_config.mosi_io_num,
          bus_config.miso_io_num, bus_config.sclk_io_num, device_config.cs_io_num);

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, 1));

    // Add device to the SPI bus
    esp_flash_t *ext_flash;
    ESP_ERROR_CHECK(spi_bus_add_flash_device(&ext_flash, &device_config));

    // Probe the Flash chip and initialize it
    esp_err_t err = esp_flash_init(ext_flash);
    if (err != ESP_OK) {
        log_e("Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(err), err);
        return NULL;
    }

    // Print out the ID and size
    uint32_t id;
    ESP_ERROR_CHECK(esp_flash_read_id(ext_flash, &id));
    // For some reason, we can't auto-detect the size of 512mbit flash chips
    if (ext_flash->size == 0 && id == 0xef4020) {
        ext_flash->size = 1024 * 1024 * 512 / 8;
        log_i("Detected 512mbit (64MB) flash ID=0x%x, hardcoding size to %d", id, ext_flash->size);
    }
    log_i("Initialized external Flash, size=%d KB, ID=0x%x", ext_flash->size / 1024, id);

    return ext_flash;
}

const esp_partition_t *SPIFlashAddPartition(esp_flash_t *ext_flash, const char *partition_label,
                                            esp_partition_subtype_t subtype) {
    size_t size = ext_flash->size;
    log_i("Adding external Flash as a partition, label=\"%s\", size=%d KB", partition_label,
          size / 1024);
    const esp_partition_t *partition;
    const int offset = 0x0;
    ESP_ERROR_CHECK(esp_partition_register_external(ext_flash, offset, size, partition_label,
                                                    ESP_PARTITION_TYPE_DATA, subtype, &partition));
    return partition;
}

void SPIFlashListPartitions(void) {
    log_i("Listing data partitions:");
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    for (; it != NULL; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);
        log_i("- chip '%d' partition '%s', subtype 0x%02x, offset 0x%x, size %d kB",
              part->flash_chip, part->label, part->subtype, part->address, part->size);
    }

    esp_partition_iterator_release(it);
}
