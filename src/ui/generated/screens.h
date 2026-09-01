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
    lv_obj_t *home_weight;
    lv_obj_t *home_bottom_status;
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
    lv_obj_t *obj0;
    lv_obj_t *obj0__home_header_1;
    lv_obj_t *obj0__printer_label;
    lv_obj_t *obj0__wifi;
    lv_obj_t *obj0__printer;
    lv_obj_t *obj0__nfc;
    lv_obj_t *obj0__spoolman;
    lv_obj_t *obj1;
    lv_obj_t *obj1__cmp_settings_button_content;
    lv_obj_t *ams1;
    lv_obj_t *ams1__ams;
    lv_obj_t *ams1__tray1;
    lv_obj_t *ams1__tray2;
    lv_obj_t *ams1__tray3;
    lv_obj_t *ams1__tray4;
    lv_obj_t *ams1__nozzle1;
    lv_obj_t *ams1__nozzle2;
    lv_obj_t *ams1__nozzle3;
    lv_obj_t *ams1__nozzle4;
    lv_obj_t *ams2;
    lv_obj_t *ams2__ams;
    lv_obj_t *ams2__tray1;
    lv_obj_t *ams2__tray2;
    lv_obj_t *ams2__tray3;
    lv_obj_t *ams2__tray4;
    lv_obj_t *ams2__nozzle1;
    lv_obj_t *ams2__nozzle2;
    lv_obj_t *ams2__nozzle3;
    lv_obj_t *ams2__nozzle4;
    lv_obj_t *ams3;
    lv_obj_t *ams3__ams;
    lv_obj_t *ams3__tray1;
    lv_obj_t *ams3__tray2;
    lv_obj_t *ams3__tray3;
    lv_obj_t *ams3__tray4;
    lv_obj_t *ams3__nozzle1;
    lv_obj_t *ams3__nozzle2;
    lv_obj_t *ams3__nozzle3;
    lv_obj_t *ams3__nozzle4;
    lv_obj_t *ams4;
    lv_obj_t *ams4__ams;
    lv_obj_t *ams4__tray1;
    lv_obj_t *ams4__tray2;
    lv_obj_t *ams4__tray3;
    lv_obj_t *ams4__tray4;
    lv_obj_t *ams4__nozzle1;
    lv_obj_t *ams4__nozzle2;
    lv_obj_t *ams4__nozzle3;
    lv_obj_t *ams4__nozzle4;
    lv_obj_t *select_title;
    lv_obj_t *select_printer_1;
    lv_obj_t *select_printer_2;
    lv_obj_t *select_printer_3;
    lv_obj_t *select_printer_4;
    lv_obj_t *select_back;
    lv_obj_t *select_bottom_status;
    lv_obj_t *obj2;
    lv_obj_t *obj2__home_header_1;
    lv_obj_t *obj2__printer_label;
    lv_obj_t *obj2__wifi;
    lv_obj_t *obj2__printer;
    lv_obj_t *obj2__nfc;
    lv_obj_t *obj2__spoolman;
    lv_obj_t *obj3;
    lv_obj_t *obj3__cmp_settings_button_content;
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
    lv_obj_t *obj4;
    lv_obj_t *obj4__home_header_1;
    lv_obj_t *obj4__printer_label;
    lv_obj_t *obj4__wifi;
    lv_obj_t *obj4__printer;
    lv_obj_t *obj4__nfc;
    lv_obj_t *obj4__spoolman;
    lv_obj_t *staging_details_title;
    lv_obj_t *staging_details_content;
    lv_obj_t *staging_details_color_1;
    lv_obj_t *staging_details_color_2;
    lv_obj_t *staging_details_color_3;
    lv_obj_t *staging_details_quick_weight;
    lv_obj_t *staging_details_more;
    lv_obj_t *staging_details_close;
    lv_obj_t *obj5;
    lv_obj_t *obj5__home_header_1;
    lv_obj_t *obj5__printer_label;
    lv_obj_t *obj5__wifi;
    lv_obj_t *obj5__printer;
    lv_obj_t *obj5__nfc;
    lv_obj_t *obj5__spoolman;
    lv_obj_t *obj6;
    lv_obj_t *obj6__cmp_settings_button_content;
    lv_obj_t *staging_action_configure;
    lv_obj_t *staging_action_advanced_weight;
    lv_obj_t *staging_action_clear;
    lv_obj_t *staging_action_write_tag;
    lv_obj_t *staging_action_link_tag;
    lv_obj_t *staging_action_unlink_tag;
    lv_obj_t *staging_action_erase_tag;
    lv_obj_t *staging_actions_back;
    lv_obj_t *obj7;
    lv_obj_t *obj7__home_header_1;
    lv_obj_t *obj7__printer_label;
    lv_obj_t *obj7__wifi;
    lv_obj_t *obj7__printer;
    lv_obj_t *obj7__nfc;
    lv_obj_t *obj7__spoolman;
    lv_obj_t *obj8;
    lv_obj_t *obj8__cmp_settings_button_content;
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
    lv_obj_t *obj9;
    lv_obj_t *obj9__home_header_1;
    lv_obj_t *obj9__printer_label;
    lv_obj_t *obj9__wifi;
    lv_obj_t *obj9__printer;
    lv_obj_t *obj9__nfc;
    lv_obj_t *obj9__spoolman;
    lv_obj_t *obj10;
    lv_obj_t *obj10__cmp_settings_button_content;
    lv_obj_t *tray_action_from_staging;
    lv_obj_t *tray_action_manual;
    lv_obj_t *tray_action_untag;
    lv_obj_t *tray_action_reset;
    lv_obj_t *tray_action_reapply;
    lv_obj_t *tray_action_refresh;
    lv_obj_t *tray_actions_back;
    lv_obj_t *obj11;
    lv_obj_t *obj11__home_header_1;
    lv_obj_t *obj11__printer_label;
    lv_obj_t *obj11__wifi;
    lv_obj_t *obj11__printer;
    lv_obj_t *obj11__nfc;
    lv_obj_t *obj11__spoolman;
    lv_obj_t *obj12;
    lv_obj_t *obj12__cmp_settings_button_content;
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
    lv_obj_t *obj13;
    lv_obj_t *obj13__home_header_1;
    lv_obj_t *obj13__printer_label;
    lv_obj_t *obj13__wifi;
    lv_obj_t *obj13__printer;
    lv_obj_t *obj13__nfc;
    lv_obj_t *obj13__spoolman;
    lv_obj_t *obj14;
    lv_obj_t *obj14__cmp_settings_button_content;
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
    lv_obj_t *obj15;
    lv_obj_t *obj15__home_header_1;
    lv_obj_t *obj15__printer_label;
    lv_obj_t *obj15__wifi;
    lv_obj_t *obj15__printer;
    lv_obj_t *obj15__nfc;
    lv_obj_t *obj15__spoolman;
    lv_obj_t *obj16;
    lv_obj_t *obj16__cmp_settings_button_content;
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
    lv_obj_t *obj17;
    lv_obj_t *obj17__home_header_1;
    lv_obj_t *obj17__printer_label;
    lv_obj_t *obj17__wifi;
    lv_obj_t *obj17__printer;
    lv_obj_t *obj17__nfc;
    lv_obj_t *obj17__spoolman;
    lv_obj_t *obj18;
    lv_obj_t *obj18__cmp_settings_button_content;
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
    lv_obj_t *obj19;
    lv_obj_t *obj19__home_header_1;
    lv_obj_t *obj19__printer_label;
    lv_obj_t *obj19__wifi;
    lv_obj_t *obj19__printer;
    lv_obj_t *obj19__nfc;
    lv_obj_t *obj19__spoolman;
    lv_obj_t *obj20;
    lv_obj_t *obj20__cmp_settings_button_content;
    lv_obj_t *wifi_settings_title;
    lv_obj_t *wifi_settings_status;
    lv_obj_t *wifi_settings_ssid;
    lv_obj_t *wifi_settings_ip;
    lv_obj_t *wifi_settings_portal;
    lv_obj_t *wifi_settings_reset;
    lv_obj_t *wifi_settings_back;
    lv_obj_t *obj21;
    lv_obj_t *obj21__home_header_1;
    lv_obj_t *obj21__printer_label;
    lv_obj_t *obj21__wifi;
    lv_obj_t *obj21__printer;
    lv_obj_t *obj21__nfc;
    lv_obj_t *obj21__spoolman;
    lv_obj_t *obj22;
    lv_obj_t *obj22__cmp_settings_button_content;
    lv_obj_t *scale_settings_title;
    lv_obj_t *scale_settings_weight;
    lv_obj_t *scale_settings_calibration;
    lv_obj_t *scale_settings_tare;
    lv_obj_t *scale_settings_calibrate;
    lv_obj_t *scale_settings_reset;
    lv_obj_t *scale_settings_back;
    lv_obj_t *obj23;
    lv_obj_t *obj23__home_header_1;
    lv_obj_t *obj23__printer_label;
    lv_obj_t *obj23__wifi;
    lv_obj_t *obj23__printer;
    lv_obj_t *obj23__nfc;
    lv_obj_t *obj23__spoolman;
    lv_obj_t *obj24;
    lv_obj_t *obj24__cmp_settings_button_content;
    lv_obj_t *device_settings_title;
    lv_obj_t *device_settings_name;
    lv_obj_t *device_settings_version;
    lv_obj_t *device_settings_storage;
    lv_obj_t *device_settings_restart;
    lv_obj_t *device_settings_back;
    lv_obj_t *obj25;
    lv_obj_t *obj25__home_header_1;
    lv_obj_t *obj25__printer_label;
    lv_obj_t *obj25__wifi;
    lv_obj_t *obj25__printer;
    lv_obj_t *obj25__nfc;
    lv_obj_t *obj25__spoolman;
    lv_obj_t *obj26;
    lv_obj_t *obj26__cmp_settings_button_content;
    lv_obj_t *diagnostics_settings_title;
    lv_obj_t *diagnostics_settings_heap;
    lv_obj_t *diagnostics_settings_psram;
    lv_obj_t *diagnostics_settings_tasks;
    lv_obj_t *diagnostics_settings_refresh;
    lv_obj_t *diagnostics_settings_back;
    lv_obj_t *obj27;
    lv_obj_t *obj27__home_header_1;
    lv_obj_t *obj27__printer_label;
    lv_obj_t *obj27__wifi;
    lv_obj_t *obj27__printer;
    lv_obj_t *obj27__nfc;
    lv_obj_t *obj27__spoolman;
    lv_obj_t *obj28;
    lv_obj_t *obj28__cmp_settings_button_content;
    lv_obj_t *firmware_settings_title;
    lv_obj_t *firmware_settings_current;
    lv_obj_t *firmware_settings_available;
    lv_obj_t *firmware_settings_status;
    lv_obj_t *firmware_settings_check;
    lv_obj_t *firmware_settings_back;
    lv_obj_t *obj29;
    lv_obj_t *obj29__home_header_1;
    lv_obj_t *obj29__printer_label;
    lv_obj_t *obj29__wifi;
    lv_obj_t *obj29__printer;
    lv_obj_t *obj29__nfc;
    lv_obj_t *obj29__spoolman;
    lv_obj_t *obj30;
    lv_obj_t *obj30__cmp_settings_button_content;
    lv_obj_t *tag_action_title;
    lv_obj_t *tag_action_info;
    lv_obj_t *tag_action_select_spool;
    lv_obj_t *tag_action_use_last_spool;
    lv_obj_t *tag_action_link_staging;
    lv_obj_t *tag_action_erase;
    lv_obj_t *tag_action_back;
    lv_obj_t *tag_action_load_to_staging;
    lv_obj_t *obj31;
    lv_obj_t *obj31__home_header_1;
    lv_obj_t *obj31__printer_label;
    lv_obj_t *obj31__wifi;
    lv_obj_t *obj31__printer;
    lv_obj_t *obj31__nfc;
    lv_obj_t *obj31__spoolman;
    lv_obj_t *obj32;
    lv_obj_t *obj32__cmp_settings_button_content;
    lv_obj_t *tag_review_title;
    lv_obj_t *tag_review_summary;
    lv_obj_t *tag_review_back;
    lv_obj_t *tag_review_cancel;
    lv_obj_t *tag_review_confirm;
    lv_obj_t *obj33;
    lv_obj_t *obj33__home_header_1;
    lv_obj_t *obj33__printer_label;
    lv_obj_t *obj33__wifi;
    lv_obj_t *obj33__printer;
    lv_obj_t *obj33__nfc;
    lv_obj_t *obj33__spoolman;
    lv_obj_t *obj34;
    lv_obj_t *obj34__cmp_settings_button_content;
    lv_obj_t *tag_write_title;
    lv_obj_t *tag_write_detected;
    lv_obj_t *tag_write_memory;
    lv_obj_t *tag_write_data;
    lv_obj_t *tag_write_verify;
    lv_obj_t *tag_write_cancel;
    lv_obj_t *obj35;
    lv_obj_t *obj35__home_header_1;
    lv_obj_t *obj35__printer_label;
    lv_obj_t *obj35__wifi;
    lv_obj_t *obj35__printer;
    lv_obj_t *obj35__nfc;
    lv_obj_t *obj35__spoolman;
    lv_obj_t *obj36;
    lv_obj_t *obj36__cmp_settings_button_content;
    lv_obj_t *tag_result_title;
    lv_obj_t *tag_result_message;
    lv_obj_t *tag_result_quick_weight;
    lv_obj_t *tag_result_advanced_weight;
    lv_obj_t *tag_result_close;
    lv_obj_t *obj37;
    lv_obj_t *obj37__home_header_1;
    lv_obj_t *obj37__printer_label;
    lv_obj_t *obj37__wifi;
    lv_obj_t *obj37__printer;
    lv_obj_t *obj37__nfc;
    lv_obj_t *obj37__spoolman;
    lv_obj_t *obj38;
    lv_obj_t *obj38__cmp_settings_button_content;
    lv_obj_t *tag_definition_import_title;
    lv_obj_t *tag_definition_import_summary;
    lv_obj_t *tag_definition_import_select_spool;
    lv_obj_t *tag_definition_import_spoolman;
    lv_obj_t *tag_definition_import_cancel;
    lv_obj_t *obj39;
    lv_obj_t *obj39__home_header_1;
    lv_obj_t *obj39__printer_label;
    lv_obj_t *obj39__wifi;
    lv_obj_t *obj39__printer;
    lv_obj_t *obj39__nfc;
    lv_obj_t *obj39__spoolman;
    lv_obj_t *obj40;
    lv_obj_t *obj40__cmp_settings_button_content;
    lv_obj_t *tag_legacy_title;
    lv_obj_t *tag_legacy_summary;
    lv_obj_t *tag_legacy_select_spool;
    lv_obj_t *tag_legacy_import;
    lv_obj_t *tag_legacy_migrate;
    lv_obj_t *tag_legacy_erase;
    lv_obj_t *tag_legacy_close;
    lv_obj_t *obj41;
    lv_obj_t *obj41__home_header_1;
    lv_obj_t *obj41__printer_label;
    lv_obj_t *obj41__wifi;
    lv_obj_t *obj41__printer;
    lv_obj_t *obj41__nfc;
    lv_obj_t *obj41__spoolman;
    lv_obj_t *obj42;
    lv_obj_t *obj42__cmp_settings_button_content;
    lv_obj_t *tag_unknown_title;
    lv_obj_t *tag_unknown_summary;
    lv_obj_t *tag_unknown_select_spool;
    lv_obj_t *tag_unknown_close;
    lv_obj_t *obj43;
    lv_obj_t *obj43__home_header_1;
    lv_obj_t *obj43__printer_label;
    lv_obj_t *obj43__wifi;
    lv_obj_t *obj43__printer;
    lv_obj_t *obj43__nfc;
    lv_obj_t *obj43__spoolman;
    lv_obj_t *obj44;
    lv_obj_t *obj44__cmp_settings_button_content;
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

void create_user_widget_cmp_ams_tray_overview(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_cmp_ams_tray_overview(int startWidgetIndex);

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