#pragma once
#include <cstdint>
struct esp_partition_t { uint32_t size; };
extern esp_partition_t testPartition;
inline const esp_partition_t *esp_ota_get_next_update_partition(const void *) { return &testPartition; }
