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
    SCREEN_ID_SCR_STAGING_DETAILS = 5,
    SCREEN_ID_SCR_STAGING_ACTIONS = 6,
    SCREEN_ID_SCR_TRAY_DETAILS = 7,
    SCREEN_ID_SCR_TRAY_ACTIONS = 8,
    SCREEN_ID_SCR_TRAY_SELECT = 9,
    SCREEN_ID_SCR_SETTINGS_SPOOLMAN = 10,
    SCREEN_ID_SCR_SETTINGS_PRINTERS = 11,
    SCREEN_ID_SCR_SETTINGS_PRINTER_EDIT = 12,
    SCREEN_ID_SCR_SETTINGS_WIFI = 13,
    SCREEN_ID_SCR_SETTINGS_SCALE = 14,
    SCREEN_ID_SCR_SETTINGS_DEVICE = 15,
    SCREEN_ID_SCR_SETTINGS_DIAGNOSTICS = 16,
    SCREEN_ID_SCR_SETTINGS_FIRMWARE = 17,
    SCREEN_ID_SCR_TAG_ACTION_SELECT = 18,
    SCREEN_ID_SCR_TAG_REVIEW = 19,
    SCREEN_ID_SCR_TAG_WRITE = 20,
    SCREEN_ID_SCR_TAG_RESULT = 21,
    SCREEN_ID_SCR_TAG_DEFINITION_IMPORT = 22,
    SCREEN_ID_SCR_TAG_LEGACY = 23,
    SCREEN_ID_SCR_TAG_UNKNOWN = 24,
    SCREEN_ID_SCR_BAMBU_SPOOL_TYPE = 25,
    _SCREEN_ID_LAST = 25
};

typedef struct _objects_t {
    lv_obj_t *scr_boot;
    lv_obj_t *scr_home;
    lv_obj_t *scr_printer_select;
    lv_obj_t *scr_settings_home;
    lv_obj_t *scr_staging_details;
    lv_obj_t *scr_staging_actions;
    lv_obj_t *scr_tray_details;
    lv_obj_t *scr_tray_actions;
    lv_obj_t *scr_tray_select;
    lv_obj_t *scr_settings_spoolman;
    lv_obj_t *scr_settings_printers;
    lv_obj_t *scr_settings_printer_edit;
    lv_obj_t *scr_settings_wifi;
    lv_obj_t *scr_settings_scale;
    lv_obj_t *scr_settings_device;
    lv_obj_t *scr_settings_diagnostics;
    lv_obj_t *scr_settings_firmware;
    lv_obj_t *scr_tag_action_select;
    lv_obj_t *scr_tag_review;
    lv_obj_t *scr_tag_write;
    lv_obj_t *scr_tag_result;
    lv_obj_t *scr_tag_definition_import;
    lv_obj_t *scr_tag_legacy;
    lv_obj_t *scr_tag_unknown;
    lv_obj_t *scr_bambu_spool_type;
    lv_obj_t *boot_title;
    lv_obj_t *boot_version;
    lv_obj_t *boot_status;
    lv_obj_t *home_header;
    lv_obj_t *home_header_spoolman;
    lv_obj_t *home_header_wifi;
    lv_obj_t *home_header_printer;
    lv_obj_t *home_settings;
    lv_obj_t *home_ams_1;
    lv_obj_t *home_ams_2;
    lv_obj_t *home_active_ams;
    lv_obj_t *home_ams_4;
    lv_obj_t *home_weight;
    lv_obj_t *home_bottom_status;
    lv_obj_t *home_ams_1_1;
    lv_obj_t *home_ams_1_2;
    lv_obj_t *home_ams_1_3;
    lv_obj_t *home_ams_1_4;
    lv_obj_t *home_ams_2_1;
    lv_obj_t *home_ams_2_2;
    lv_obj_t *home_ams_2_3;
    lv_obj_t *home_ams_2_4;
    lv_obj_t *home_ams_3_1;
    lv_obj_t *home_ams_3_2;
    lv_obj_t *home_ams_3_3;
    lv_obj_t *home_ams_3_4;
    lv_obj_t *home_ams_4_1;
    lv_obj_t *home_ams_4_2;
    lv_obj_t *home_ams_4_3;
    lv_obj_t *home_ams_4_4;
    lv_obj_t *home_tray_2;
    lv_obj_t *home_tray_2__tray;
    lv_obj_t *home_tray_2__color_1;
    lv_obj_t *home_tray_2__color_2;
    lv_obj_t *home_tray_2__spoolmanager_id_container;
    lv_obj_t *home_tray_2__spoolmanager_id;
    lv_obj_t *home_tray_2__nozzle_icon;
    lv_obj_t *home_tray_2__material;
    lv_obj_t *home_tray_2__weight;
    lv_obj_t *home_tray_2__k_factor;
    lv_obj_t *home_tray_3;
    lv_obj_t *home_tray_3__tray;
    lv_obj_t *home_tray_3__color_1;
    lv_obj_t *home_tray_3__color_2;
    lv_obj_t *home_tray_3__spoolmanager_id_container;
    lv_obj_t *home_tray_3__spoolmanager_id;
    lv_obj_t *home_tray_3__nozzle_icon;
    lv_obj_t *home_tray_3__material;
    lv_obj_t *home_tray_3__weight;
    lv_obj_t *home_tray_3__k_factor;
    lv_obj_t *home_tray_4;
    lv_obj_t *home_tray_4__tray;
    lv_obj_t *home_tray_4__color_1;
    lv_obj_t *home_tray_4__color_2;
    lv_obj_t *home_tray_4__spoolmanager_id_container;
    lv_obj_t *home_tray_4__spoolmanager_id;
    lv_obj_t *home_tray_4__nozzle_icon;
    lv_obj_t *home_tray_4__material;
    lv_obj_t *home_tray_4__weight;
    lv_obj_t *home_tray_4__k_factor;
    lv_obj_t *home_tray_external;
    lv_obj_t *home_tray_external__tray;
    lv_obj_t *home_tray_external__color_1;
    lv_obj_t *home_tray_external__color_2;
    lv_obj_t *home_tray_external__spoolmanager_id_container;
    lv_obj_t *home_tray_external__spoolmanager_id;
    lv_obj_t *home_tray_external__nozzle_icon;
    lv_obj_t *home_tray_external__material;
    lv_obj_t *home_tray_external__weight;
    lv_obj_t *home_tray_external__k_factor;
    lv_obj_t *home_tray_1;
    lv_obj_t *home_tray_1__tray;
    lv_obj_t *home_tray_1__color_1;
    lv_obj_t *home_tray_1__color_2;
    lv_obj_t *home_tray_1__spoolmanager_id_container;
    lv_obj_t *home_tray_1__spoolmanager_id;
    lv_obj_t *home_tray_1__nozzle_icon;
    lv_obj_t *home_tray_1__material;
    lv_obj_t *home_tray_1__weight;
    lv_obj_t *home_tray_1__k_factor;
    lv_obj_t *staging;
    lv_obj_t *staging__staging;
    lv_obj_t *staging__color_3;
    lv_obj_t *staging__color_4;
    lv_obj_t *staging__spoolmanager_id_container;
    lv_obj_t *staging__spoolmanager_id;
    lv_obj_t *staging__staging_label;
    lv_obj_t *staging__material;
    lv_obj_t *staging__weight;
    lv_obj_t *staging__k_factor;
    lv_obj_t *select_header;
    lv_obj_t *select_header_spoolman;
    lv_obj_t *select_header_wifi;
    lv_obj_t *select_header_printer;
    lv_obj_t *select_settings;
    lv_obj_t *select_title;
    lv_obj_t *select_printer_1;
    lv_obj_t *select_printer_2;
    lv_obj_t *select_printer_3;
    lv_obj_t *select_printer_4;
    lv_obj_t *select_back;
    lv_obj_t *select_bottom_status;
    lv_obj_t *settings_header;
    lv_obj_t *settings_header_spoolman;
    lv_obj_t *settings_header_wifi;
    lv_obj_t *settings_header_printer;
    lv_obj_t *settings_title;
    lv_obj_t *settings_wifi;
    lv_obj_t *settings_spoolman;
    lv_obj_t *settings_scale;
    lv_obj_t *settings_printers;
    lv_obj_t *settings_device;
    lv_obj_t *settings_diagnostics;
    lv_obj_t *settings_firmware;
    lv_obj_t *settings_back;
    lv_obj_t *settings_bottom_status;
    lv_obj_t *staging_details_header;
    lv_obj_t *staging_details_header_spoolman;
    lv_obj_t *staging_details_header_wifi;
    lv_obj_t *staging_details_header_printer;
    lv_obj_t *staging_details_settings;
    lv_obj_t *staging_details_title;
    lv_obj_t *staging_details_content;
    lv_obj_t *staging_details_color_1;
    lv_obj_t *staging_details_color_2;
    lv_obj_t *staging_details_color_3;
    lv_obj_t *staging_details_quick_weight;
    lv_obj_t *staging_details_more;
    lv_obj_t *staging_details_close;
    lv_obj_t *staging_actions_header;
    lv_obj_t *staging_actions_header_spoolman;
    lv_obj_t *staging_actions_header_wifi;
    lv_obj_t *staging_actions_header_printer;
    lv_obj_t *staging_actions_settings;
    lv_obj_t *staging_action_configure;
    lv_obj_t *staging_action_advanced_weight;
    lv_obj_t *staging_action_clear;
    lv_obj_t *staging_action_write_tag;
    lv_obj_t *staging_action_link_tag;
    lv_obj_t *staging_action_unlink_tag;
    lv_obj_t *staging_action_erase_tag;
    lv_obj_t *staging_actions_back;
    lv_obj_t *tray_details_header;
    lv_obj_t *tray_details_header_spoolman;
    lv_obj_t *tray_details_header_wifi;
    lv_obj_t *tray_details_header_printer;
    lv_obj_t *tray_details_settings;
    lv_obj_t *tray_details_title;
    lv_obj_t *tray_details_tab_slot;
    lv_obj_t *tray_details_tab_spool;
    lv_obj_t *tray_details_content;
    lv_obj_t *tray_details_color_1;
    lv_obj_t *tray_details_color_2;
    lv_obj_t *tray_details_color_3;
    lv_obj_t *tray_details_more;
    lv_obj_t *tray_details_refresh;
    lv_obj_t *tray_details_close;
    lv_obj_t *tray_actions_header;
    lv_obj_t *tray_actions_header_spoolman;
    lv_obj_t *tray_actions_header_wifi;
    lv_obj_t *tray_actions_header_printer;
    lv_obj_t *tray_actions_settings;
    lv_obj_t *tray_action_from_staging;
    lv_obj_t *tray_action_manual;
    lv_obj_t *tray_action_untag;
    lv_obj_t *tray_action_reset;
    lv_obj_t *tray_action_reapply;
    lv_obj_t *tray_action_refresh;
    lv_obj_t *tray_actions_back;
    lv_obj_t *tray_select_header;
    lv_obj_t *tray_select_header_spoolman;
    lv_obj_t *tray_select_header_wifi;
    lv_obj_t *tray_select_header_printer;
    lv_obj_t *tray_select_settings;
    lv_obj_t *tray_select_title;
    lv_obj_t *tray_select_ams_1;
    lv_obj_t *tray_select_ams_2;
    lv_obj_t *tray_select_ams_3;
    lv_obj_t *tray_select_ams_4;
    lv_obj_t *tray_select_slot_1;
    lv_obj_t *tray_select_slot_2;
    lv_obj_t *tray_select_slot_3;
    lv_obj_t *tray_select_slot_4;
    lv_obj_t *tray_select_external;
    lv_obj_t *tray_select_summary;
    lv_obj_t *tray_select_cancel;
    lv_obj_t *spoolman_settings_header;
    lv_obj_t *spoolman_settings_header_spoolman;
    lv_obj_t *spoolman_settings_header_wifi;
    lv_obj_t *spoolman_settings_header_printer;
    lv_obj_t *spoolman_settings_settings;
    lv_obj_t *spoolman_settings_title;
    lv_obj_t *spoolman_setting_name;
    lv_obj_t *spoolman_setting_protocol;
    lv_obj_t *spoolman_setting_host;
    lv_obj_t *spoolman_setting_port;
    lv_obj_t *spoolman_setting_base_path;
    lv_obj_t *spoolman_setting_timeout;
    lv_obj_t *spoolman_setting_status;
    lv_obj_t *spoolman_setting_version;
    lv_obj_t *spoolman_setting_test;
    lv_obj_t *spoolman_setting_save;
    lv_obj_t *spoolman_setting_cancel;
    lv_obj_t *printer_settings_header;
    lv_obj_t *printer_settings_header_spoolman;
    lv_obj_t *printer_settings_header_wifi;
    lv_obj_t *printer_settings_header_printer;
    lv_obj_t *printer_settings_settings;
    lv_obj_t *printer_settings_title;
    lv_obj_t *printer_settings_row_1;
    lv_obj_t *printer_settings_row_2;
    lv_obj_t *printer_settings_row_3;
    lv_obj_t *printer_settings_row_4;
    lv_obj_t *printer_settings_active;
    lv_obj_t *printer_settings_enabled;
    lv_obj_t *printer_settings_default;
    lv_obj_t *printer_settings_add;
    lv_obj_t *printer_settings_edit;
    lv_obj_t *printer_settings_back;
    lv_obj_t *printer_edit_header;
    lv_obj_t *printer_edit_header_spoolman;
    lv_obj_t *printer_edit_header_wifi;
    lv_obj_t *printer_edit_header_printer;
    lv_obj_t *printer_edit_settings;
    lv_obj_t *printer_edit_title;
    lv_obj_t *printer_edit_name;
    lv_obj_t *printer_edit_host;
    lv_obj_t *printer_edit_serial;
    lv_obj_t *printer_edit_access_code;
    lv_obj_t *printer_edit_mask;
    lv_obj_t *printer_edit_test;
    lv_obj_t *printer_edit_save;
    lv_obj_t *printer_edit_delete;
    lv_obj_t *printer_edit_cancel;
    lv_obj_t *printer_edit_status;
    lv_obj_t *wifi_settings_header;
    lv_obj_t *wifi_settings_header_spoolman;
    lv_obj_t *wifi_settings_header_wifi;
    lv_obj_t *wifi_settings_header_printer;
    lv_obj_t *wifi_settings_settings;
    lv_obj_t *wifi_settings_title;
    lv_obj_t *wifi_settings_status;
    lv_obj_t *wifi_settings_ssid;
    lv_obj_t *wifi_settings_ip;
    lv_obj_t *wifi_settings_portal;
    lv_obj_t *wifi_settings_reset;
    lv_obj_t *wifi_settings_back;
    lv_obj_t *scale_settings_header;
    lv_obj_t *scale_settings_header_spoolman;
    lv_obj_t *scale_settings_header_wifi;
    lv_obj_t *scale_settings_header_printer;
    lv_obj_t *scale_settings_settings;
    lv_obj_t *scale_settings_title;
    lv_obj_t *scale_settings_weight;
    lv_obj_t *scale_settings_calibration;
    lv_obj_t *scale_settings_tare;
    lv_obj_t *scale_settings_calibrate;
    lv_obj_t *scale_settings_reset;
    lv_obj_t *scale_settings_back;
    lv_obj_t *device_settings_header;
    lv_obj_t *device_settings_header_spoolman;
    lv_obj_t *device_settings_header_wifi;
    lv_obj_t *device_settings_header_printer;
    lv_obj_t *device_settings_settings;
    lv_obj_t *device_settings_title;
    lv_obj_t *device_settings_name;
    lv_obj_t *device_settings_version;
    lv_obj_t *device_settings_storage;
    lv_obj_t *device_settings_restart;
    lv_obj_t *device_settings_back;
    lv_obj_t *diagnostics_settings_header;
    lv_obj_t *diagnostics_settings_header_spoolman;
    lv_obj_t *diagnostics_settings_header_wifi;
    lv_obj_t *diagnostics_settings_header_printer;
    lv_obj_t *diagnostics_settings_settings;
    lv_obj_t *diagnostics_settings_title;
    lv_obj_t *diagnostics_settings_heap;
    lv_obj_t *diagnostics_settings_psram;
    lv_obj_t *diagnostics_settings_tasks;
    lv_obj_t *diagnostics_settings_refresh;
    lv_obj_t *diagnostics_settings_back;
    lv_obj_t *firmware_settings_header;
    lv_obj_t *firmware_settings_header_spoolman;
    lv_obj_t *firmware_settings_header_wifi;
    lv_obj_t *firmware_settings_header_printer;
    lv_obj_t *firmware_settings_settings;
    lv_obj_t *firmware_settings_title;
    lv_obj_t *firmware_settings_current;
    lv_obj_t *firmware_settings_available;
    lv_obj_t *firmware_settings_status;
    lv_obj_t *firmware_settings_check;
    lv_obj_t *firmware_settings_back;
    lv_obj_t *tag_action_header;
    lv_obj_t *tag_action_header_spoolman;
    lv_obj_t *tag_action_header_wifi;
    lv_obj_t *tag_action_header_printer;
    lv_obj_t *tag_action_settings;
    lv_obj_t *tag_action_title;
    lv_obj_t *tag_action_info;
    lv_obj_t *tag_action_select_spool;
    lv_obj_t *tag_action_use_last_spool;
    lv_obj_t *tag_action_write;
    lv_obj_t *tag_action_erase;
    lv_obj_t *tag_action_back;
    lv_obj_t *tag_review_header;
    lv_obj_t *tag_review_header_spoolman;
    lv_obj_t *tag_review_header_wifi;
    lv_obj_t *tag_review_header_printer;
    lv_obj_t *tag_review_settings;
    lv_obj_t *tag_review_title;
    lv_obj_t *tag_review_summary;
    lv_obj_t *tag_review_back;
    lv_obj_t *tag_review_cancel;
    lv_obj_t *tag_review_confirm;
    lv_obj_t *tag_write_header;
    lv_obj_t *tag_write_header_spoolman;
    lv_obj_t *tag_write_header_wifi;
    lv_obj_t *tag_write_header_printer;
    lv_obj_t *tag_write_settings;
    lv_obj_t *tag_write_title;
    lv_obj_t *tag_write_detected;
    lv_obj_t *tag_write_memory;
    lv_obj_t *tag_write_data;
    lv_obj_t *tag_write_verify;
    lv_obj_t *tag_write_cancel;
    lv_obj_t *tag_result_header;
    lv_obj_t *tag_result_header_spoolman;
    lv_obj_t *tag_result_header_wifi;
    lv_obj_t *tag_result_header_printer;
    lv_obj_t *tag_result_settings;
    lv_obj_t *tag_result_title;
    lv_obj_t *tag_result_message;
    lv_obj_t *tag_result_quick_weight;
    lv_obj_t *tag_result_advanced_weight;
    lv_obj_t *tag_result_close;
    lv_obj_t *tag_definition_import_header;
    lv_obj_t *tag_definition_import_header_spoolman;
    lv_obj_t *tag_definition_import_header_wifi;
    lv_obj_t *tag_definition_import_header_printer;
    lv_obj_t *tag_definition_import_settings;
    lv_obj_t *tag_definition_import_title;
    lv_obj_t *tag_definition_import_summary;
    lv_obj_t *tag_definition_import_select_spool;
    lv_obj_t *tag_definition_import_spoolman;
    lv_obj_t *tag_definition_import_cancel;
    lv_obj_t *tag_legacy_header;
    lv_obj_t *tag_legacy_header_spoolman;
    lv_obj_t *tag_legacy_header_wifi;
    lv_obj_t *tag_legacy_header_printer;
    lv_obj_t *tag_legacy_settings;
    lv_obj_t *tag_legacy_title;
    lv_obj_t *tag_legacy_summary;
    lv_obj_t *tag_legacy_select_spool;
    lv_obj_t *tag_legacy_import;
    lv_obj_t *tag_legacy_migrate;
    lv_obj_t *tag_legacy_erase;
    lv_obj_t *tag_legacy_close;
    lv_obj_t *tag_unknown_header;
    lv_obj_t *tag_unknown_header_spoolman;
    lv_obj_t *tag_unknown_header_wifi;
    lv_obj_t *tag_unknown_header_printer;
    lv_obj_t *tag_unknown_settings;
    lv_obj_t *tag_unknown_title;
    lv_obj_t *tag_unknown_summary;
    lv_obj_t *tag_unknown_select_spool;
    lv_obj_t *tag_unknown_close;
    lv_obj_t *bambu_spool_type_header;
    lv_obj_t *bambu_spool_type_title;
    lv_obj_t *bambu_spool_type_low;
    lv_obj_t *bambu_spool_type_high;
    lv_obj_t *bambu_spool_type_manual;
    lv_obj_t *bambu_spool_type_back;
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

void create_screen_scr_staging_details();
void tick_screen_scr_staging_details();

void create_screen_scr_staging_actions();
void tick_screen_scr_staging_actions();

void create_screen_scr_tray_details();
void tick_screen_scr_tray_details();

void create_screen_scr_tray_actions();
void tick_screen_scr_tray_actions();

void create_screen_scr_tray_select();
void tick_screen_scr_tray_select();

void create_screen_scr_settings_spoolman();
void tick_screen_scr_settings_spoolman();

void create_screen_scr_settings_printers();
void tick_screen_scr_settings_printers();

void create_screen_scr_settings_printer_edit();
void tick_screen_scr_settings_printer_edit();

void create_screen_scr_settings_wifi();
void tick_screen_scr_settings_wifi();

void create_screen_scr_settings_scale();
void tick_screen_scr_settings_scale();

void create_screen_scr_settings_device();
void tick_screen_scr_settings_device();

void create_screen_scr_settings_diagnostics();
void tick_screen_scr_settings_diagnostics();

void create_screen_scr_settings_firmware();
void tick_screen_scr_settings_firmware();

void create_screen_scr_tag_action_select();
void tick_screen_scr_tag_action_select();

void create_screen_scr_tag_review();
void tick_screen_scr_tag_review();

void create_screen_scr_tag_write();
void tick_screen_scr_tag_write();

void create_screen_scr_tag_result();
void tick_screen_scr_tag_result();

void create_screen_scr_tag_definition_import();
void tick_screen_scr_tag_definition_import();

void create_screen_scr_tag_legacy();
void tick_screen_scr_tag_legacy();

void create_screen_scr_tag_unknown();
void tick_screen_scr_tag_unknown();

void create_screen_scr_bambu_spool_type();
void tick_screen_scr_bambu_spool_type();

void create_user_widget_cmp_spool_picker(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_spool_picker(int startWidgetIndex);

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

// Color themes

enum Themes {
    THEME_ID_DEFAULT,
};
enum Colors {
    COLOR_ID_BUTTON_ACTIVE,
    COLOR_ID_BUTTON_DISABLED,
    COLOR_ID_BUTTON_TEXT_ACTIVE,
    COLOR_ID_BUTTON_TEXT_DISABLED,
    COLOR_ID_BUTTON_NEUTRAL_ACTIVE,
    COLOR_ID_BUTTON_DANGER_ACTIVE,
    COLOR_ID_FILAMENT_TEXT_LIGHT,
    COLOR_ID_FILAMENT_TEXT_DARK,
};
void change_color_theme(uint32_t themeIndex);
extern uint32_t theme_colors[1][8];
extern uint32_t active_theme_index;

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/