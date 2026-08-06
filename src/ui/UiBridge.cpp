#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <soc/soc_memory_types.h>

#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchDriver.h"
#include "ui/generated/ui.h"
#include "ui/models/MockUiDataProvider.h"

extern "C" {
extern const lv_font_t ui_font_ui_german16;
}

namespace filament_station::ui {
namespace {

void* drawBuffer1 = nullptr;
void* drawBuffer2 = nullptr;
lv_display_t* lvglDisplay = nullptr;
lv_indev_t* touchInput = nullptr;
rtos::RtosContext* rtosContext = nullptr;
rtos::PrinterId currentPrinterId = 1;
std::uint8_t currentAmsId = 1;
std::uint8_t selectedTrayAmsId = 1;
std::uint8_t selectedTrayId = 0;
rtos::SpoolId selectedTraySpoolId = 0;
std::uint8_t selectedTrayTab = 0;
bool trayTargetSelected = false;
struct SpoolmanUiDraft {
  char name[32] = "Werkstatt";
  char protocol[8] = "http";
  char host[64] = "spoolman.local";
  char port[8] = "7912";
  char basePath[32] = "/api/v1";
  char timeoutMs[8] = "5000";
};
SpoolmanUiDraft spoolmanDraft{};
lv_obj_t* spoolmanEditor = nullptr;
lv_obj_t* spoolmanKeyboard = nullptr;
std::int32_t activeSpoolmanField = 0;
enum class EditorContext : std::uint8_t { None, Spoolman, Printer };
EditorContext editorContext = EditorContext::None;
std::int32_t activePrinterField = 0;
rtos::PrinterId managedPrinterId = 1;
rtos::PrinterId editingPrinterId = 1;
bool showPrinterAccessCode = false;
struct PrinterUiEntry {
  rtos::PrinterId id;
  char name[32];
  bool enabled;
  bool isDefault;
  bool isActive;
  bool exists;
};
std::array<PrinterUiEntry, 4> printerEntries{{
    {1, "P1S Werkstatt", true, true, true, true},
    {2, "X1C Labor", true, false, false, true},
    {3, "A1 Mini Buero", false, false, false, true},
    {4, "Neuer Drucker", true, false, false, false},
}};
struct PrinterUiDraft {
  char name[32];
  char host[64];
  char serial[32];
  char accessCode[16];
};
PrinterUiDraft printerUiDraft{};
constexpr const char* kKeyboardLowerMap[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "DEL", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "OK", "\n",
    "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
    "ABC", "123", "<", "SPACE", ">", "CANCEL", ""};
constexpr const char* kKeyboardUpperMap[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "DEL", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "OK", "\n",
    "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
    "abc", "123", "<", "SPACE", ">", "CANCEL", ""};
constexpr const char* kKeyboardNumberMap[] = {
    "1", "2", "3", "DEL", "\n", "4", "5", "6", "CANCEL", "\n",
    "7", "8", "9", "OK", "\n", "ABC", "0", ".", "<", ">", ""};
std::uint32_t nextRequestId = 100;
constexpr std::size_t kHomeColorStripGroups = 6;
std::array<std::array<lv_obj_t*, models::kMaximumFilamentColors>,
           kHomeColorStripGroups>
    homeColorStrips{};
std::array<lv_obj_t*, 8> stagingTableRows{};
bool touchWasPressed = false;
std::size_t touchMarkerColorIndex = 0;

void deleteTouchMarker(lv_timer_t* timer) {
  auto* marker = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
  if (marker != nullptr) {
    lv_obj_delete(marker);
  }
}

void showTouchMarker(std::int32_t x, std::int32_t y) {
  constexpr std::array<std::uint32_t, 5> kMarkerColors{{
      0xFFEB3B, 0x00E676, 0x00BCD4, 0xFF4081, 0xFF9100,
  }};
  constexpr lv_coord_t kMarkerDiameter = 12;
  lv_obj_t* marker = lv_obj_create(lv_layer_top());
  if (marker == nullptr) {
    return;
  }
  lv_obj_set_pos(marker, x - kMarkerDiameter / 2,
                 y - kMarkerDiameter / 2);
  lv_obj_set_size(marker, kMarkerDiameter, kMarkerDiameter);
  lv_obj_remove_flag(marker, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(marker, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(marker, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(marker, lv_color_hex(0x101820), LV_PART_MAIN);
  lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      marker,
      lv_color_hex(kMarkerColors[touchMarkerColorIndex % kMarkerColors.size()]),
      LV_PART_MAIN);
  ++touchMarkerColorIndex;

  lv_timer_t* timer = lv_timer_create(deleteTouchMarker, 2000, marker);
  if (timer == nullptr) {
    lv_obj_delete(marker);
    return;
  }
  lv_timer_set_repeat_count(timer, 1);
}

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
    if (!touchWasPressed) {
      showTouchMarker(x, y);
    }
    touchWasPressed = true;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    touchWasPressed = false;
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
                std::int32_t value = 0, rtos::SpoolId spoolId = 0,
                const char* text = nullptr) {
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
  if (text != nullptr) {
    std::snprintf(event.uiAction.text, sizeof(event.uiAction.text), "%s", text);
  }
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

const char* spoolmanFieldValue(std::int32_t field) {
  switch (field) {
    case 1:
      return spoolmanDraft.name;
    case 2:
      return spoolmanDraft.protocol;
    case 3:
      return spoolmanDraft.host;
    case 4:
      return spoolmanDraft.port;
    case 5:
      return spoolmanDraft.basePath;
    case 6:
      return spoolmanDraft.timeoutMs;
    default:
      return "";
  }
}

char* spoolmanFieldDestination(std::int32_t field) {
  return const_cast<char*>(spoolmanFieldValue(field));
}

std::size_t spoolmanFieldCapacity(std::int32_t field) {
  switch (field) {
    case 1:
      return sizeof(spoolmanDraft.name);
    case 2:
      return sizeof(spoolmanDraft.protocol);
    case 3:
      return sizeof(spoolmanDraft.host);
    case 4:
      return sizeof(spoolmanDraft.port);
    case 5:
      return sizeof(spoolmanDraft.basePath);
    case 6:
      return sizeof(spoolmanDraft.timeoutMs);
    default:
      return 0;
  }
}

void updateSpoolmanSettingsContent() {
  char text[96];
  std::snprintf(text, sizeof(text), "Name: %s", spoolmanDraft.name);
  lv_label_set_text(objects.spoolman_setting_name, text);
  std::snprintf(text, sizeof(text), "Protokoll: %s",
                spoolmanDraft.protocol);
  lv_label_set_text(objects.spoolman_setting_protocol, text);
  std::snprintf(text, sizeof(text), "Host: %s", spoolmanDraft.host);
  lv_label_set_text(objects.spoolman_setting_host, text);
  std::snprintf(text, sizeof(text), "Port: %s", spoolmanDraft.port);
  lv_label_set_text(objects.spoolman_setting_port, text);
  std::snprintf(text, sizeof(text), "Basispfad: %s", spoolmanDraft.basePath);
  lv_label_set_text(objects.spoolman_setting_base_path, text);
  std::snprintf(text, sizeof(text), "Timeout: %s ms", spoolmanDraft.timeoutMs);
  lv_label_set_text(objects.spoolman_setting_timeout, text);
}

void closeSpoolmanEditor() {
  if (spoolmanKeyboard != nullptr) {
    lv_obj_delete_async(spoolmanKeyboard);
    spoolmanKeyboard = nullptr;
  }
  if (spoolmanEditor != nullptr) {
    lv_obj_delete_async(spoolmanEditor);
    spoolmanEditor = nullptr;
  }
  activeSpoolmanField = 0;
  activePrinterField = 0;
  editorContext = EditorContext::None;
}

void spoolmanKeyboardEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
      spoolmanEditor == nullptr || spoolmanKeyboard == nullptr) {
    return;
  }
  const std::uint32_t button =
      lv_buttonmatrix_get_selected_button(spoolmanKeyboard);
  if (button == LV_BUTTONMATRIX_BUTTON_NONE) {
    return;
  }
  const char* key = lv_buttonmatrix_get_button_text(spoolmanKeyboard, button);
  if (key == nullptr) {
    return;
  }
  if (std::strcmp(key, "OK") == 0) {
    const bool printerEditor = editorContext == EditorContext::Printer;
    sendAction(printerEditor ? rtos::UiActionType::EditPrinterField
                             : rtos::UiActionType::EditSpoolmanSetting,
               printerEditor ? editingPrinterId : currentPrinterId, 0, 0,
               printerEditor ? activePrinterField : activeSpoolmanField, 0,
               lv_textarea_get_text(spoolmanEditor));
    closeSpoolmanEditor();
  } else if (std::strcmp(key, "CANCEL") == 0) {
    closeSpoolmanEditor();
  } else if (std::strcmp(key, "DEL") == 0) {
    lv_textarea_delete_char(spoolmanEditor);
  } else if (std::strcmp(key, "<") == 0) {
    lv_textarea_cursor_left(spoolmanEditor);
  } else if (std::strcmp(key, ">") == 0) {
    lv_textarea_cursor_right(spoolmanEditor);
  } else if (std::strcmp(key, "SPACE") == 0) {
    lv_textarea_add_char(spoolmanEditor, ' ');
  } else if (std::strcmp(key, "ABC") == 0) {
    lv_buttonmatrix_set_map(spoolmanKeyboard, kKeyboardUpperMap);
  } else if (std::strcmp(key, "abc") == 0) {
    lv_buttonmatrix_set_map(spoolmanKeyboard, kKeyboardLowerMap);
  } else if (std::strcmp(key, "123") == 0) {
    lv_buttonmatrix_set_map(spoolmanKeyboard, kKeyboardNumberMap);
  } else {
    lv_textarea_add_text(spoolmanEditor, key);
  }
}

void spoolmanFieldClicked(lv_event_t* event) {
  const auto field = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  if (field == 2) {
    const char* protocol =
        spoolmanDraft.protocol[4] == 's' ? "http" : "https";
    sendAction(rtos::UiActionType::EditSpoolmanSetting, currentPrinterId, 0,
               0, field, 0, protocol);
    return;
  }
  closeSpoolmanEditor();
  activeSpoolmanField = field;
  editorContext = EditorContext::Spoolman;
  lv_obj_scroll_to_y(objects.scr_settings_spoolman, 0, LV_ANIM_OFF);
  lv_obj_remove_flag(objects.scr_settings_spoolman, LV_OBJ_FLAG_SCROLLABLE);
  spoolmanEditor = lv_textarea_create(objects.scr_settings_spoolman);
  lv_obj_set_pos(spoolmanEditor, 4, 0);
  lv_obj_set_size(spoolmanEditor, 472, 40);
  lv_obj_set_style_pad_top(spoolmanEditor, 7, LV_PART_MAIN);
  lv_obj_set_style_text_font(spoolmanEditor, &ui_font_ui_german16,
                             LV_PART_MAIN);
  lv_textarea_set_one_line(spoolmanEditor, true);
  lv_textarea_set_max_length(spoolmanEditor,
                             spoolmanFieldCapacity(field) - 1U);
  lv_textarea_set_text(spoolmanEditor, spoolmanFieldValue(field));
  spoolmanKeyboard = lv_buttonmatrix_create(objects.scr_settings_spoolman);
  lv_obj_set_pos(spoolmanKeyboard, 0, 40);
  lv_obj_set_size(spoolmanKeyboard, 480, 180);
  lv_buttonmatrix_set_map(spoolmanKeyboard,
                          (field == 4 || field == 6) ? kKeyboardNumberMap
                                                     : kKeyboardLowerMap);
  lv_obj_set_style_pad_all(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_text_font(spoolmanKeyboard, LV_FONT_DEFAULT,
                             LV_PART_ITEMS);
  lv_obj_add_event_cb(spoolmanKeyboard, spoolmanKeyboardEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_move_foreground(spoolmanEditor);
  lv_obj_move_foreground(spoolmanKeyboard);
}

void spoolmanActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, currentPrinterId);
}

PrinterUiEntry* printerEntry(rtos::PrinterId id) {
  for (auto& entry : printerEntries) {
    if (entry.id == id) return &entry;
  }
  return nullptr;
}

const char* printerDraftValue(std::int32_t field) {
  switch (field) {
    case 1: return printerUiDraft.name;
    case 2: return printerUiDraft.host;
    case 3: return printerUiDraft.serial;
    case 4: return printerUiDraft.accessCode;
    default: return "";
  }
}

char* printerDraftDestination(std::int32_t field) {
  return const_cast<char*>(printerDraftValue(field));
}

std::size_t printerDraftCapacity(std::int32_t field) {
  switch (field) {
    case 1: return sizeof(printerUiDraft.name);
    case 2: return sizeof(printerUiDraft.host);
    case 3: return sizeof(printerUiDraft.serial);
    case 4: return sizeof(printerUiDraft.accessCode);
    default: return 0;
  }
}

void loadPrinterUiDraft(rtos::PrinterId id) {
  editingPrinterId = id;
  showPrinterAccessCode = false;
  if (id == 2) {
    std::snprintf(printerUiDraft.name, sizeof(printerUiDraft.name), "X1C Labor");
    std::snprintf(printerUiDraft.host, sizeof(printerUiDraft.host), "192.168.1.51");
    std::snprintf(printerUiDraft.serial, sizeof(printerUiDraft.serial), "00M987654321");
    std::snprintf(printerUiDraft.accessCode, sizeof(printerUiDraft.accessCode), "87654321");
  } else if (id == 3) {
    std::snprintf(printerUiDraft.name, sizeof(printerUiDraft.name), "A1 Mini Buero");
    std::snprintf(printerUiDraft.host, sizeof(printerUiDraft.host), "192.168.1.52");
    std::snprintf(printerUiDraft.serial, sizeof(printerUiDraft.serial), "030123456789");
    std::snprintf(printerUiDraft.accessCode, sizeof(printerUiDraft.accessCode), "11223344");
  } else if (id == 4) {
    std::snprintf(printerUiDraft.name, sizeof(printerUiDraft.name), "Neuer Drucker");
    printerUiDraft.host[0] = '\0';
    printerUiDraft.serial[0] = '\0';
    printerUiDraft.accessCode[0] = '\0';
  } else {
    std::snprintf(printerUiDraft.name, sizeof(printerUiDraft.name), "P1S Werkstatt");
    std::snprintf(printerUiDraft.host, sizeof(printerUiDraft.host), "192.168.1.50");
    std::snprintf(printerUiDraft.serial, sizeof(printerUiDraft.serial), "01P123456789");
    std::snprintf(printerUiDraft.accessCode, sizeof(printerUiDraft.accessCode), "12345678");
  }
}

void updatePrinterEditorContent() {
  char text[96];
  std::snprintf(text, sizeof(text), "Anzeigename: %s", printerUiDraft.name);
  lv_label_set_text(objects.printer_edit_name, text);
  std::snprintf(text, sizeof(text), "Host/IP: %s", printerUiDraft.host);
  lv_label_set_text(objects.printer_edit_host, text);
  std::snprintf(text, sizeof(text), "Seriennummer: %s", printerUiDraft.serial);
  lv_label_set_text(objects.printer_edit_serial, text);
  std::snprintf(text, sizeof(text), "LAN-Zugangscode: %s",
                showPrinterAccessCode ? printerUiDraft.accessCode : "********");
  lv_label_set_text(objects.printer_edit_access_code, text);
  lv_label_set_text(objects.printer_edit_mask,
                    showPrinterAccessCode ? "Verbergen" : "Anzeigen");
}

void updatePrinterSettingsList() {
  const std::array<lv_obj_t*, 4> rows{{objects.printer_settings_row_1,
                                      objects.printer_settings_row_2,
                                      objects.printer_settings_row_3,
                                      objects.printer_settings_row_4}};
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& entry = printerEntries[index];
    char text[96];
    if (!entry.exists) {
      std::snprintf(text, sizeof(text), "+ freier Druckerplatz (ID %u)", entry.id);
    } else {
      std::snprintf(text, sizeof(text), "%c %s | %s%s%s", entry.isActive ? '>' : ' ',
                    entry.name, entry.enabled ? "aktiv" : "deaktiviert",
                    entry.isDefault ? " | Standard" : "",
                    entry.isActive ? " | ausgewaehlt" : "");
    }
    lv_label_set_text(rows[index], text);
    lv_obj_set_style_bg_color(rows[index],
                              lv_color_hex(entry.id == managedPrinterId
                                               ? 0xEF6C00
                                               : (entry.enabled && entry.exists
                                                      ? 0x1565C0
                                                      : 0x78909C)),
                              LV_PART_MAIN);
  }
}

void printerRowClicked(lv_event_t* event) {
  const auto id = static_cast<rtos::PrinterId>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectManagedPrinter, id);
}

void printerListActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const rtos::PrinterId id = type == rtos::UiActionType::AddPrinter ? 4 : managedPrinterId;
  sendAction(type, id);
}

void printerFieldClicked(lv_event_t* event) {
  const auto field = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  closeSpoolmanEditor();
  editorContext = EditorContext::Printer;
  activePrinterField = field;
  spoolmanEditor = lv_textarea_create(objects.scr_settings_printer_edit);
  lv_obj_set_pos(spoolmanEditor, 4, 0);
  lv_obj_set_size(spoolmanEditor, 472, 40);
  lv_obj_set_style_text_font(spoolmanEditor, &ui_font_ui_german16, LV_PART_MAIN);
  lv_textarea_set_one_line(spoolmanEditor, true);
  lv_textarea_set_max_length(spoolmanEditor, printerDraftCapacity(field) - 1U);
  lv_textarea_set_text(spoolmanEditor, printerDraftValue(field));
  spoolmanKeyboard = lv_buttonmatrix_create(objects.scr_settings_printer_edit);
  lv_obj_set_pos(spoolmanKeyboard, 0, 40);
  lv_obj_set_size(spoolmanKeyboard, 480, 180);
  lv_buttonmatrix_set_map(spoolmanKeyboard, kKeyboardLowerMap);
  lv_obj_set_style_pad_all(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(spoolmanKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_text_font(spoolmanKeyboard, LV_FONT_DEFAULT, LV_PART_ITEMS);
  lv_obj_add_event_cb(spoolmanKeyboard, spoolmanKeyboardEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_move_foreground(spoolmanEditor);
  lv_obj_move_foreground(spoolmanKeyboard);
}

void printerMaskClicked(lv_event_t*) {
  showPrinterAccessCode = !showPrinterAccessCode;
  updatePrinterEditorContent();
}

void printerEditActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, editingPrinterId);
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

void trayDetailsClicked(lv_event_t* event) {
  const auto value = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectTray, currentPrinterId,
             selectedTrayAmsId, selectedTrayId, value, selectedTraySpoolId);
}

void trayActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, currentPrinterId, selectedTrayAmsId, selectedTrayId, 0,
             selectedTraySpoolId);
}

void trayTargetClicked(lv_event_t* event) {
  const auto trayId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const std::uint8_t amsId = trayId == 0xFF ? 0xFF : currentAmsId;
  sendAction(rtos::UiActionType::SelectTray, currentPrinterId, amsId, trayId,
             2, models::mock::staging().spoolId);
}

void bindClick(lv_obj_t* object, lv_event_cb_t callback,
               std::uintptr_t userData = 0) {
  lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(userData));
  // Widget binding is performed once at startup. Let IDLE0 run between the
  // many generated objects instead of monopolizing its core for several
  // watchdog periods.
  vTaskDelay(pdMS_TO_TICKS(10));
}

void styleLabelButton(lv_obj_t* object, std::uint32_t color = 0x1565C0) {
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_color(object, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_radius(object, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_top(object, 14, LV_PART_MAIN);
  vTaskDelay(pdMS_TO_TICKS(10));
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
      vTaskDelay(pdMS_TO_TICKS(5));
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
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void createTrayDetailsDecoration() {
  // Reuse the EEZ-owned label. Creating six additional labels here exhausted
  // the remaining internal LVGL heap after the printer-management screens
  // were added.
  lv_obj_remove_flag(objects.tray_details_content, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_opa(objects.tray_details_content, LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.tray_details_content,
                            lv_color_hex(0xECEFF1), LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.tray_details_content,
                              lv_color_hex(0x101820), LV_PART_MAIN);
  lv_obj_set_style_pad_all(objects.tray_details_content, 6, LV_PART_MAIN);
}

void applyApplicationFont() {
  const std::array<lv_obj_t*, 12> screens{{
      objects.scr_boot, objects.scr_home, objects.scr_printer_select,
      objects.scr_settings_home, objects.scr_staging_details,
      objects.scr_staging_actions, objects.scr_tray_details,
      objects.scr_tray_actions, objects.scr_tray_select,
      objects.scr_settings_spoolman,
      objects.scr_settings_printers, objects.scr_settings_printer_edit,
  }};
  for (lv_obj_t* screen : screens) {
    lv_obj_set_style_text_font(screen, &ui_font_ui_german16, LV_PART_MAIN);
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

  bindClick(objects.tray_details_header, headerClicked);
  bindClick(objects.tray_actions_header, headerClicked);
  bindClick(objects.tray_select_header, headerClicked);
  bindClick(objects.tray_details_settings, settingsClicked);
  bindClick(objects.tray_actions_settings, settingsClicked);
  bindClick(objects.tray_select_settings, settingsClicked);
  bindClick(objects.tray_details_tab_slot, trayDetailsClicked, 3);
  bindClick(objects.tray_details_tab_spool, trayDetailsClicked, 4);
  bindClick(objects.tray_details_more, trayDetailsClicked, 1);
  bindClick(objects.tray_details_refresh, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::RefreshSlot));
  bindClick(objects.tray_details_close, backClicked);
  bindClick(objects.tray_action_from_staging, trayActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::ConfigureSlotFromStaging));
  bindClick(objects.tray_action_manual, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SelectSpool));
  bindClick(objects.tray_action_untag, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::UntagSlot));
  bindClick(objects.tray_action_reset, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ResetSlot));
  bindClick(objects.tray_action_reapply, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ReapplySlot));
  bindClick(objects.tray_action_refresh, trayActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::RefreshSlot));
  bindClick(objects.tray_actions_back, backClicked);
  bindClick(objects.tray_select_ams_1, amsClicked, 1);
  bindClick(objects.tray_select_ams_2, amsClicked, 2);
  bindClick(objects.tray_select_ams_3, amsClicked, 3);
  bindClick(objects.tray_select_ams_4, amsClicked, 4);
  bindClick(objects.tray_select_slot_1, trayTargetClicked, 0);
  bindClick(objects.tray_select_slot_2, trayTargetClicked, 1);
  bindClick(objects.tray_select_slot_3, trayTargetClicked, 2);
  bindClick(objects.tray_select_slot_4, trayTargetClicked, 3);
  bindClick(objects.tray_select_external, trayTargetClicked, 0xFF);
  bindClick(objects.tray_select_cancel, backClicked);

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
  lv_obj_add_flag(objects.settings_firmware, LV_OBJ_FLAG_CLICKABLE);
  bindClick(objects.settings_firmware, settingsCategoryClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::OpenFirmwareSettings));

  bindClick(objects.spoolman_settings_header, headerClicked);
  bindClick(objects.spoolman_settings_settings, settingsClicked);
  bindClick(objects.spoolman_setting_name, spoolmanFieldClicked, 1);
  bindClick(objects.spoolman_setting_protocol, spoolmanFieldClicked, 2);
  bindClick(objects.spoolman_setting_host, spoolmanFieldClicked, 3);
  bindClick(objects.spoolman_setting_port, spoolmanFieldClicked, 4);
  bindClick(objects.spoolman_setting_base_path, spoolmanFieldClicked, 5);
  bindClick(objects.spoolman_setting_timeout, spoolmanFieldClicked, 6);
  bindClick(objects.spoolman_setting_test, spoolmanActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::TestSpoolmanConnection));
  bindClick(objects.spoolman_setting_save, spoolmanActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::SaveSpoolmanSettings));
  bindClick(objects.spoolman_setting_cancel, backClicked);

  bindClick(objects.printer_settings_header, headerClicked);
  bindClick(objects.printer_settings_settings, settingsClicked);
  bindClick(objects.printer_settings_row_1, printerRowClicked, 1);
  bindClick(objects.printer_settings_row_2, printerRowClicked, 2);
  bindClick(objects.printer_settings_row_3, printerRowClicked, 3);
  bindClick(objects.printer_settings_row_4, printerRowClicked, 4);
  bindClick(objects.printer_settings_active, printerListActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SetActivePrinter));
  bindClick(objects.printer_settings_enabled, printerListActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::TogglePrinterEnabled));
  bindClick(objects.printer_settings_default, printerListActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SetDefaultPrinter));
  bindClick(objects.printer_settings_add, printerListActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AddPrinter));
  bindClick(objects.printer_settings_edit, printerListActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::EditPrinter));
  bindClick(objects.printer_settings_back, backClicked);

  bindClick(objects.printer_edit_header, headerClicked);
  bindClick(objects.printer_edit_settings, settingsClicked);
  bindClick(objects.printer_edit_name, printerFieldClicked, 1);
  bindClick(objects.printer_edit_host, printerFieldClicked, 2);
  bindClick(objects.printer_edit_serial, printerFieldClicked, 3);
  bindClick(objects.printer_edit_access_code, printerFieldClicked, 4);
  bindClick(objects.printer_edit_mask, printerMaskClicked);
  bindClick(objects.printer_edit_test, printerEditActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::TestPrinterConnection));
  bindClick(objects.printer_edit_save, printerEditActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SavePrinterSettings));
  bindClick(objects.printer_edit_delete, printerEditActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::DeletePrinter));
  bindClick(objects.printer_edit_cancel, backClicked);

  const std::array<lv_obj_t*, 20> printerButtons{{
      objects.printer_settings_header, objects.printer_settings_settings,
      objects.printer_settings_row_1, objects.printer_settings_row_2,
      objects.printer_settings_row_3, objects.printer_settings_row_4,
      objects.printer_settings_active, objects.printer_settings_enabled,
      objects.printer_settings_default, objects.printer_settings_add,
      objects.printer_settings_edit, objects.printer_edit_header,
      objects.printer_edit_settings, objects.printer_edit_name,
      objects.printer_edit_host, objects.printer_edit_serial,
      objects.printer_edit_access_code, objects.printer_edit_mask,
      objects.printer_edit_test, objects.printer_edit_save,
  }};
  for (lv_obj_t* button : printerButtons) styleLabelButton(button);
  styleLabelButton(objects.printer_settings_back, 0x455A64);
  styleLabelButton(objects.printer_edit_delete, 0xC62828);
  styleLabelButton(objects.printer_edit_cancel, 0x455A64);

  const std::array<lv_obj_t*, 10> spoolmanButtons{{
      objects.spoolman_settings_header, objects.spoolman_settings_settings,
      objects.spoolman_setting_name, objects.spoolman_setting_protocol,
      objects.spoolman_setting_host, objects.spoolman_setting_port,
      objects.spoolman_setting_base_path, objects.spoolman_setting_timeout,
      objects.spoolman_setting_test, objects.spoolman_setting_save,
  }};
  for (lv_obj_t* button : spoolmanButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.spoolman_setting_cancel, 0x455A64);

  const std::array<lv_obj_t*, 12> stagingButtons{{
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
  }};
  for (lv_obj_t* button : stagingButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.staging_action_clear, 0xC62828);
  styleLabelButton(objects.staging_action_erase_tag, 0xC62828);
  styleLabelButton(objects.staging_actions_back, 0x455A64);
  const std::array<lv_obj_t*, 23> trayButtons{{
      objects.tray_details_header, objects.tray_details_settings,
      objects.tray_details_tab_slot, objects.tray_details_tab_spool,
      objects.tray_details_more, objects.tray_details_refresh,
      objects.tray_actions_header, objects.tray_actions_settings,
      objects.tray_action_from_staging, objects.tray_action_manual,
      objects.tray_action_reapply, objects.tray_action_refresh,
      objects.tray_select_header, objects.tray_select_settings,
      objects.tray_select_ams_1, objects.tray_select_ams_2,
      objects.tray_select_ams_3, objects.tray_select_ams_4,
      objects.tray_select_slot_1, objects.tray_select_slot_2,
      objects.tray_select_slot_3, objects.tray_select_slot_4,
      objects.tray_select_external,
  }};
  for (lv_obj_t* button : trayButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.tray_action_untag, 0xC62828);
  styleLabelButton(objects.tray_action_reset, 0xC62828);
  styleLabelButton(objects.tray_details_close, 0x455A64);
  styleLabelButton(objects.tray_actions_back, 0x455A64);
  styleLabelButton(objects.tray_select_cancel, 0x455A64);
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
  vTaskDelay(pdMS_TO_TICKS(250));
  createHomeColorStrips();
  vTaskDelay(pdMS_TO_TICKS(250));
  createStagingTableDecoration();
  vTaskDelay(pdMS_TO_TICKS(250));
  createTrayDetailsDecoration();
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
    setButtonText(button, text);
    const bool available = ams != nullptr && ams->trayCount > 0;
    lv_obj_set_state(button, LV_STATE_DISABLED, false);
    lv_obj_set_flag(button, LV_OBJ_FLAG_CLICKABLE, available);
    const std::uint32_t background =
        !available ? 0x616161 : (amsId == currentAmsId ? 0xF9A825 : 0x1565C0);
    setButtonColors(button, background);
    if (!available) {
      lv_obj_t* label = buttonLabel(button);
      if (label != nullptr) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xD7DCE0),
                                    LV_PART_MAIN);
      }
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

const char* trayStateText(models::UiTrayState state) {
  switch (state) {
    case models::UiTrayState::Empty: return "leer";
    case models::UiTrayState::Reading: return "wird gelesen";
    case models::UiTrayState::Ready: return "bereit";
    case models::UiTrayState::Loading: return "wird geladen";
    case models::UiTrayState::Loaded: return "geladen";
    case models::UiTrayState::Unloading: return "wird entladen";
    case models::UiTrayState::Error: return "Fehler";
    case models::UiTrayState::Unknown: return "unbekannt";
  }
  return "unbekannt";
}

void updateTrayDetails() {
  const models::UiTraySummary* tray = models::mock::findTray(
      currentPrinterId, selectedTrayAmsId, selectedTrayId);
  char title[48];
  if (selectedTrayId == 0xFF) {
    std::snprintf(title, sizeof(title), "External Slot");
  } else {
    std::snprintf(title, sizeof(title), "AMS %u | Slot %u",
                  selectedTrayAmsId, selectedTrayId + 1U);
  }
  lv_label_set_text(objects.tray_details_title, title);

  std::array<std::array<char, 96>, 6> rows{};
  if (tray == nullptr) {
    std::snprintf(rows[0].data(), rows[0].size(), "Keine Slotdaten verfügbar");
  } else if (selectedTrayTab == 0) {
    std::snprintf(rows[0].data(), rows[0].size(), "Status: %s",
                  trayStateText(tray->state));
    std::snprintf(rows[1].data(), rows[1].size(), "Material: %s", tray->material);
    if (tray->colorCount == 1) {
      std::snprintf(rows[2].data(), rows[2].size(), "Farben: #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU));
    } else if (tray->colorCount == 2) {
      std::snprintf(rows[2].data(), rows[2].size(), "Farben: #%06lX / #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[1] & 0xFFFFFFU));
    } else {
      std::snprintf(rows[2].data(), rows[2].size(),
                    "Farben: #%06lX / #%06lX / #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[1] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[2] & 0xFFFFFFU));
    }
    std::snprintf(rows[3].data(), rows[3].size(), "Loaded: %s",
                  tray->loaded ? "ja" : "nein");
    std::snprintf(rows[4].data(), rows[4].size(), "In Use: %s",
                  tray->inUse ? "ja" : "nein");
    std::snprintf(rows[5].data(), rows[5].size(), "Zuordnung: Spule %lu",
                  static_cast<unsigned long>(tray->spoolId));
  } else {
    std::snprintf(rows[0].data(), rows[0].size(), "Spoolman-ID: %lu",
                  static_cast<unsigned long>(tray->spoolId));
    std::snprintf(rows[1].data(), rows[1].size(), "Material: %s", tray->material);
    if (tray->colorCount == 1) {
      std::snprintf(rows[2].data(), rows[2].size(), "Farben: #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU));
    } else if (tray->colorCount == 2) {
      std::snprintf(rows[2].data(), rows[2].size(), "Farben: #%06lX / #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[1] & 0xFFFFFFU));
    } else {
      std::snprintf(rows[2].data(), rows[2].size(),
                    "Farben: #%06lX / #%06lX / #%06lX",
                    static_cast<unsigned long>(tray->colorRgb[0] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[1] & 0xFFFFFFU),
                    static_cast<unsigned long>(tray->colorRgb[2] & 0xFFFFFFU));
    }
    std::snprintf(rows[3].data(), rows[3].size(), "Restgewicht: %.0f g",
                  static_cast<double>(tray->remainingWeightGrams));
    std::snprintf(rows[4].data(), rows[4].size(), "Letzte Messung: Mock-Daten");
  }
  char content[448];
  std::snprintf(content, sizeof(content), "%s\n%s\n%s\n%s\n%s\n%s",
                rows[0].data(), rows[1].data(), rows[2].data(), rows[3].data(),
                rows[4].data(), rows[5].data());
  lv_label_set_text(objects.tray_details_content, content);
  const std::array<lv_obj_t*, models::kMaximumFilamentColors> colorFields{{
      objects.tray_details_color_1, objects.tray_details_color_2,
      objects.tray_details_color_3,
  }};
  for (std::size_t index = 0; index < colorFields.size(); ++index) {
    lv_obj_t* field = colorFields[index];
    if (tray == nullptr || index >= tray->colorCount) {
      lv_obj_add_flag(field, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(field, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(field, lv_color_hex(tray->colorRgb[index]),
                              LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, lv_color_hex(0x101820), LV_PART_MAIN);
    lv_obj_set_style_radius(field, 4, LV_PART_MAIN);
    lv_obj_move_foreground(field);
  }
  lv_obj_set_style_bg_color(objects.tray_details_tab_slot,
                            lv_color_hex(selectedTrayTab == 0 ? 0xF9A825 : 0x1565C0),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.tray_details_tab_spool,
                            lv_color_hex(selectedTrayTab == 1 ? 0xF9A825 : 0x1565C0),
                            LV_PART_MAIN);
}

void updateTraySelection(rtos::PrinterId printerId, std::uint8_t amsId,
                         std::uint8_t trayId, bool selected) {
  if (selected) {
    currentPrinterId = printerId;
    selectedTrayAmsId = amsId;
    selectedTrayId = trayId;
    trayTargetSelected = true;
  }
  const auto& staging = models::mock::staging();
  char title[80];
  std::snprintf(title, sizeof(title), "Zielslot für Spule %lu auswählen",
                static_cast<unsigned long>(staging.spoolId));
  lv_label_set_text(objects.tray_select_title, title);

  const std::array<lv_obj_t*, 4> amsButtons{{
      objects.tray_select_ams_1, objects.tray_select_ams_2,
      objects.tray_select_ams_3, objects.tray_select_ams_4,
  }};
  for (std::uint8_t id = 1; id <= amsButtons.size(); ++id) {
    const bool available = models::mock::findAms(currentPrinterId, id) != nullptr;
    char label[12];
    std::snprintf(label, sizeof(label), "AMS %u", id);
    lv_label_set_text(amsButtons[id - 1U], label);
    lv_obj_set_flag(amsButtons[id - 1U], LV_OBJ_FLAG_CLICKABLE, available);
    lv_obj_set_style_bg_color(
        amsButtons[id - 1U],
        lv_color_hex(!available ? 0x616161
                                : (id == currentAmsId ? 0xF9A825 : 0x1565C0)),
        LV_PART_MAIN);
  }

  const std::array<lv_obj_t*, 4> slotButtons{{
      objects.tray_select_slot_1, objects.tray_select_slot_2,
      objects.tray_select_slot_3, objects.tray_select_slot_4,
  }};
  for (std::uint8_t id = 0; id < slotButtons.size(); ++id) {
    char label[32];
    const auto* tray = models::mock::findTray(currentPrinterId, currentAmsId, id);
    std::snprintf(label, sizeof(label), "Slot %u\n%s", id + 1U,
                  tray == nullptr ? "frei" : trayStateText(tray->state));
    lv_label_set_text(slotButtons[id], label);
    const bool highlighted = trayTargetSelected && selectedTrayAmsId == currentAmsId &&
                             selectedTrayId == id;
    lv_obj_set_style_bg_color(slotButtons[id],
                              lv_color_hex(highlighted ? 0xF9A825 : 0x1565C0),
                              LV_PART_MAIN);
  }
  lv_obj_set_style_bg_color(
      objects.tray_select_external,
      lv_color_hex(trayTargetSelected && selectedTrayId == 0xFF ? 0xF9A825
                                                                 : 0x1565C0),
      LV_PART_MAIN);

  char summary[96];
  if (!trayTargetSelected) {
    std::snprintf(summary, sizeof(summary), "Noch kein Zielslot ausgewählt");
  } else if (selectedTrayId == 0xFF) {
    std::snprintf(summary, sizeof(summary), "Ziel: Drucker %u | External | Spule %lu",
                  currentPrinterId, static_cast<unsigned long>(staging.spoolId));
  } else {
    std::snprintf(summary, sizeof(summary),
                  "Ziel: Drucker %u | AMS %u | Slot %u | Spule %lu",
                  currentPrinterId, selectedTrayAmsId, selectedTrayId + 1U,
                  static_cast<unsigned long>(staging.spoolId));
  }
  lv_label_set_text(objects.tray_select_summary, summary);
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
  lv_label_set_text(objects.tray_details_header, header);
  lv_label_set_text(objects.tray_actions_header, header);
  lv_label_set_text(objects.tray_select_header, header);
  lv_label_set_text(objects.spoolman_settings_header, header);
  lv_label_set_text(objects.printer_settings_header, header);
  lv_label_set_text(objects.printer_edit_header, header);

  updateHomeContent();
  updatePrinterList();
  updateStagingContent();
  updateTrayDetails();
  updateTraySelection(currentPrinterId, currentAmsId, 0, false);
  updatePrinterSettingsList();
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
  lv_label_set_text(objects.tray_details_header, header);
  lv_label_set_text(objects.tray_actions_header, header);
  lv_label_set_text(objects.tray_select_header, header);
  lv_label_set_text(objects.spoolman_settings_header, header);
  lv_label_set_text(objects.printer_settings_header, header);
  lv_label_set_text(objects.printer_edit_header, header);
  updateHomeContent();
  updateTraySelection(currentPrinterId, currentAmsId, 0, false);
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
    case rtos::UiScreenId::TrayDetails:
      updateTrayDetails();
      loadScreen(SCREEN_ID_SCR_TRAY_DETAILS);
      break;
    case rtos::UiScreenId::TrayActions:
      loadScreen(SCREEN_ID_SCR_TRAY_ACTIONS);
      break;
    case rtos::UiScreenId::TraySelect:
      trayTargetSelected = false;
      updateTraySelection(currentPrinterId, currentAmsId, 0, false);
      loadScreen(SCREEN_ID_SCR_TRAY_SELECT);
      break;
    case rtos::UiScreenId::SettingsSpoolman:
      closeSpoolmanEditor();
      updateSpoolmanSettingsContent();
      loadScreen(SCREEN_ID_SCR_SETTINGS_SPOOLMAN);
      break;
    case rtos::UiScreenId::SettingsPrinters:
      closeSpoolmanEditor();
      updatePrinterSettingsList();
      loadScreen(SCREEN_ID_SCR_SETTINGS_PRINTERS);
      break;
    case rtos::UiScreenId::SettingsPrinterEdit:
      closeSpoolmanEditor();
      updatePrinterEditorContent();
      lv_label_set_text(objects.printer_edit_status, "Status: Mock-Daten");
      loadScreen(SCREEN_ID_SCR_SETTINGS_PRINTER_EDIT);
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
  rtos::logLine("UiTask: EEZ screens created");
  // A one-tick delay can expire on the next tick boundary without IDLE0 ever
  // running. Reserve a real scheduling window during this one-time startup.
  vTaskDelay(pdMS_TO_TICKS(250));
  applyApplicationFont();
  rtos::logLine("UiTask: UI font applied");
  vTaskDelay(pdMS_TO_TICKS(250));
  bindGeneratedWidgets();
  rtos::logLine("UiTask: UI widgets bound");
  vTaskDelay(pdMS_TO_TICKS(250));
  updateHeaders(currentPrinterId);
  rtos::logLine("UiTask: initial UI model applied");
  vTaskDelay(pdMS_TO_TICKS(250));
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
      if (command.screenId == rtos::UiScreenId::TrayDetails ||
          command.screenId == rtos::UiScreenId::TrayActions) {
        currentPrinterId = command.printerId;
        selectedTrayAmsId = command.amsId;
        selectedTrayId = command.trayId;
        selectedTraySpoolId = command.spoolId;
      }
      if (command.screenId == rtos::UiScreenId::SettingsPrinterEdit) {
        loadPrinterUiDraft(command.printerId);
      }
      showScreen(command.screenId);
      break;
    case rtos::UiCommandType::UpdateHeader:
      updateHeaders(command.printerId);
      break;
    case rtos::UiCommandType::UpdateAmsOverview:
      updateAmsOverview(command.printerId, command.amsId);
      break;
    case rtos::UiCommandType::UpdateTrayDetails:
      if (command.value == 2) {
        selectedTraySpoolId = command.spoolId;
        updateTraySelection(command.printerId, command.amsId, command.trayId,
                            true);
      } else {
        selectedTrayTab = command.value == 4 ? 1 : 0;
        updateTrayDetails();
      }
      break;
    case rtos::UiCommandType::UpdateSettings: {
      const bool printerFieldUpdate = command.value >= 21 && command.value <= 24;
      const std::int32_t field = printerFieldUpdate ? command.value - 20 : command.value;
      char* destination = printerFieldUpdate ? printerDraftDestination(field)
                                             : spoolmanFieldDestination(field);
      const std::size_t capacity = printerFieldUpdate
                                       ? printerDraftCapacity(field)
                                       : spoolmanFieldCapacity(field);
      if (destination != nullptr && capacity > 0) {
        std::snprintf(destination, capacity, "%s", command.text);
        if (printerFieldUpdate) {
          updatePrinterEditorContent();
          lv_label_set_text(objects.printer_edit_status,
                            "Status: geaendert, nicht gespeichert");
        } else {
          updateSpoolmanSettingsContent();
          lv_label_set_text(objects.spoolman_setting_status,
                            "Status: geaendert, nicht gespeichert");
          lv_label_set_text(objects.spoolman_setting_version, "Server: -");
        }
      }
      break;
    }
    case rtos::UiCommandType::UpdatePrinterList: {
      managedPrinterId = command.printerId;
      PrinterUiEntry* entry = printerEntry(command.printerId);
      if (entry != nullptr) {
        if (command.value == 1) entry->enabled = !entry->enabled;
        if (command.value == 2) {
          for (auto& item : printerEntries) item.isDefault = false;
          entry->isDefault = true;
        }
        if (command.value == 3 && entry->exists && entry->enabled) {
          for (auto& item : printerEntries) item.isActive = false;
          entry->isActive = true;
          updateHeaders(entry->id);
        }
        if (command.value == 4) entry->exists = false;
      }
      updatePrinterSettingsList();
      break;
    }
    case rtos::UiCommandType::ShowStatus:
    case rtos::UiCommandType::ShowToast:
      lv_label_set_text(objects.home_bottom_status, command.text);
      lv_label_set_text(objects.settings_bottom_status, command.text);
      if (command.value >= 100) {
        lv_label_set_text(objects.spoolman_setting_status, command.text);
      }
      if (command.value >= 200) {
        lv_label_set_text(objects.printer_edit_status, command.text);
        if (command.value == 202) {
          PrinterUiEntry* entry = printerEntry(command.printerId);
          if (entry != nullptr) {
            entry->exists = true;
            std::snprintf(entry->name, sizeof(entry->name), "%s",
                          printerUiDraft.name);
          }
        }
      }
      if (command.value == 101 && command.title[0] != '\0') {
        char version[64];
        std::snprintf(version, sizeof(version), "Server: %s", command.title);
        lv_label_set_text(objects.spoolman_setting_version, version);
      } else if (command.value == 100) {
        lv_label_set_text(objects.spoolman_setting_version, "Server: -");
      }
      break;
    default:
      break;
  }
}

}  // namespace filament_station::ui
