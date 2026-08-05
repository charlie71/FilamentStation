#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <soc/soc_memory_types.h>

#include <array>
#include <cstdint>
#include <cstdio>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchDriver.h"
#include "ui/generated/ui.h"
#include "ui/models/MockUiDataProvider.h"

namespace filament_station::ui {
namespace {

void* drawBuffer1 = nullptr;
void* drawBuffer2 = nullptr;
lv_display_t* lvglDisplay = nullptr;
lv_indev_t* touchInput = nullptr;
rtos::RtosContext* rtosContext = nullptr;
rtos::PrinterId currentPrinterId = 1;
std::uint8_t currentAmsId = 1;
std::uint32_t nextRequestId = 100;
constexpr std::size_t kHomeColorStripGroups = 6;
std::array<std::array<lv_obj_t*, models::kMaximumFilamentColors>,
           kHomeColorStripGroups>
    homeColorStrips{};
std::array<lv_obj_t*, 8> stagingTableRows{};

std::uint32_t tickMilliseconds() { return millis(); }

void flushDisplay(lv_display_t* display, const lv_area_t* area,
                  std::uint8_t* pixelMap) {
  const std::int32_t width = area->x2 - area->x1 + 1;
  const std::int32_t height = area->y2 - area->y1 + 1;
  drivers::displayDevice().pushImage(
      area->x1, area->y1, width, height,
      reinterpret_cast<const lgfx::rgb565_t*>(pixelMap));
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
  std::int32_t x = 0;
  std::int32_t y = 0;
  if (drivers::readTouchCoordinates(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

const char* connectionText(models::UiConnectionState state) {
  switch (state) {
    case models::UiConnectionState::Disabled:
      return "deaktiviert";
    case models::UiConnectionState::Connecting:
      return "verbindet";
    case models::UiConnectionState::Connected:
      return "online";
    case models::UiConnectionState::Offline:
      return "offline";
    case models::UiConnectionState::Error:
      return "Fehler";
  }
  return "unbekannt";
}

lv_obj_t* buttonLabel(lv_obj_t* button) {
  return button == nullptr ? nullptr : lv_obj_get_child(button, 0);
}

void setButtonText(lv_obj_t* button, const char* text) {
  lv_obj_t* label = buttonLabel(button);
  if (label != nullptr) {
    lv_label_set_text(label, text);
  }
}

void setButtonColors(lv_obj_t* button, std::uint32_t backgroundRgb) {
  lv_obj_set_style_bg_color(button, lv_color_hex(backgroundRgb), LV_PART_MAIN);
  const std::uint32_t red = (backgroundRgb >> 16U) & 0xFFU;
  const std::uint32_t green = (backgroundRgb >> 8U) & 0xFFU;
  const std::uint32_t blue = backgroundRgb & 0xFFU;
  const bool useDarkText = (red * 299U + green * 587U + blue * 114U) > 150000U;
  lv_obj_t* label = buttonLabel(button);
  if (label != nullptr) {
    lv_obj_set_style_text_color(label,
                                lv_color_hex(useDarkText ? 0x101820 : 0xFFFFFF),
                                LV_PART_MAIN);
  }
}

void sendAction(rtos::UiActionType type, rtos::PrinterId printerId,
                std::uint8_t amsId = 0, std::uint8_t trayId = 0,
                std::int32_t value = 0, rtos::SpoolId spoolId = 0) {
  if (rtosContext == nullptr) {
    return;
  }
  rtos::AppEvent event{};
  event.type = rtos::AppEventType::UiAction;
  event.requestId = nextRequestId++;
  event.uiAction.type = type;
  event.uiAction.requestId = event.requestId;
  event.uiAction.printerId = printerId;
  event.uiAction.spoolId = spoolId;
  event.uiAction.amsId = amsId;
  event.uiAction.trayId = trayId;
  event.uiAction.value = value;
  if (xQueueSend(rtosContext->appEventQueue, &event, 0) != pdPASS) {
    rtos::logLine("UiTask: appEventQueue overflow while sending UiAction");
  }
}

void headerClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::SelectPrinter, currentPrinterId, 0, 0, 1);
}

void settingsClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::OpenSettings, currentPrinterId);
}

void backClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::Back, currentPrinterId);
}

void stagingClicked(lv_event_t*) {
  const auto& staging = models::mock::staging();
  const rtos::SpoolId spoolId =
      staging.printerId == currentPrinterId ? staging.spoolId : 0;
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 0,
             spoolId);
}

void amsClicked(lv_event_t* event) {
  const auto amsId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectAms, currentPrinterId, amsId);
}

void trayClicked(lv_event_t* event) {
  const auto trayId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const models::UiPrinterSummary* printer =
      models::mock::findPrinter(currentPrinterId);
  const std::uint8_t amsId =
      trayId == 0xFF
          ? 0xFF
          : (printer == nullptr ? 0 : currentAmsId);
  const models::UiTraySummary* tray =
      models::mock::findTray(currentPrinterId, amsId, trayId);
  sendAction(rtos::UiActionType::SelectTray, currentPrinterId, amsId, trayId,
             0, tray == nullptr ? 0 : tray->spoolId);
}

void printerClicked(lv_event_t* event) {
  const auto printerId = static_cast<rtos::PrinterId>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectPrinter, printerId);
}

void settingsCategoryClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, currentPrinterId);
}

void managePrintersClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::OpenPrinterSettings, currentPrinterId);
}

void stagingMoreClicked(lv_event_t*) {
  const auto& staging = models::mock::staging();
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 1,
             staging.spoolId);
}

void stagingActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, currentPrinterId, 0, 0, 0,
             models::mock::staging().spoolId);
}

void bindClick(lv_obj_t* object, lv_event_cb_t callback,
               std::uintptr_t userData = 0) {
  lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(userData));
}

void styleLabelButton(lv_obj_t* object, std::uint32_t color = 0x1565C0) {
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_color(object, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_radius(object, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_top(object, 14, LV_PART_MAIN);
}

void createHomeColorStrips() {
  const std::array<lv_obj_t*, kHomeColorStripGroups> parents{{
      objects.home_tray_1, objects.home_tray_2, objects.home_tray_3,
      objects.home_tray_4, objects.home_external, objects.home_staging,
  }};
  constexpr lv_coord_t kColorDiameter = 10;
  constexpr lv_coord_t kColorSpacing = 2;
  lv_obj_update_layout(objects.scr_home);
  for (std::size_t group = 0; group < parents.size(); ++group) {
    const lv_coord_t parentX = lv_obj_get_x(parents[group]);
    const lv_coord_t parentY = lv_obj_get_y(parents[group]);
    const lv_coord_t parentWidth = lv_obj_get_width(parents[group]);
    for (std::size_t color = 0; color < models::kMaximumFilamentColors;
         ++color) {
      lv_obj_t* strip = lv_obj_create(objects.scr_home);
      homeColorStrips[group][color] = strip;
      lv_obj_set_pos(
          strip,
          parentX + parentWidth - kColorDiameter -
              static_cast<lv_coord_t>(color * (kColorDiameter + kColorSpacing)) -
              3,
          parentY + 4);
      lv_obj_set_size(strip, kColorDiameter, kColorDiameter);
      lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(strip, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_border_width(strip, 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(strip, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
      lv_obj_set_style_radius(strip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      lv_obj_set_style_pad_all(strip, 0, LV_PART_MAIN);
    }
  }
}

void updateHomeColorStrips(
    std::size_t group,
    const std::array<std::uint32_t, models::kMaximumFilamentColors>& colors,
    std::uint8_t colorCount) {
  const std::uint8_t visibleColorCount = colorCount > 1 ? colorCount : 0;
  for (std::size_t index = 0; index < models::kMaximumFilamentColors; ++index) {
    lv_obj_t* strip = homeColorStrips[group][index];
    if (index >= visibleColorCount) {
      lv_obj_add_flag(strip, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_HIDDEN);
    constexpr lv_coord_t kColorDiameter = 10;
    constexpr lv_coord_t kColorSpacing = 2;
    const std::array<lv_obj_t*, kHomeColorStripGroups> parents{{
        objects.home_tray_1, objects.home_tray_2, objects.home_tray_3,
        objects.home_tray_4, objects.home_external, objects.home_staging,
    }};
    lv_obj_t* parent = parents[group];
    lv_obj_set_pos(
        strip,
        lv_obj_get_x(parent) + lv_obj_get_width(parent) - kColorDiameter -
            static_cast<lv_coord_t>(index * (kColorDiameter + kColorSpacing)) -
            3,
        lv_obj_get_y(parent) + 4);
    lv_obj_set_size(strip, kColorDiameter, kColorDiameter);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(strip, lv_color_hex(colors[index]), LV_PART_MAIN);
    lv_obj_move_foreground(strip);
  }
}

void createStagingTableDecoration() {
  lv_obj_set_style_bg_opa(objects.staging_details_title, LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.staging_details_title,
                            lv_color_hex(0x1565C0), LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.staging_details_title,
                              lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_radius(objects.staging_details_title, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_left(objects.staging_details_title, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_top(objects.staging_details_title, 6, LV_PART_MAIN);
  lv_obj_add_flag(objects.staging_details_content, LV_OBJ_FLAG_HIDDEN);

  for (std::size_t row = 0; row < stagingTableRows.size(); ++row) {
    lv_obj_t* label = lv_label_create(objects.scr_staging_details);
    stagingTableRows[row] = label;
    lv_obj_set_pos(label, 8, 80 + static_cast<lv_coord_t>(row * 20));
    lv_obj_set_size(label, 464, 20);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_left(label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, 2, LV_PART_MAIN);
    if ((row % 2U) == 1U) {
      lv_obj_set_style_bg_color(label, lv_color_hex(0xB8BDC0), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(label, lv_color_hex(0xECEFF1), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(0x101820), LV_PART_MAIN);
  }
}

void bindGeneratedWidgets() {
  bindClick(objects.home_header, headerClicked);
  bindClick(objects.home_bottom_printers, headerClicked);
  bindClick(objects.select_header, headerClicked);
  bindClick(objects.settings_header, headerClicked);

  bindClick(objects.home_settings, settingsClicked);
  bindClick(objects.select_settings, settingsClicked);
  bindClick(objects.settings_settings, settingsClicked);
  bindClick(objects.select_back, backClicked);
  bindClick(objects.select_bottom_status, managePrintersClicked);
  bindClick(objects.settings_back, backClicked);

  bindClick(objects.staging_details_header, headerClicked);
  bindClick(objects.staging_actions_header, headerClicked);
  bindClick(objects.staging_details_settings, settingsClicked);
  bindClick(objects.staging_actions_settings, settingsClicked);
  bindClick(objects.staging_details_quick_weight, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::QuickWeight));
  bindClick(objects.staging_details_more, stagingMoreClicked);
  bindClick(objects.staging_details_close, backClicked);
  bindClick(objects.staging_actions_back, backClicked);

  bindClick(objects.staging_action_configure, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ConfigureSlot));
  bindClick(objects.staging_action_advanced_weight, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AdvancedWeight));
  bindClick(objects.staging_action_clear, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ClearStaging));
  bindClick(objects.staging_action_write_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::WriteTag));
  bindClick(objects.staging_action_link_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::LinkTag));
  bindClick(objects.staging_action_unlink_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::UnlinkTag));
  bindClick(objects.staging_action_erase_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::EraseTag));
  bindClick(objects.staging_action_search_spool, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SearchSpool));
  bindClick(objects.staging_action_spool_details, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SelectSpool));

  bindClick(objects.home_ams_1, amsClicked, 1);
  bindClick(objects.home_ams_2, amsClicked, 2);
  bindClick(objects.home_active_ams, amsClicked, 3);
  bindClick(objects.home_ams_4, amsClicked, 4);
  bindClick(objects.home_tray_1, trayClicked, 0);
  bindClick(objects.home_tray_2, trayClicked, 1);
  bindClick(objects.home_tray_3, trayClicked, 2);
  bindClick(objects.home_tray_4, trayClicked, 3);
  bindClick(objects.home_external, trayClicked, 0xFF);
  bindClick(objects.home_staging, stagingClicked);

  bindClick(objects.select_printer_1, printerClicked, 1);
  bindClick(objects.select_printer_2, printerClicked, 2);
  bindClick(objects.select_printer_3, printerClicked, 3);

  bindClick(objects.settings_wifi, settingsCategoryClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::OpenWifiSettings));
  bindClick(
      objects.settings_spoolman, settingsCategoryClicked,
      static_cast<std::uintptr_t>(rtos::UiActionType::OpenSpoolmanSettings));
  bindClick(objects.settings_scale, settingsCategoryClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::OpenScaleSettings));
  bindClick(
      objects.settings_printers, settingsCategoryClicked,
      static_cast<std::uintptr_t>(rtos::UiActionType::OpenPrinterSettings));
  bindClick(objects.settings_device, settingsCategoryClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::OpenDeviceSettings));
  bindClick(objects.settings_diagnostics, settingsCategoryClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::OpenDiagnostics));

  const std::array<lv_obj_t*, 14> stagingButtons{{
      objects.staging_details_header,
      objects.staging_details_settings,
      objects.staging_details_quick_weight,
      objects.staging_details_more,
      objects.staging_details_close,
      objects.staging_actions_header,
      objects.staging_actions_settings,
      objects.staging_action_configure,
      objects.staging_action_advanced_weight,
      objects.staging_action_write_tag,
      objects.staging_action_link_tag,
      objects.staging_action_unlink_tag,
      objects.staging_action_search_spool,
      objects.staging_action_spool_details,
  }};
  for (lv_obj_t* button : stagingButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.staging_action_clear, 0xC62828);
  styleLabelButton(objects.staging_action_erase_tag, 0xC62828);
  styleLabelButton(objects.staging_actions_back, 0x455A64);
  styleLabelButton(objects.home_active_ams, 0x455A64);
  styleLabelButton(objects.home_ams_4, 0x455A64);
  const lv_font_t* amsFont =
      lv_obj_get_style_text_font(buttonLabel(objects.home_ams_1), LV_PART_MAIN);
  for (lv_obj_t* amsLabel :
       std::array<lv_obj_t*, 2>{{objects.home_active_ams, objects.home_ams_4}}) {
    lv_obj_set_style_text_font(amsLabel, amsFont, LV_PART_MAIN);
    lv_obj_set_style_text_align(amsLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(amsLabel, 10, LV_PART_MAIN);
  }
  createHomeColorStrips();
  createStagingTableDecoration();
}

void updatePrinterList() {
  const auto& printers = models::mock::printers();
  const std::array<lv_obj_t*, 3> buttons{{
      objects.select_printer_1,
      objects.select_printer_2,
      objects.select_printer_3,
  }};

  for (std::size_t index = 0; index < printers.size(); ++index) {
    const auto& printer = printers[index];
    char text[96];
    std::snprintf(
        text, sizeof(text), "%s%s\n%s | %u AMS%s",
        printer.printerId == currentPrinterId ? "> " : "", printer.name,
        connectionText(printer.connectionState), printer.amsCount,
        printer.isDefault ? " | Standard" : "");
    setButtonText(buttons[index], text);

    const std::uint32_t color =
        printer.printerId == currentPrinterId
            ? 0x1565C0
            : (printer.connectionState == models::UiConnectionState::Connected
                   ? 0x2E7D32
                   : 0x616161);
    setButtonColors(buttons[index], color);
  }

  lv_obj_set_size(objects.select_bottom_status, 300, 48);
  lv_obj_set_style_bg_opa(objects.select_bottom_status, LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.select_bottom_status,
                            lv_color_hex(0x1565C0), LV_PART_MAIN);
  lv_obj_set_style_radius(objects.select_bottom_status, 8, LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.select_bottom_status,
                              lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(objects.select_bottom_status,
                              LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_pad_top(objects.select_bottom_status, 14, LV_PART_MAIN);
  lv_label_set_text(objects.select_bottom_status, "Drucker verwalten");
}

void updateTrayButton(lv_obj_t* button, rtos::PrinterId printerId,
                      std::uint8_t amsId, std::uint8_t trayId,
                      const char* title, std::size_t colorStripGroup) {
  const models::UiTraySummary* tray =
      models::mock::findTray(printerId, amsId, trayId);
  char text[64];
  if (tray == nullptr || tray->state == models::UiTrayState::Empty) {
    std::snprintf(text, sizeof(text), "%s\nleer", title);
    setButtonColors(button, 0x455A64);
    updateHomeColorStrips(colorStripGroup, {}, 0);
  } else {
    std::snprintf(text, sizeof(text), "%s\n%s #%lu\n%.0f g", title,
                  tray->material, static_cast<unsigned long>(tray->spoolId),
                  static_cast<double>(tray->remainingWeightGrams));
    setButtonColors(button,
                    tray->colorCount > 0 ? tray->colorRgb[0] : 0x455A64);
    updateHomeColorStrips(colorStripGroup, tray->colorRgb, tray->colorCount);
  }
  setButtonText(button, text);
}

void updateHomeContent() {
  const models::UiPrinterSummary* printer =
      models::mock::findPrinter(currentPrinterId);
  if (printer == nullptr) {
    return;
  }

  const std::array<lv_obj_t*, 4> amsButtons{{
      objects.home_ams_1,
      objects.home_ams_2,
      objects.home_active_ams,
      objects.home_ams_4,
  }};
  for (std::uint8_t amsId = 1; amsId <= amsButtons.size(); ++amsId) {
    lv_obj_t* button = amsButtons[amsId - 1U];
    const models::UiAmsSummary* ams =
        models::mock::findAms(currentPrinterId, amsId);
    char text[32];
    if (ams == nullptr || ams->trayCount == 0) {
      std::snprintf(text, sizeof(text), "AMS %u --", amsId);
    } else {
      std::snprintf(text, sizeof(text), "AMS %u  %u/%u", amsId,
                    ams->occupiedTrayCount, ams->trayCount);
    }
    if (amsId <= 2) {
      setButtonText(button, text);
    } else {
      lv_label_set_text(button, text);
    }
    const bool available = ams != nullptr && ams->trayCount > 0;
    lv_obj_set_state(button, LV_STATE_DISABLED, false);
    lv_obj_set_flag(button, LV_OBJ_FLAG_CLICKABLE, available);
    const std::uint32_t background =
        !available ? 0x616161 : (amsId == currentAmsId ? 0xF9A825 : 0x1565C0);
    if (amsId <= 2) {
      setButtonColors(button, background);
      if (!available) {
        lv_obj_t* label = buttonLabel(button);
        if (label != nullptr) {
          lv_obj_set_style_text_color(label, lv_color_hex(0xD7DCE0),
                                      LV_PART_MAIN);
        }
      }
    } else {
      lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_color(button, lv_color_hex(background), LV_PART_MAIN);
      lv_obj_set_style_text_color(button,
                                  lv_color_hex(available ? 0xFFFFFF : 0xD7DCE0),
                                  LV_PART_MAIN);
    }
  }

  updateTrayButton(objects.home_tray_1, currentPrinterId, currentAmsId, 0,
                   "Slot 1", 0);
  updateTrayButton(objects.home_tray_2, currentPrinterId, currentAmsId, 1,
                   "Slot 2", 1);
  updateTrayButton(objects.home_tray_3, currentPrinterId, currentAmsId, 2,
                   "Slot 3", 2);
  updateTrayButton(objects.home_tray_4, currentPrinterId, currentAmsId, 3,
                   "Slot 4", 3);
  updateTrayButton(objects.home_external, currentPrinterId, 0xFF, 0xFF,
                   "External", 4);

  const auto& staging = models::mock::staging();
  char stagingText[64];
  if (staging.printerId == currentPrinterId && staging.spoolId != 0) {
    std::snprintf(stagingText, sizeof(stagingText), "Staging\n%s #%lu\n%.0f g",
                  staging.material, static_cast<unsigned long>(staging.spoolId),
                  static_cast<double>(staging.remainingWeightGrams));
    setButtonColors(objects.home_staging,
                    staging.colorCount > 0 ? staging.colorRgb[0] : 0x455A64);
    updateHomeColorStrips(5, staging.colorRgb, staging.colorCount);
  } else {
    std::snprintf(stagingText, sizeof(stagingText), "Staging\nleer");
    setButtonColors(objects.home_staging, 0x455A64);
    updateHomeColorStrips(5, {}, 0);
  }
  setButtonText(objects.home_staging, stagingText);

  const auto& weight = models::mock::weight();
  char weightText[64];
  std::snprintf(weightText, sizeof(weightText), "Waage\n%.0f g\n%s",
                static_cast<double>(weight.grossWeightGrams), weight.status);
  setButtonText(objects.home_weight, weightText);

  const auto& settings = models::mock::settings();
  char statusText[96];
  std::snprintf(statusText, sizeof(statusText),
                "NFC: %s\nSpoolman: %s\nWLAN: %s", staging.nfcStatus,
                connectionText(settings.spoolmanState),
                connectionText(settings.wifiState));
  setButtonText(objects.home_status, statusText);
}

void updateStagingContent() {
  const auto& staging = models::mock::staging();
  const auto& spool = models::mock::spool();
  char colors[40];
  switch (staging.colorCount) {
    case 0:
      std::snprintf(colors, sizeof(colors), "keine");
      break;
    case 1:
      std::snprintf(colors, sizeof(colors), "#%06lX",
                    static_cast<unsigned long>(staging.colorRgb[0] & 0xFFFFFFU));
      break;
    case 2:
      std::snprintf(colors, sizeof(colors), "#%06lX / #%06lX",
                    static_cast<unsigned long>(staging.colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(staging.colorRgb[1] & 0xFFFFFFU));
      break;
    default:
      std::snprintf(colors, sizeof(colors), "#%06lX / #%06lX / #%06lX",
                    static_cast<unsigned long>(staging.colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(staging.colorRgb[1] & 0xFFFFFFU),
                    static_cast<unsigned long>(staging.colorRgb[2] & 0xFFFFFFU));
      break;
  }

  float remainingPercent = 0.0F;
  if (spool.initialWeightGrams > 0.0F) {
    remainingPercent =
        staging.remainingWeightGrams * 100.0F / spool.initialWeightGrams;
    if (remainingPercent < 0.0F) {
      remainingPercent = 0.0F;
    } else if (remainingPercent > 100.0F) {
      remainingPercent = 100.0F;
    }
  }

  char rowText[96];
  std::snprintf(rowText, sizeof(rowText), "Spoolman-ID: %lu",
                static_cast<unsigned long>(staging.spoolId));
  lv_label_set_text(stagingTableRows[0], rowText);
  std::snprintf(rowText, sizeof(rowText), "Hersteller: %s", staging.vendor);
  lv_label_set_text(stagingTableRows[1], rowText);
  std::snprintf(rowText, sizeof(rowText), "Material: %s", staging.material);
  lv_label_set_text(stagingTableRows[2], rowText);
  std::snprintf(rowText, sizeof(rowText), "Farben: %s", colors);
  lv_label_set_text(stagingTableRows[3], rowText);
  std::snprintf(rowText, sizeof(rowText), "Leergewicht: %.0f g",
                static_cast<double>(spool.emptyWeightGrams));
  lv_label_set_text(stagingTableRows[4], rowText);
  std::snprintf(rowText, sizeof(rowText), "Bruttogewicht: %.0f g",
                static_cast<double>(staging.grossWeightGrams));
  lv_label_set_text(stagingTableRows[5], rowText);
  std::snprintf(rowText, sizeof(rowText), "Restgewicht: %.0f g (%.1f %%)",
                static_cast<double>(staging.remainingWeightGrams),
                static_cast<double>(remainingPercent));
  lv_label_set_text(stagingTableRows[6], rowText);
  std::snprintf(rowText, sizeof(rowText), "NFC: %s", staging.nfcStatus);
  lv_label_set_text(stagingTableRows[7], rowText);

  const std::array<lv_obj_t*, models::kMaximumFilamentColors> colorFields{{
      objects.staging_details_color_1,
      objects.staging_details_color_2,
      objects.staging_details_color_3,
  }};
  for (std::size_t index = 0; index < colorFields.size(); ++index) {
    lv_obj_t* field = colorFields[index];
    if (index >= staging.colorCount) {
      lv_obj_add_flag(field, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(field, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(field, lv_color_hex(staging.colorRgb[index]),
                              LV_PART_MAIN);
    lv_obj_set_pos(field, 350 + static_cast<lv_coord_t>(index * 40), 138);
    lv_obj_set_style_border_width(field, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(field, 4, LV_PART_MAIN);
    lv_obj_move_foreground(field);
  }
}

void updateHeaders(rtos::PrinterId printerId) {
  const models::UiPrinterSummary* printer = models::mock::findPrinter(printerId);
  if (printer == nullptr) {
    return;
  }
  currentPrinterId = printerId;
  currentAmsId = printer->activeAmsId;

  char header[64];
  char ams[24];
  if (printer->activeAmsId == 0) {
    std::snprintf(ams, sizeof(ams), "kein AMS");
  } else {
    std::snprintf(ams, sizeof(ams), "AMS %u", printer->activeAmsId);
  }
  std::snprintf(header, sizeof(header), "%s | %s | %s",
                connectionText(printer->connectionState), printer->name, ams);

  setButtonText(objects.home_header, header);
  setButtonText(objects.select_header, header);
  setButtonText(objects.settings_header, header);
  lv_label_set_text(objects.staging_details_header, header);
  lv_label_set_text(objects.staging_actions_header, header);

  updateHomeContent();
  updatePrinterList();
  updateStagingContent();
}

void updateAmsOverview(rtos::PrinterId printerId, std::uint8_t amsId) {
  const models::UiPrinterSummary* printer = models::mock::findPrinter(printerId);
  if (printerId != currentPrinterId || printer == nullptr ||
      models::mock::findAms(printerId, amsId) == nullptr) {
    return;
  }
  currentAmsId = amsId;

  char header[64];
  std::snprintf(header, sizeof(header), "%s | %s | AMS %u",
                connectionText(printer->connectionState), printer->name,
                currentAmsId);
  setButtonText(objects.home_header, header);
  setButtonText(objects.select_header, header);
  setButtonText(objects.settings_header, header);
  lv_label_set_text(objects.staging_details_header, header);
  lv_label_set_text(objects.staging_actions_header, header);
  updateHomeContent();
}

void showScreen(rtos::UiScreenId screenId) {
  switch (screenId) {
    case rtos::UiScreenId::Boot:
      loadScreen(SCREEN_ID_SCR_BOOT);
      break;
    case rtos::UiScreenId::Home:
      loadScreen(SCREEN_ID_SCR_HOME);
      break;
    case rtos::UiScreenId::PrinterSelect:
      updatePrinterList();
      loadScreen(SCREEN_ID_SCR_PRINTER_SELECT);
      break;
    case rtos::UiScreenId::SettingsHome:
      loadScreen(SCREEN_ID_SCR_SETTINGS_HOME);
      break;
    case rtos::UiScreenId::StagingDetails:
      updateStagingContent();
      loadScreen(SCREEN_ID_SCR_STAGING_DETAILS);
      break;
    case rtos::UiScreenId::StagingActions:
      loadScreen(SCREEN_ID_SCR_STAGING_ACTIONS);
      break;
  }
}

void releaseDrawBuffers() {
  if (drawBuffer1 != nullptr) {
    heap_caps_free(drawBuffer1);
    drawBuffer1 = nullptr;
  }
  if (drawBuffer2 != nullptr) {
    heap_caps_free(drawBuffer2);
    drawBuffer2 = nullptr;
  }
}

}  // namespace

bool initializeLvgl(UiRuntimeInfo& runtimeInfo, rtos::RtosContext& context) {
  rtosContext = &context;
  lv_init();
  lv_tick_set_cb(tickMilliseconds);

  constexpr std::size_t kBytesPerPixel = sizeof(std::uint16_t);
  const std::size_t bufferBytes =
      static_cast<std::size_t>(config::kDisplayWidth) *
      config::kLvglDrawBufferLines * kBytesPerPixel;
  constexpr std::uint32_t kBufferCapabilities =
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  drawBuffer1 = heap_caps_malloc(bufferBytes, kBufferCapabilities);
  drawBuffer2 = heap_caps_malloc(bufferBytes, kBufferCapabilities);
  if (drawBuffer1 == nullptr || drawBuffer2 == nullptr) {
    releaseDrawBuffers();
    return false;
  }

  lvglDisplay =
      lv_display_create(config::kDisplayWidth, config::kDisplayHeight);
  if (lvglDisplay == nullptr) {
    releaseDrawBuffers();
    return false;
  }
  lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(lvglDisplay, drawBuffer1, drawBuffer2, bufferBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvglDisplay, flushDisplay);

  touchInput = lv_indev_create();
  if (touchInput == nullptr) {
    lv_display_delete(lvglDisplay);
    lvglDisplay = nullptr;
    releaseDrawBuffers();
    return false;
  }
  lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(touchInput, lvglDisplay);
  lv_indev_set_read_cb(touchInput, readTouch);

  ui_init();
  bindGeneratedWidgets();
  updateHeaders(currentPrinterId);
  loadScreen(SCREEN_ID_SCR_HOME);

  runtimeInfo.bytesPerDrawBuffer = bufferBytes;
  runtimeInfo.totalDrawBufferBytes = bufferBytes * 2U;
  runtimeInfo.drawBuffersInPsram = esp_ptr_external_ram(drawBuffer1) &&
                                   esp_ptr_external_ram(drawBuffer2);
  return true;
}

std::uint32_t runLvglTimers() {
  ui_tick();
  return lv_timer_handler();
}

void processUiCommand(const rtos::UiCommand& command) {
  switch (command.type) {
    case rtos::UiCommandType::ShowScreen:
      showScreen(command.screenId);
      break;
    case rtos::UiCommandType::UpdateHeader:
      updateHeaders(command.printerId);
      break;
    case rtos::UiCommandType::UpdateAmsOverview:
      updateAmsOverview(command.printerId, command.amsId);
      break;
    case rtos::UiCommandType::ShowStatus:
    case rtos::UiCommandType::ShowToast:
      lv_label_set_text(objects.home_bottom_status, command.text);
      break;
    default:
      break;
  }
}

}  // namespace filament_station::ui
