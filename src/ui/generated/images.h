#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_wifi;
extern const lv_img_dsc_t img_scale;
extern const lv_img_dsc_t img_3_d_printer_w;
extern const lv_img_dsc_t img_wifi_connected_w;
extern const lv_img_dsc_t img_back_w;
extern const lv_img_dsc_t img_scale_w;
extern const lv_img_dsc_t img_cancel;
extern const lv_img_dsc_t img_cancel_w;
extern const lv_img_dsc_t img_setup;
extern const lv_img_dsc_t img_setup_w;
extern const lv_img_dsc_t img_nfc_w;
extern const lv_img_dsc_t img_save_w;
extern const lv_img_dsc_t img_spoolman_connected_w;
extern const lv_img_dsc_t img_nfc;
extern const lv_img_dsc_t img_more_w;
extern const lv_img_dsc_t img_refresh_w;
extern const lv_img_dsc_t img_conneced_w;
extern const lv_img_dsc_t img_disconneced_w;
extern const lv_img_dsc_t img_wifi_disconnected_w;
extern const lv_img_dsc_t img_spoolman_disconneced;
extern const lv_img_dsc_t img_bambulab_logo;
extern const lv_img_dsc_t img_spoolman_logo;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[22];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/