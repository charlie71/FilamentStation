#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: ButtonPrimary
lv_style_t *get_style_button_primary_MAIN_DEFAULT();
lv_style_t *get_style_button_primary_MAIN_DISABLED();
void add_style_button_primary(lv_obj_t *obj);
void remove_style_button_primary(lv_obj_t *obj);

// Style: ButtonNeutral
lv_style_t *get_style_button_neutral_MAIN_DEFAULT();
lv_style_t *get_style_button_neutral_MAIN_DISABLED();
void add_style_button_neutral(lv_obj_t *obj);
void remove_style_button_neutral(lv_obj_t *obj);

// Style: ButtonDanger
lv_style_t *get_style_button_danger_MAIN_DEFAULT();
lv_style_t *get_style_button_danger_MAIN_DISABLED();
void add_style_button_danger(lv_obj_t *obj);
void remove_style_button_danger(lv_obj_t *obj);

// Style: LabelHeader
lv_style_t *get_style_label_header_MAIN_DEFAULT();
void add_style_label_header(lv_obj_t *obj);
void remove_style_label_header(lv_obj_t *obj);

// Style: LabelHeader_W
lv_style_t *get_style_label_header_w_MAIN_DEFAULT();
void add_style_label_header_w(lv_obj_t *obj);
void remove_style_label_header_w(lv_obj_t *obj);

// Style: LabelStandart
lv_style_t *get_style_label_standart_MAIN_DEFAULT();
void add_style_label_standart(lv_obj_t *obj);
void remove_style_label_standart(lv_obj_t *obj);

// Style: LabelStandart_W
lv_style_t *get_style_label_standart_w_MAIN_DEFAULT();
void add_style_label_standart_w(lv_obj_t *obj);
void remove_style_label_standart_w(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/