#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_SCR_BOOT = 1,
    SCREEN_ID_SCR_HOME = 2,
    SCREEN_ID_SCR_PRINTER_SELECT = 3,
    SCREEN_ID_SCR_SETTINGS_HOME = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *scr_boot;
    lv_obj_t *scr_home;
    lv_obj_t *scr_printer_select;
    lv_obj_t *scr_settings_home;
    lv_obj_t *boot_title;
    lv_obj_t *boot_version;
    lv_obj_t *boot_status;
    lv_obj_t *home_header;
    lv_obj_t *home_settings;
    lv_obj_t *home_ams_1;
    lv_obj_t *home_ams_2;
    lv_obj_t *home_active_ams;
    lv_obj_t *home_tray_1;
    lv_obj_t *home_tray_2;
    lv_obj_t *home_tray_3;
    lv_obj_t *home_tray_4;
    lv_obj_t *home_external;
    lv_obj_t *home_staging;
    lv_obj_t *home_weight;
    lv_obj_t *home_status;
    lv_obj_t *home_bottom_printers;
    lv_obj_t *home_bottom_status;
    lv_obj_t *select_header;
    lv_obj_t *select_settings;
    lv_obj_t *select_title;
    lv_obj_t *select_printer_1;
    lv_obj_t *select_printer_2;
    lv_obj_t *select_printer_3;
    lv_obj_t *select_back;
    lv_obj_t *select_bottom_status;
    lv_obj_t *settings_header;
    lv_obj_t *settings_settings;
    lv_obj_t *settings_title;
    lv_obj_t *settings_wifi;
    lv_obj_t *settings_spoolman;
    lv_obj_t *settings_scale;
    lv_obj_t *settings_printers;
    lv_obj_t *settings_device;
    lv_obj_t *settings_diagnostics;
    lv_obj_t *settings_back;
    lv_obj_t *settings_bottom_status;
} objects_t;

extern objects_t objects;

void create_screen_scr_boot();
void tick_screen_scr_boot();

void create_screen_scr_home();
void tick_screen_scr_home();

void create_screen_scr_printer_select();
void tick_screen_scr_printer_select();

void create_screen_scr_settings_home();
void tick_screen_scr_settings_home();

void create_user_widget_cmp_top_printer_bar(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_top_printer_bar(int startWidgetIndex);

void create_user_widget_cmp_bottom_action_bar(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_bottom_action_bar(int startWidgetIndex);

void create_user_widget_cmp_status_badge(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_status_badge(int startWidgetIndex);

void create_user_widget_cmp_connection_indicator(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_connection_indicator(int startWidgetIndex);

void create_user_widget_cmp_ams_selector(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_ams_selector(int startWidgetIndex);

void create_user_widget_cmp_tray_card(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_tray_card(int startWidgetIndex);

void create_user_widget_cmp_staging_card(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_staging_card(int startWidgetIndex);

void create_user_widget_cmp_spool_summary(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_spool_summary(int startWidgetIndex);

void create_user_widget_cmp_weight_display(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_weight_display(int startWidgetIndex);

void create_user_widget_cmp_progress_overlay(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_progress_overlay(int startWidgetIndex);

void create_user_widget_cmp_confirm_dialog(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_confirm_dialog(int startWidgetIndex);

void create_user_widget_cmp_result_dialog(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_result_dialog(int startWidgetIndex);

void create_user_widget_cmp_error_dialog(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_error_dialog(int startWidgetIndex);

void create_user_widget_cmp_numeric_input(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_numeric_input(int startWidgetIndex);

void create_user_widget_cmp_text_input(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_text_input(int startWidgetIndex);

void create_user_widget_cmp_settings_button(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_settings_button(int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/