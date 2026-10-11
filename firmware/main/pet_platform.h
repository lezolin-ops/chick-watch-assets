#ifndef PET_PLATFORM_H
#define PET_PLATFORM_H
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#ifndef PET_EXT_RAM_BSS
#define PET_EXT_RAM_BSS EXT_RAM_BSS_ATTR
#endif
#else
#ifndef PET_EXT_RAM_BSS
#define PET_EXT_RAM_BSS
#endif
#endif
#endif
