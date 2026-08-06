#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130
#define LV_MEM_SIZE (128U * 1024U)
#define LV_MEM_POOL_INCLUDE <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) \
  heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1

#endif  // LV_CONF_H
