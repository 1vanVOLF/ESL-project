#ifndef COLOR_STORAGE_H
#define COLOR_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "hsv_color.h"

#define COLOR_STORAGE_BOOTLOADER_ADDR   0xE0000UL
#define COLOR_STORAGE_PAGE_SIZE         0x1000UL   /* 4096 bytes          */
#define COLOR_STORAGE_PAGE_COUNT        3U
#define COLOR_STORAGE_AREA_SIZE         (COLOR_STORAGE_PAGE_SIZE * COLOR_STORAGE_PAGE_COUNT)
#define COLOR_STORAGE_START_ADDR        (COLOR_STORAGE_BOOTLOADER_ADDR - COLOR_STORAGE_AREA_SIZE)
#define COLOR_STORAGE_END_ADDR          (COLOR_STORAGE_START_ADDR + COLOR_STORAGE_AREA_SIZE)

bool color_storage_load(hsv_t *p_hsv);

void color_storage_save(hsv_t hsv);

#endif
