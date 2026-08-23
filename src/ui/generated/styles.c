#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: ButtonPrimary
//

void init_style_button_primary_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][0]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][2]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_primary_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_primary_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_button_primary_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][1]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_primary_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_primary_MAIN_DISABLED(style);
    }
    return style;
};

void add_style_button_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_button_primary_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

void remove_style_button_primary(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_primary_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_button_primary_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

//
// Style: ButtonNeutral
//

void init_style_button_neutral_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][4]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][2]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_neutral_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_neutral_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_button_neutral_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][1]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_neutral_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_neutral_MAIN_DISABLED(style);
    }
    return style;
};

void add_style_button_neutral(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_neutral_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_button_neutral_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

void remove_style_button_neutral(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_neutral_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_button_neutral_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

//
// Style: ButtonDanger
//

void init_style_button_danger_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][5]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][2]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_danger_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_danger_MAIN_DEFAULT(style);
    }
    return style;
};

void init_style_button_danger_MAIN_DISABLED(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(theme_colors[active_theme_index][1]));
    lv_style_set_text_color(style, lv_color_hex(theme_colors[active_theme_index][3]));
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_button_danger_MAIN_DISABLED() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_button_danger_MAIN_DISABLED(style);
    }
    return style;
};

void add_style_button_danger(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_button_danger_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_button_danger_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

void remove_style_button_danger(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_button_danger_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_button_danger_MAIN_DISABLED(), LV_PART_MAIN | LV_STATE_DISABLED);
};

//
// Style: LabelStandart
//

void init_style_label_standart_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_ui_german16);
};

lv_style_t *get_style_label_standart_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label_standart_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_label_standart(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label_standart_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_label_standart(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label_standart_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: LabelHeader
//

void init_style_label_header_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_font(style, &ui_font_ui_german18_bold);
};

lv_style_t *get_style_label_header_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = (lv_style_t *)lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_label_header_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_label_header(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_label_header_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_label_header(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_label_header_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_button_primary,
        add_style_button_neutral,
        add_style_button_danger,
        add_style_label_standart,
        add_style_label_header,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_button_primary,
        remove_style_button_neutral,
        remove_style_button_danger,
        remove_style_label_standart,
        remove_style_label_header,
    };
    remove_style_funcs[styleIndex](obj);
}