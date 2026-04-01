/**
 * LVGL Configuration for SSD1327 128x128 Grayscale Display
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include <stdbool.h>

/* Display size */
#define LV_HOR_RES_MAX          128
#define LV_VER_RES_MAX          128

/* Color depth: 1 (mono), 8 (256 colors), 16 (RGB565), 32 (ARGB8888) */
#define LV_COLOR_DEPTH          8

/* Enable grayscale support for 256-color mode */
#define LV_COLOR_16_SWAP        0
#define LV_COLOR_MIX_ROUND_OFS  (LV_COLOR_DEPTH == 32 ? 0 : 128)

/* Tick period in milliseconds */
#define LV_TICK_PERIOD_MS       10

/* Default font */
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_12

/* Enable various LVGL features */
#define LV_USE_OBJ_REALIGN      1
#define LV_USE_LARGE_DISPLAY    0

/* Memory settings */
#define LV_MEM_SIZE             (48 * 1024UL)
#define LV_MEM_POOL_INCLUDE_DRAM 1
#define LV_MEMSET_MEMCPY_SMALL  1

/* Logging */
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN

/* Animation settings */
#define LV_USE_ANIMATION        1

/* Theme settings */
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_COLOR_PRIMARY      lv_color_hex(0xFFFFFF)
#define LV_THEME_DEFAULT_COLOR_SECONDARY    lv_color_hex(0x808080)

/* Gesture settings */
#define LV_USE_GESTURES         0

/* Input device settings */
#define LV_USE_KEYBOARD         0
#define LV_USE_MOUSE            0

#endif /* LV_CONF_H */
