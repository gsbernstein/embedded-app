#include <esp_flash.h>
#include <esp_partition.h>

esp_flash_t *SPIFlashInit(void);

const esp_partition_t *SPIFlashAddPartition(esp_flash_t *ext_flash, const char *partition_label,
                                            esp_partition_subtype_t subtype);

void SPIFlashListPartitions(void);
