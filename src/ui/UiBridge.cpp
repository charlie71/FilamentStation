#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <soc/soc_memory_types.h>

#include <array>
#include <cstdlib>
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
    {3, "A1 Mini Büro", false, false, false, true},
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
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "Entf.", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "OK", "\n",
    "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
    "ABC", "123", "<", "Leer", ">", "Abbr.", ""};
constexpr const char* kKeyboardUpperMap[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "Entf.", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "OK", "\n",
    "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
    "abc", "123", "<", "Leer", ">", "Abbr.", ""};
constexpr const char* kKeyboardNumberMap[] = {
    "1", "2", "3", "Entf.", "\n", "4", "5", "6", "Abbr.", "\n",
    "7", "8", "9", "OK", "\n", "ABC", "0", ".", "<", ">", ""};
std::uint32_t nextRequestId = 100;
models::UiWeightState liveWeight{0.0F, 0.0F, false, false, true,
                                 "wartet auf Messwert"};
lv_obj_t* calibrationEditor = nullptr;
lv_obj_t* calibrationKeyboard = nullptr;
constexpr std::size_t kHomeColorStripGroups = 6;
std::array<std::array<lv_obj_t*, models::kMaximumFilamentColors>,
           kHomeColorStripGroups>
    homeColorStrips{};
std::array<lv_obj_t*, 8> stagingTableRows{};
bool touchWasPressed = false;
std::size_t touchMarkerColorIndex = 0;
lv_obj_t* overlayBackdrop = nullptr;
lv_obj_t* overlayPanel = nullptr;
lv_obj_t* overlayTitle = nullptr;
lv_obj_t* overlayText = nullptr;
lv_obj_t* overlayProgress = nullptr;
lv_obj_t* overlayCancel = nullptr;
lv_obj_t* overlayConfirm = nullptr;
std::array<lv_obj_t*, 4> advancedModeButtons{};
std::array<lv_obj_t*, 4> spoolPickerButtons{};
lv_obj_t* advancedInput = nullptr;
lv_obj_t* advancedKeyboard = nullptr;
std::int32_t advancedInputMode = 0;
rtos::UiOverlayKind activeOverlayKind = rtos::UiOverlayKind::None;
std::uint32_t activeOverlayRequestId = 0;

void sendAction(rtos::UiActionType type, rtos::PrinterId printerId,
                std::uint8_t amsId = 0, std::uint8_t trayId = 0,
                std::int32_t value = 0, rtos::SpoolId spoolId = 0,
                const char* text = nullptr);
void updateWeightDisplays();

constexpr const char* kAdvancedNumberMap[] = {
    "1", "2", "3", "Entf.", "\n", "4", "5", "6", "Abbr.", "\n",
    "7", "8", "9", "OK", "\n", "0", ".", ""};

void advancedModeClicked(lv_event_t* event) {
  const auto mode = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::AdvancedWeight, currentPrinterId, 0, 0, mode);
}

void spoolPickerItemClicked(lv_event_t* event) {
  const auto spoolId = static_cast<rtos::SpoolId>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectSpool, currentPrinterId, 0, 0, 0,
             spoolId);
}

lv_obj_t* createSpoolPickerButton(const char* text, std::int32_t y,
                                  rtos::SpoolId spoolId) {
  lv_obj_t* button = lv_button_create(overlayPanel);
  lv_obj_set_pos(button, 16, y);
  lv_obj_set_size(button, 388, 40);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1565C0), LV_PART_MAIN);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(button);
  lv_obj_set_width(label, 372);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_ui_german16, LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      button, spoolPickerItemClicked, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<std::uintptr_t>(spoolId)));
  lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
  return button;
}

void advancedKeyboardEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
      advancedInput == nullptr || advancedKeyboard == nullptr) return;
  const std::uint32_t button =
      lv_buttonmatrix_get_selected_button(advancedKeyboard);
  if (button == LV_BUTTONMATRIX_BUTTON_NONE) return;
  const char* key = lv_buttonmatrix_get_button_text(advancedKeyboard, button);
  if (key == nullptr) return;
  if (std::strcmp(key, "OK") == 0) {
    sendAction(rtos::UiActionType::AdvancedWeight, currentPrinterId, 0, 0,
               advancedInputMode, 0, lv_textarea_get_text(advancedInput));
  } else if (std::strcmp(key, "Abbr.") == 0) {
    sendAction(rtos::UiActionType::Cancel, currentPrinterId);
  } else if (std::strcmp(key, "Entf.") == 0) {
    lv_textarea_delete_char(advancedInput);
  } else {
    lv_textarea_add_text(advancedInput, key);
  }
}

lv_obj_t* createAdvancedModeButton(const char* text, std::int32_t x,
                                   std::int32_t y, std::int32_t mode) {
  lv_obj_t* button = lv_button_create(overlayPanel);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, 186, 48);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1565C0), LV_PART_MAIN);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_ui_german16, LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(button, advancedModeClicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<std::uintptr_t>(mode)));
  lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
  return button;
}

void overlayActionClicked(lv_event_t* event) {
  const auto action = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(action, currentPrinterId, currentAmsId, 0,
             static_cast<std::int32_t>(activeOverlayKind), 0, nullptr);
}

lv_obj_t* createOverlayButton(lv_obj_t* parent, const char* text,
                              std::int32_t x, std::uint32_t color,
                              rtos::UiActionType action) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, 158);
  lv_obj_set_size(button, 170, 50);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_ui_german16, LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(button, overlayActionClicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(
                          static_cast<std::uintptr_t>(action)));
  return button;
}

void ensureOverlay() {
  if (overlayBackdrop != nullptr) return;
  overlayBackdrop = lv_obj_create(lv_layer_top());
  lv_obj_set_pos(overlayBackdrop, 0, 0);
  lv_obj_set_size(overlayBackdrop, 480, 320);
  lv_obj_remove_flag(overlayBackdrop, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(overlayBackdrop, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(overlayBackdrop, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_width(overlayBackdrop, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(overlayBackdrop, 0, LV_PART_MAIN);

  overlayPanel = lv_obj_create(overlayBackdrop);
  lv_obj_set_size(overlayPanel, 420, 238);
  lv_obj_center(overlayPanel);
  lv_obj_remove_flag(overlayPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(overlayPanel, lv_color_hex(0xF4F6F8), LV_PART_MAIN);
  lv_obj_set_style_border_color(overlayPanel, lv_color_hex(0x1565C0), LV_PART_MAIN);
  lv_obj_set_style_border_width(overlayPanel, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(overlayPanel, 12, LV_PART_MAIN);
  lv_obj_set_style_text_color(overlayPanel, lv_color_hex(0x101820), LV_PART_MAIN);
  // Child coordinates define the complete overlay layout. Removing the
  // theme's implicit content padding keeps both action columns
  // inside the 420 px panel with equal 16 px margins.
  lv_obj_set_style_pad_all(overlayPanel, 0, LV_PART_MAIN);

  overlayTitle = lv_label_create(overlayPanel);
  lv_obj_set_pos(overlayTitle, 16, 12);
  lv_obj_set_size(overlayTitle, 388, 32);
  lv_obj_set_style_text_font(overlayTitle, &ui_font_ui_german16, LV_PART_MAIN);
  overlayText = lv_label_create(overlayPanel);
  lv_obj_set_pos(overlayText, 16, 52);
  lv_obj_set_size(overlayText, 388, 100);
  lv_label_set_long_mode(overlayText, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(overlayText, &ui_font_ui_german16, LV_PART_MAIN);

  overlayProgress = lv_bar_create(overlayPanel);
  lv_obj_set_pos(overlayProgress, 16, 126);
  lv_obj_set_size(overlayProgress, 388, 14);
  lv_bar_set_range(overlayProgress, 0, 100);
  lv_bar_set_value(overlayProgress, 60, LV_ANIM_OFF);

  overlayCancel = createOverlayButton(overlayPanel, "Abbrechen", 16, 0x607D8B,
                                      rtos::UiActionType::Cancel);
  overlayConfirm = createOverlayButton(overlayPanel, "Best\xC3\xA4tigen", 218,
                                       0x1565C0,
                                       rtos::UiActionType::Confirm);
  advancedModeButtons = {{
      createAdvancedModeButton("Gebrauchte Spule", 16, 50, 1),
      createAdvancedModeButton("Volle/neue Spule", 218, 50, 2),
      createAdvancedModeButton("Leergewicht", 16, 104, 3),
      createAdvancedModeButton("Ausgangsgewicht", 218, 104, 4),
  }};
  spoolPickerButtons = {{
      createSpoolPickerButton("#42 | PLA | 642 g Rest", 44, 42),
      createSpoolPickerButton("#51 | PETG | 811 g Rest", 88, 51),
      createSpoolPickerButton("#67 | PLA Multicolor | 504 g Rest", 132, 67),
      createSpoolPickerButton("#91 | PLA Basic | 997 g Rest", 176, 91),
  }};
  advancedInput = lv_textarea_create(overlayPanel);
  lv_obj_set_pos(advancedInput, 16, 44);
  lv_obj_set_size(advancedInput, 388, 38);
  lv_textarea_set_one_line(advancedInput, true);
  lv_textarea_set_max_length(advancedInput, 7);
  lv_obj_set_style_text_font(advancedInput, &ui_font_ui_german16, LV_PART_MAIN);
  lv_obj_add_flag(advancedInput, LV_OBJ_FLAG_HIDDEN);
  advancedKeyboard = lv_buttonmatrix_create(overlayPanel);
  lv_obj_set_pos(advancedKeyboard, 16, 86);
  lv_obj_set_size(advancedKeyboard, 388, 136);
  lv_buttonmatrix_set_map(advancedKeyboard, kAdvancedNumberMap);
  lv_obj_set_style_pad_all(advancedKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(advancedKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(advancedKeyboard, 2, LV_PART_MAIN);
  lv_obj_add_event_cb(advancedKeyboard, advancedKeyboardEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(advancedKeyboard, LV_OBJ_FLAG_HIDDEN);
}

void hideOverlay() {
  if (overlayBackdrop != nullptr) lv_obj_add_flag(overlayBackdrop, LV_OBJ_FLAG_HIDDEN);
  activeOverlayKind = rtos::UiOverlayKind::None;
  activeOverlayRequestId = 0;
}

void showOverlay(const rtos::UiCommand& command, bool progress) {
  ensureOverlay();
  // Restore the standard geometry first because the same overlay objects are
  // reused for every dialog.
  lv_obj_set_size(overlayPanel, 420, 238);
  lv_obj_center(overlayPanel);
  lv_obj_set_pos(overlayText, 16, 52);
  lv_obj_set_size(overlayText, 388, 100);
  lv_obj_set_pos(overlayCancel, 16, 158);
  lv_obj_set_size(overlayCancel, 170, 50);
  lv_obj_set_pos(overlayConfirm, 218, 158);
  lv_obj_set_size(overlayConfirm, 170, 50);
  if (command.overlayKind == rtos::UiOverlayKind::AdvancedWeightConfirmation ||
      command.overlayKind == rtos::UiOverlayKind::AdvancedWeightResult) {
    // Six separate summary lines need more vertical room than ordinary
    // messages. Keep the action buttons below the complete summary.
    lv_obj_set_size(overlayPanel, 420, 286);
    lv_obj_center(overlayPanel);
    lv_obj_set_pos(overlayText, 16, 46);
    lv_obj_set_size(overlayText, 388, 166);
    lv_obj_set_y(overlayCancel, 220);
    lv_obj_set_y(overlayConfirm, 220);
  }
  activeOverlayKind = command.overlayKind;
  activeOverlayRequestId = command.requestId;
  lv_label_set_text(overlayTitle, command.title);
  lv_label_set_text(overlayText, command.text);
  lv_obj_remove_flag(overlayBackdrop, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(overlayText, LV_OBJ_FLAG_HIDDEN);
  for (lv_obj_t* button : advancedModeButtons)
    lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
  for (lv_obj_t* button : spoolPickerButtons)
    lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(advancedInput, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(advancedKeyboard, LV_OBJ_FLAG_HIDDEN);
  if (command.overlayKind == rtos::UiOverlayKind::AdvancedWeightMode) {
    lv_obj_add_flag(overlayText, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t* button : advancedModeButtons)
      lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(overlayCancel, 0), "Abbrechen");
    lv_obj_move_foreground(overlayBackdrop);
    return;
  }
  if (command.overlayKind == rtos::UiOverlayKind::AdvancedWeightInput) {
    advancedInputMode = command.value;
    lv_obj_add_flag(overlayText, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(advancedInput, command.text);
    lv_obj_remove_flag(advancedInput, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(advancedKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlayBackdrop);
    return;
  }
  if (command.overlayKind == rtos::UiOverlayKind::SpoolPicker) {
    lv_obj_set_size(overlayPanel, 420, 286);
    lv_obj_center(overlayPanel);
    lv_obj_add_flag(overlayText, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(overlayCancel, 220);
    lv_obj_set_size(overlayCancel, 388, 50);
    lv_obj_set_x(overlayCancel, 16);
    lv_label_set_text(lv_obj_get_child(overlayCancel, 0), "Abbrechen");
    for (lv_obj_t* button : spoolPickerButtons)
      lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlayBackdrop);
    return;
  }
  if (progress) {
    lv_obj_remove_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(overlayCancel, 0), "Schlie\xC3\x9F" "en");
  } else {
    lv_obj_add_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    // Numeric input hides the standard buttons. Always restore the
    // close/cancel button when the following summary or result is displayed.
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    const bool confirmation =
        command.overlayKind == rtos::UiOverlayKind::Confirmation ||
        command.overlayKind == rtos::UiOverlayKind::RestartConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::WifiResetConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::QuickWeightConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::AdvancedWeightConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::TagDefinitionImport;
    if (confirmation) {
      lv_obj_remove_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(lv_obj_get_child(overlayCancel, 0), "Abbrechen");
    } else {
      lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(lv_obj_get_child(overlayCancel, 0), "Schlie\xC3\x9F" "en");
    }
  }
  lv_obj_move_foreground(overlayBackdrop);
}

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
      return "verbunden";
    case models::UiConnectionState::Offline:
      return "nicht verbunden";
    case models::UiConnectionState::Error:
      return "Fehler";
  }
  return "unbekannt";
}

lv_obj_t* firstLabelDescendant(lv_obj_t* object) {
  if (object == nullptr) return nullptr;
  const std::uint32_t childCount = lv_obj_get_child_count(object);
  for (std::uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t* child = lv_obj_get_child(object, static_cast<std::int32_t>(index));
    if (lv_obj_check_type(child, &lv_label_class)) return child;
    if (lv_obj_t* label = firstLabelDescendant(child); label != nullptr)
      return label;
  }
  return nullptr;
}

lv_obj_t* buttonLabel(lv_obj_t* button) {
  return firstLabelDescendant(button);
}

void setButtonText(lv_obj_t* button, const char* text) {
  lv_obj_t* label = buttonLabel(button);
  if (label != nullptr) {
    lv_label_set_text(label, text);
  }
}

void centerButtonLabel(lv_obj_t* button) {
  lv_obj_t* label = buttonLabel(button);
  if (label == nullptr || label == button) return;
  lv_obj_update_layout(button);
  lv_obj_set_width(label, lv_obj_get_width(button) - 8);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_center(label);
}

void setControlText(lv_obj_t* object, const char* text) {
  if (object == nullptr) return;
  if (lv_obj_check_type(object, &lv_label_class)) {
    lv_obj_t* label = buttonLabel(object);
    lv_label_set_text(label == nullptr ? object : label, text);
    centerButtonLabel(object);
  } else {
    setButtonText(object, text);
    centerButtonLabel(object);
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
                std::uint8_t amsId, std::uint8_t trayId,
                std::int32_t value, rtos::SpoolId spoolId,
                const char* text) {
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
  setControlText(objects.spoolman_setting_name, text);
  std::snprintf(text, sizeof(text), "Protokoll: %s",
                spoolmanDraft.protocol);
  setControlText(objects.spoolman_setting_protocol, text);
  std::snprintf(text, sizeof(text), "Host: %s", spoolmanDraft.host);
  setControlText(objects.spoolman_setting_host, text);
  std::snprintf(text, sizeof(text), "Port: %s", spoolmanDraft.port);
  setControlText(objects.spoolman_setting_port, text);
  std::snprintf(text, sizeof(text), "Basispfad: %s", spoolmanDraft.basePath);
  setControlText(objects.spoolman_setting_base_path, text);
  std::snprintf(text, sizeof(text), "Timeout: %s ms", spoolmanDraft.timeoutMs);
  setControlText(objects.spoolman_setting_timeout, text);
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
  } else if (std::strcmp(key, "Abbr.") == 0) {
    closeSpoolmanEditor();
  } else if (std::strcmp(key, "Entf.") == 0) {
    lv_textarea_delete_char(spoolmanEditor);
  } else if (std::strcmp(key, "<") == 0) {
    lv_textarea_cursor_left(spoolmanEditor);
  } else if (std::strcmp(key, ">") == 0) {
    lv_textarea_cursor_right(spoolmanEditor);
  } else if (std::strcmp(key, "Leer") == 0) {
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

void closeCalibrationEditor() {
  if (calibrationKeyboard != nullptr) {
    lv_obj_delete_async(calibrationKeyboard);
    calibrationKeyboard = nullptr;
  }
  if (calibrationEditor != nullptr) {
    lv_obj_delete_async(calibrationEditor);
    calibrationEditor = nullptr;
  }
}

void calibrationKeyboardEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
      calibrationEditor == nullptr || calibrationKeyboard == nullptr) return;
  const std::uint32_t button =
      lv_buttonmatrix_get_selected_button(calibrationKeyboard);
  if (button == LV_BUTTONMATRIX_BUTTON_NONE) return;
  const char* key = lv_buttonmatrix_get_button_text(calibrationKeyboard, button);
  if (key == nullptr) return;
  if (std::strcmp(key, "OK") == 0) {
    char* end = nullptr;
    const long grams =
        std::strtol(lv_textarea_get_text(calibrationEditor), &end, 10);
    if (end != nullptr && *end == '\0' && grams > 0 && grams <= 100000) {
      sendAction(rtos::UiActionType::StartScaleCalibration, currentPrinterId,
                 0, 0, static_cast<std::int32_t>(grams));
      closeCalibrationEditor();
    } else {
      lv_label_set_text(objects.scale_settings_calibration,
                        "Referenzgewicht: 1 bis 100000 g eingeben");
    }
  } else if (std::strcmp(key, "Abbr.") == 0) {
    closeCalibrationEditor();
  } else if (std::strcmp(key, "Entf.") == 0) {
    lv_textarea_delete_char(calibrationEditor);
  } else if (key[1] == '\0' && key[0] >= '0' && key[0] <= '9') {
    lv_textarea_add_text(calibrationEditor, key);
  }
}

void calibrationClicked(lv_event_t*) {
  closeCalibrationEditor();
  calibrationEditor = lv_textarea_create(objects.scr_settings_scale);
  lv_obj_set_pos(calibrationEditor, 8, 76);
  lv_obj_set_size(calibrationEditor, 464, 44);
  lv_textarea_set_one_line(calibrationEditor, true);
  lv_textarea_set_max_length(calibrationEditor, 6);
  lv_textarea_set_placeholder_text(calibrationEditor, "Referenzgewicht in g");
  lv_obj_set_style_text_font(calibrationEditor, &ui_font_ui_german16,
                             LV_PART_MAIN);
  calibrationKeyboard = lv_buttonmatrix_create(objects.scr_settings_scale);
  lv_obj_set_pos(calibrationKeyboard, 8, 124);
  lv_obj_set_size(calibrationKeyboard, 464, 132);
  lv_buttonmatrix_set_map(calibrationKeyboard, kKeyboardNumberMap);
  lv_obj_set_style_pad_all(calibrationKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(calibrationKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(calibrationKeyboard, 2, LV_PART_MAIN);
  lv_obj_add_event_cb(calibrationKeyboard, calibrationKeyboardEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_move_foreground(calibrationEditor);
  lv_obj_move_foreground(calibrationKeyboard);
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
    std::snprintf(printerUiDraft.name, sizeof(printerUiDraft.name), "A1 Mini Büro");
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
  setControlText(objects.printer_edit_name, text);
  std::snprintf(text, sizeof(text), "Host/IP: %s", printerUiDraft.host);
  setControlText(objects.printer_edit_host, text);
  std::snprintf(text, sizeof(text), "Seriennummer: %s", printerUiDraft.serial);
  setControlText(objects.printer_edit_serial, text);
  std::snprintf(text, sizeof(text), "LAN-Zugangscode: %s",
                showPrinterAccessCode ? printerUiDraft.accessCode : "********");
  setControlText(objects.printer_edit_access_code, text);
  setControlText(objects.printer_edit_mask,
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
                    entry.isActive ? " | ausgewählt" : "");
    }
    setControlText(rows[index], text);
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
  const auto& spool = models::mock::spool();
  char advancedData[64]{};
  const char* actionText = nullptr;
  if (type == rtos::UiActionType::QuickWeight) {
    actionText = spool.filament;
  } else if (type == rtos::UiActionType::AdvancedWeight) {
    std::snprintf(advancedData, sizeof(advancedData), "%s|%.1f|%.1f",
                  spool.filament, static_cast<double>(spool.emptyWeightGrams),
                  static_cast<double>(spool.initialWeightGrams));
    actionText = advancedData;
  }
  sendAction(type, currentPrinterId, 0, 0,
             type == rtos::UiActionType::QuickWeight
                 ? static_cast<std::int32_t>(spool.emptyWeightGrams)
                 : 0,
             models::mock::staging().spoolId,
             actionText);
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

void tagActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const rtos::SpoolId spoolId =
      (type == rtos::UiActionType::SelectSpool ||
       type == rtos::UiActionType::AssignTag)
          ? models::mock::staging().spoolId
          : 0;
  sendAction(type, currentPrinterId, 0, 0, 0, spoolId);
}

void lastTagSpoolClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::SelectSpool, currentPrinterId);
}

void makeDescendantsTouchTransparent(lv_obj_t* object) {
  const std::uint32_t childCount = lv_obj_get_child_count(object);
  for (std::uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t* child = lv_obj_get_child(object, static_cast<std::int32_t>(index));
    lv_obj_remove_flag(child, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(child, LV_OBJ_FLAG_SCROLLABLE);
    makeDescendantsTouchTransparent(child);
  }
}

void bindClick(lv_obj_t* object, lv_event_cb_t callback,
               std::uintptr_t userData = 0) {
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  makeDescendantsTouchTransparent(object);
  lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(userData));
}

void styleLabelButton(lv_obj_t* object, std::uint32_t color = 0x1565C0) {
  if (object == nullptr) return;
  if (lv_obj_check_type(object, &lv_label_class) &&
      lv_obj_get_child_count(object) == 0) {
    char text[128]{};
    std::snprintf(text, sizeof(text), "%s", lv_label_get_text(object));
    const lv_font_t* font = static_cast<const lv_font_t*>(
        lv_obj_get_style_text_font(object, LV_PART_MAIN));
    lv_label_set_text(object, "");
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);

    // EEZ uses labels as clickable buttons. Give the visible caption its own
    // content-sized label across the full button width (4 px margin). LVGL can
    // then center that complete one- or multi-line block reliably.
    lv_obj_t* caption = lv_label_create(object);
    lv_obj_set_width(caption, lv_obj_get_width(object) - 8);
    lv_obj_set_height(caption, LV_SIZE_CONTENT);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_WRAP);
    lv_label_set_text(caption, text);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(caption, 0, LV_PART_MAIN);
    if (font != nullptr)
      lv_obj_set_style_text_font(caption, font, LV_PART_MAIN);
    lv_obj_remove_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(caption, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(caption);
  }
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_color(object, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_radius(object, 8, LV_PART_MAIN);
  centerButtonLabel(object);
}

void setLabelButtonAvailable(lv_obj_t* object, bool available,
                             std::uint32_t activeColor) {
  lv_obj_set_flag(object, LV_OBJ_FLAG_CLICKABLE, available);
  lv_obj_set_style_bg_color(
      object, lv_color_hex(available ? activeColor : 0x616161), LV_PART_MAIN);
  lv_obj_set_style_text_color(
      object, lv_color_hex(available ? 0xFFFFFF : 0xD7DCE0), LV_PART_MAIN);
  if (lv_obj_get_child_count(object) > 0) {
    lv_obj_t* caption = lv_obj_get_child(object, 0);
    lv_obj_set_style_text_color(
        caption, lv_color_hex(available ? 0xFFFFFF : 0xD7DCE0), LV_PART_MAIN);
  }
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
  const std::array<lv_obj_t*, 25> screens{{
      objects.scr_boot, objects.scr_home, objects.scr_printer_select,
      objects.scr_settings_home, objects.scr_staging_details,
      objects.scr_staging_actions, objects.scr_tray_details,
      objects.scr_tray_actions, objects.scr_tray_select,
      objects.scr_settings_spoolman,
      objects.scr_settings_printers, objects.scr_settings_printer_edit,
      objects.scr_settings_wifi, objects.scr_settings_scale,
      objects.scr_settings_device, objects.scr_settings_diagnostics,
      objects.scr_settings_firmware,
      objects.scr_tag_action_select, objects.scr_tag_review,
      objects.scr_tag_write, objects.scr_tag_result,
      objects.scr_tag_definition_import, objects.scr_bambu_spool_type,
      objects.scr_tag_legacy, objects.scr_tag_unknown,
  }};
  for (lv_obj_t* screen : screens) {
    lv_obj_set_style_text_font(screen, &ui_font_ui_german16, LV_PART_MAIN);
  }
}

void bindGeneratedWidgets() {
  // EEZ identifiers remain stable; only the user-facing wording is localized.
  setControlText(objects.staging_details_quick_weight, "Schnellwiegen");
  setControlText(objects.staging_action_advanced_weight,
                 "Erweitertes Wiegen");
  setControlText(objects.tray_select_external, "Extern");
  setControlText(objects.tray_action_reset, "Slot zur\xC3\xBC" "cksetzen");
  setControlText(objects.scale_settings_reset,
                 "Zur\xC3\xBC" "cksetzen");
  constexpr lv_coord_t kScaleActionWidth = 115;
  constexpr lv_coord_t kScaleActionGap = 4;
  const std::array<lv_obj_t*, 4> scaleActionButtons{{
      objects.scale_settings_tare, objects.scale_settings_calibrate,
      objects.scale_settings_reset, objects.scale_settings_back,
  }};
  for (std::size_t index = 0; index < scaleActionButtons.size(); ++index) {
    lv_obj_set_pos(scaleActionButtons[index],
                   4 + static_cast<lv_coord_t>(index) *
                           (kScaleActionWidth + kScaleActionGap),
                   264);
    lv_obj_set_size(scaleActionButtons[index], kScaleActionWidth, 56);
  }
  const std::array<lv_obj_t*, 16> settingsControls{{
      objects.home_settings, objects.select_settings, objects.settings_settings,
      objects.staging_details_settings, objects.staging_actions_settings,
      objects.tray_details_settings, objects.tray_actions_settings,
      objects.tray_select_settings, objects.spoolman_settings_settings,
      objects.printer_settings_settings, objects.printer_edit_settings,
      objects.wifi_settings_settings, objects.scale_settings_settings,
      objects.device_settings_settings, objects.diagnostics_settings_settings,
      objects.firmware_settings_settings,
  }};
  for (lv_obj_t* control : settingsControls) setControlText(control, "Einst.");

  const std::array<lv_obj_t*, 6> tagSettings{{
      objects.tag_action_settings, objects.tag_review_settings,
      objects.tag_write_settings, objects.tag_result_settings,
      objects.tag_legacy_settings, objects.tag_unknown_settings,
  }};
  for (lv_obj_t* control : tagSettings) setControlText(control, "Einst.");
  setControlText(objects.tag_action_title, "NFC-Tag-Aktionen");
  setControlText(objects.tag_action_select_spool, "Spule ausw\xC3\xA4hlen");
  setControlText(objects.tag_action_use_last_spool,
                 "Zuletzt verwendete Spule");
  setControlText(objects.tag_action_write, "Tag schreiben");
  setControlText(objects.tag_action_erase, "Tag l\xC3\xB6schen");
  setControlText(objects.tag_action_back, "Zur\xC3\xBC" "ck");
  setControlText(objects.tag_review_title, "Tag pr\xC3\xBC" "fen");
  setControlText(objects.tag_review_back, "Zur\xC3\xBC" "ck");
  setControlText(objects.tag_review_cancel, "Abbrechen");
  setControlText(objects.tag_review_confirm, "Best\xC3\xA4tigen");
  setControlText(objects.tag_write_title, "Tag wird geschrieben");
  setControlText(objects.tag_write_detected, "Tag erkannt");
  setControlText(objects.tag_write_memory, "Speicher gepr\xC3\xBC" "ft");
  setControlText(objects.tag_write_data, "Daten werden geschrieben");
  setControlText(objects.tag_write_verify, "Daten werden verifiziert");
  setControlText(objects.tag_write_cancel, "Abbrechen");
  setControlText(objects.tag_result_title, "NFC-Ergebnis");
  setControlText(objects.tag_result_quick_weight, "Schnell wiegen");
  setControlText(objects.tag_result_advanced_weight, "Erweitert wiegen");
  setControlText(objects.tag_result_close, "Schlie\xC3\x9F" "en");
  setControlText(objects.tag_definition_import_settings, "Einst.");
  setControlText(objects.tag_definition_import_title,
                 "Tagdefinition erkannt");
  setControlText(objects.tag_definition_import_select_spool,
                 "Spule verbinden");
  setControlText(objects.tag_definition_import_spoolman,
                 "Nach Spoolman importieren");
  setControlText(objects.tag_definition_import_cancel, "Abbrechen");
  setControlText(objects.tag_legacy_title, "Legacy-Tag erkannt");
  setControlText(objects.tag_legacy_select_spool, "Spule verbinden");
  setControlText(objects.tag_legacy_import, "Nach Spoolman importieren");
  setControlText(objects.tag_legacy_migrate, "Nativ migrieren");
  setControlText(objects.tag_legacy_erase, "Tag l\xC3\xB6schen");
  setControlText(objects.tag_legacy_close, "Schlie\xC3\x9F" "en");
  setControlText(objects.tag_unknown_title, "Unbekannter NFC-Tag");
  setControlText(objects.tag_unknown_select_spool,
                 "UID mit Spule verbinden");
  setControlText(objects.tag_unknown_close, "Schlie\xC3\x9F" "en");
  setControlText(objects.bambu_spool_type_title, "Leergewicht ausw\xC3\xA4hlen");
  setControlText(objects.bambu_spool_type_back, "Zur\xC3\xBC" "ck");
  styleLabelButton(objects.tag_definition_import_select_spool, 0x1565C0);
  styleLabelButton(objects.tag_definition_import_spoolman, 0x1565C0);
  styleLabelButton(objects.tag_definition_import_cancel, 0x455A64);
  styleLabelButton(objects.tag_legacy_select_spool, 0x1565C0);
  styleLabelButton(objects.tag_legacy_import, 0x1565C0);
  styleLabelButton(objects.tag_legacy_migrate, 0x1565C0);
  styleLabelButton(objects.tag_legacy_erase, 0xC62828);
  styleLabelButton(objects.tag_legacy_close, 0x455A64);
  styleLabelButton(objects.tag_unknown_select_spool, 0x1565C0);
  styleLabelButton(objects.tag_unknown_close, 0x455A64);
  styleLabelButton(objects.bambu_spool_type_low, 0x1565C0);
  styleLabelButton(objects.bambu_spool_type_high, 0x1565C0);
  styleLabelButton(objects.bambu_spool_type_manual, 0x1565C0);
  styleLabelButton(objects.bambu_spool_type_back, 0x455A64);

  bindClick(objects.tag_action_header, headerClicked);
  bindClick(objects.tag_review_header, headerClicked);
  bindClick(objects.tag_write_header, headerClicked);
  bindClick(objects.tag_result_header, headerClicked);
  for (lv_obj_t* control : tagSettings) bindClick(control, settingsClicked);
  bindClick(objects.tag_action_select_spool, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SearchSpool));
  bindClick(objects.tag_action_use_last_spool, lastTagSpoolClicked);
  bindClick(objects.tag_action_write, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.tag_action_erase, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.tag_action_back, backClicked);
  bindClick(objects.tag_review_back, backClicked);
  bindClick(objects.tag_review_cancel, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));
  bindClick(objects.tag_review_confirm, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Confirm));
  bindClick(objects.tag_write_cancel, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));
  bindClick(objects.tag_result_quick_weight, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::QuickWeight));
  bindClick(objects.tag_result_advanced_weight, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AdvancedWeight));
  bindClick(objects.tag_result_close, backClicked);
  bindClick(objects.tag_definition_import_header, headerClicked);
  bindClick(objects.tag_definition_import_settings, settingsClicked);
  bindClick(objects.tag_definition_import_select_spool, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SearchSpool));
  bindClick(objects.tag_definition_import_spoolman, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_definition_import_cancel, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));
  bindClick(objects.tag_legacy_header, headerClicked);
  bindClick(objects.tag_legacy_select_spool, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SearchSpool));
  bindClick(objects.tag_legacy_import, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_legacy_migrate, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.tag_legacy_erase, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.tag_legacy_close, backClicked);
  bindClick(objects.tag_unknown_header, headerClicked);
  bindClick(objects.tag_unknown_select_spool, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SearchSpool));
  bindClick(objects.tag_unknown_close, backClicked);
  bindClick(objects.bambu_spool_type_back, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));

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
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.staging_action_link_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.staging_action_unlink_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.staging_action_erase_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));

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

  bindClick(objects.wifi_settings_header, headerClicked);
  bindClick(objects.wifi_settings_settings, settingsClicked);
  bindClick(objects.wifi_settings_portal, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::StartWifiPortal));
  bindClick(objects.wifi_settings_reset, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ResetWifiCredentials));
  bindClick(objects.wifi_settings_back, backClicked);
  bindClick(objects.scale_settings_header, headerClicked);
  bindClick(objects.scale_settings_settings, settingsClicked);
  bindClick(objects.scale_settings_tare, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::TareScale));
  bindClick(objects.scale_settings_calibrate, calibrationClicked);
  bindClick(objects.scale_settings_reset, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ResetScaleCalibration));
  bindClick(objects.scale_settings_back, backClicked);
  bindClick(objects.device_settings_header, headerClicked);
  bindClick(objects.device_settings_settings, settingsClicked);
  bindClick(objects.device_settings_restart, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::PrepareRestart));
  bindClick(objects.device_settings_back, backClicked);
  bindClick(objects.diagnostics_settings_header, headerClicked);
  bindClick(objects.diagnostics_settings_settings, settingsClicked);
  bindClick(objects.diagnostics_settings_refresh, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::RefreshDiagnostics));
  bindClick(objects.diagnostics_settings_back, backClicked);
  bindClick(objects.firmware_settings_header, headerClicked);
  bindClick(objects.firmware_settings_settings, settingsClicked);
  bindClick(objects.firmware_settings_check, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::CheckFirmwareUpdate));
  bindClick(objects.firmware_settings_back, backClicked);

  const std::array<lv_obj_t*, 17> additionalSettingsButtons{{
      objects.wifi_settings_header, objects.wifi_settings_settings,
      objects.wifi_settings_portal, objects.wifi_settings_reset,
      objects.scale_settings_header, objects.scale_settings_settings,
      objects.scale_settings_tare, objects.scale_settings_calibrate,
      objects.scale_settings_reset, objects.device_settings_header,
      objects.device_settings_settings, objects.device_settings_restart,
      objects.diagnostics_settings_header, objects.diagnostics_settings_settings,
      objects.diagnostics_settings_refresh, objects.firmware_settings_header,
      objects.firmware_settings_settings,
  }};
  for (lv_obj_t* button : additionalSettingsButtons) styleLabelButton(button);
  styleLabelButton(objects.firmware_settings_check);
  styleLabelButton(objects.wifi_settings_back, 0x455A64);
  styleLabelButton(objects.scale_settings_back, 0x455A64);
  styleLabelButton(objects.device_settings_back, 0x455A64);
  styleLabelButton(objects.diagnostics_settings_back, 0x455A64);
  styleLabelButton(objects.firmware_settings_back, 0x455A64);

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
  const std::array<lv_obj_t*, 9> tagButtons{{
      objects.tag_action_select_spool, objects.tag_action_use_last_spool,
      objects.tag_action_write, objects.tag_review_confirm,
      objects.tag_result_quick_weight, objects.tag_result_advanced_weight,
      objects.tag_action_back, objects.tag_review_back,
      objects.tag_result_close,
  }};
  for (lv_obj_t* button : tagButtons) styleLabelButton(button);
  styleLabelButton(objects.tag_action_erase, 0xC62828);
  styleLabelButton(objects.tag_review_cancel, 0x455A64);
  styleLabelButton(objects.tag_write_cancel, 0x455A64);

  // Every enabled navigation button labelled "Zurück" uses the primary blue
  // action style.  Cancel/close buttons intentionally keep their secondary
  // grey style, but must not determine the appearance of actual back buttons.
  const std::array<lv_obj_t*, 13> activeBackButtons{{
      objects.select_back,
      objects.settings_back,
      objects.staging_actions_back,
      objects.tray_actions_back,
      objects.printer_settings_back,
      objects.wifi_settings_back,
      objects.scale_settings_back,
      objects.device_settings_back,
      objects.diagnostics_settings_back,
      objects.firmware_settings_back,
      objects.tag_action_back,
      objects.tag_review_back,
      objects.bambu_spool_type_back,
  }};
  for (lv_obj_t* button : activeBackButtons) styleLabelButton(button);
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
  setControlText(objects.select_bottom_status, "Drucker verwalten");
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
                   "Extern", 4);

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

  updateWeightDisplays();

  const auto& settings = models::mock::settings();
  char statusText[96];
  std::snprintf(statusText, sizeof(statusText),
                "NFC: %s\nSpoolman: %s\nWLAN: %s", staging.nfcStatus,
                connectionText(settings.spoolmanState),
                connectionText(settings.wifiState));
  setButtonText(objects.home_status, statusText);
}

void updateWeightDisplays() {
  const models::UiWeightState& weight = liveWeight;
  char text[96];
  if (weight.error) {
    std::snprintf(text, sizeof(text), "Waage\nFehler\n%s", weight.status);
    setButtonColors(objects.home_weight, 0xC62828);
  } else if (!weight.calibrated) {
    std::snprintf(text, sizeof(text), "Waage\n-- g\nnicht kalibriert");
    setButtonColors(objects.home_weight, 0xF9A825);
  } else {
    std::snprintf(text, sizeof(text), "Waage\n%.1f g\n%s",
                  static_cast<double>(weight.grossWeightGrams), weight.status);
    setButtonColors(objects.home_weight, weight.stable ? 0x2E7D32 : 0xF9A825);
  }
  setButtonText(objects.home_weight, text);

  if (weight.error) {
    std::snprintf(text, sizeof(text), "Gewicht: Fehler - %s", weight.status);
  } else if (!weight.calibrated) {
    std::snprintf(text, sizeof(text), "Gewicht: -- g | nicht kalibriert");
  } else {
    std::snprintf(text, sizeof(text), "Gewicht: %.1f g | %s",
                  static_cast<double>(weight.grossWeightGrams), weight.status);
  }
  lv_label_set_text(objects.scale_settings_weight, text);
  lv_obj_set_style_text_color(objects.scale_settings_weight,
                              lv_color_hex(weight.error ? 0xC62828
                                                        : (weight.stable
                                                               ? 0x2E7D32
                                                               : 0xB26A00)),
                              LV_PART_MAIN);
  std::snprintf(text, sizeof(text), "Kalibrierung: %s",
                weight.calibrated ? "geladen" : "nicht vorhanden");
  lv_label_set_text(objects.scale_settings_calibration, text);
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
    std::snprintf(title, sizeof(title), "Externer Slot");
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
    setControlText(amsButtons[id - 1U], label);
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
    setControlText(slotButtons[id], label);
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
    std::snprintf(summary, sizeof(summary), "Ziel: Drucker %u | Extern | Spule %lu",
                  currentPrinterId, static_cast<unsigned long>(staging.spoolId));
  } else {
    std::snprintf(summary, sizeof(summary),
                  "Ziel: Drucker %u | AMS %u | Slot %u | Spule %lu",
                  currentPrinterId, selectedTrayAmsId, selectedTrayId + 1U,
                  static_cast<unsigned long>(staging.spoolId));
  }
  lv_label_set_text(objects.tray_select_summary, summary);
}

void setAllHeaderTexts(const char* text) {
  const std::array<lv_obj_t*, 23> headers{{
      objects.home_header,
      objects.select_header,
      objects.settings_header,
      objects.staging_details_header,
      objects.staging_actions_header,
      objects.tray_details_header,
      objects.tray_actions_header,
      objects.tray_select_header,
      objects.spoolman_settings_header,
      objects.printer_settings_header,
      objects.printer_edit_header,
      objects.wifi_settings_header,
      objects.scale_settings_header,
      objects.device_settings_header,
      objects.diagnostics_settings_header,
      objects.firmware_settings_header,
      objects.tag_action_header,
      objects.tag_review_header,
      objects.tag_write_header,
      objects.tag_result_header,
      objects.tag_definition_import_header,
      objects.tag_legacy_header,
      objects.tag_unknown_header,
  }};
  for (lv_obj_t* header : headers) setControlText(header, text);
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

  setAllHeaderTexts(header);

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
  setAllHeaderTexts(header);
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
    case rtos::UiScreenId::SettingsWifi:
      loadScreen(SCREEN_ID_SCR_SETTINGS_WIFI);
      break;
    case rtos::UiScreenId::SettingsScale:
      closeCalibrationEditor();
      updateWeightDisplays();
      loadScreen(SCREEN_ID_SCR_SETTINGS_SCALE);
      break;
    case rtos::UiScreenId::SettingsDevice:
      loadScreen(SCREEN_ID_SCR_SETTINGS_DEVICE);
      break;
    case rtos::UiScreenId::SettingsDiagnostics: {
      char text[72];
      std::snprintf(text, sizeof(text), "Heap: %lu Bytes frei",
                    static_cast<unsigned long>(ESP.getFreeHeap()));
      lv_label_set_text(objects.diagnostics_settings_heap, text);
      std::snprintf(text, sizeof(text), "PSRAM: %lu Bytes frei",
                    static_cast<unsigned long>(ESP.getFreePsram()));
      lv_label_set_text(objects.diagnostics_settings_psram, text);
      loadScreen(SCREEN_ID_SCR_SETTINGS_DIAGNOSTICS);
      break;
    }
    case rtos::UiScreenId::SettingsFirmware:
      loadScreen(SCREEN_ID_SCR_SETTINGS_FIRMWARE);
      break;
    case rtos::UiScreenId::TagActionSelect:
      loadScreen(SCREEN_ID_SCR_TAG_ACTION_SELECT);
      break;
    case rtos::UiScreenId::TagReview:
      loadScreen(SCREEN_ID_SCR_TAG_REVIEW);
      break;
    case rtos::UiScreenId::TagWrite:
      loadScreen(SCREEN_ID_SCR_TAG_WRITE);
      break;
    case rtos::UiScreenId::TagResult:
      loadScreen(SCREEN_ID_SCR_TAG_RESULT);
      break;
    case rtos::UiScreenId::TagDefinitionImport:
      loadScreen(SCREEN_ID_SCR_TAG_DEFINITION_IMPORT);
      break;
    case rtos::UiScreenId::TagLegacy:
      loadScreen(SCREEN_ID_SCR_TAG_LEGACY);
      break;
    case rtos::UiScreenId::TagUnknown:
      loadScreen(SCREEN_ID_SCR_TAG_UNKNOWN);
      break;
    case rtos::UiScreenId::BambuSpoolType:
      loadScreen(SCREEN_ID_SCR_BAMBU_SPOOL_TYPE);
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
  lv_mem_monitor_t memoryMonitor{};
  lv_mem_monitor(&memoryMonitor);
  rtos::logf("UiTask: LVGL memory free=%lu bytes, largest=%lu bytes, fragmentation=%u%%",
             static_cast<unsigned long>(memoryMonitor.free_size),
             static_cast<unsigned long>(memoryMonitor.free_biggest_size),
             static_cast<unsigned>(memoryMonitor.frag_pct));
  vTaskDelay(pdMS_TO_TICKS(250));
  loadScreen(SCREEN_ID_SCR_BOOT);

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
    case rtos::UiCommandType::ShowProgress:
      showOverlay(command, true);
      break;
    case rtos::UiCommandType::ShowDialog:
      showOverlay(command, false);
      break;
    case rtos::UiCommandType::HideProgress:
      hideOverlay();
      break;
    case rtos::UiCommandType::ShowScreen:
      if (command.screenId == rtos::UiScreenId::StagingActions) {
        setLabelButtonAvailable(
            objects.staging_action_write_tag,
            (command.value & rtos::UI_TAG_CAP_WRITE) != 0, 0x1565C0);
        setLabelButtonAvailable(
            objects.staging_action_link_tag,
            (command.value & rtos::UI_TAG_CAP_LINK) != 0, 0x1565C0);
        setLabelButtonAvailable(
            objects.staging_action_unlink_tag,
            (command.value & rtos::UI_TAG_CAP_UNLINK) != 0, 0x1565C0);
        setLabelButtonAvailable(
            objects.staging_action_erase_tag,
            (command.value & rtos::UI_TAG_CAP_ERASE) != 0, 0xC62828);
      }
      if (command.screenId == rtos::UiScreenId::TagActionSelect &&
          command.text[0] != '\0') {
        lv_label_set_text(objects.tag_action_info, command.text);
      } else if (command.screenId == rtos::UiScreenId::TagReview &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_review_summary, command.text);
      } else if (command.screenId == rtos::UiScreenId::TagResult &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_result_message, command.text);
      } else if (command.screenId == rtos::UiScreenId::TagDefinitionImport &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_definition_import_summary, command.text);
      } else if (command.screenId == rtos::UiScreenId::TagLegacy &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_legacy_summary, command.text);
        const bool writable = command.value != 0;
        lv_obj_set_flag(objects.tag_legacy_migrate, LV_OBJ_FLAG_CLICKABLE,
                        writable);
        lv_obj_set_flag(objects.tag_legacy_erase, LV_OBJ_FLAG_CLICKABLE,
                        writable);
        lv_obj_set_style_bg_color(objects.tag_legacy_migrate,
                                  lv_color_hex(writable ? 0x1565C0 : 0x616161),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_color(objects.tag_legacy_erase,
                                  lv_color_hex(writable ? 0xC62828 : 0x616161),
                                  LV_PART_MAIN);
      } else if (command.screenId == rtos::UiScreenId::TagUnknown &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_unknown_summary, command.text);
      }
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
    case rtos::UiCommandType::UpdateWeight:
      liveWeight.grossWeightGrams = command.weightGrams;
      liveWeight.netWeightGrams = command.weightGrams;
      liveWeight.stable = (command.value & 1) != 0;
      liveWeight.calibrated = (command.value & 2) != 0;
      liveWeight.error = (command.value & 4) != 0;
      std::snprintf(liveWeight.status, sizeof(liveWeight.status), "%s",
                    command.text);
      updateWeightDisplays();
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
                            "Status: geändert, nicht gespeichert");
        } else {
          updateSpoolmanSettingsContent();
          lv_label_set_text(objects.spoolman_setting_status,
                            "Status: geändert, nicht gespeichert");
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
      if (command.value ==
          300 + static_cast<std::int32_t>(rtos::UiActionType::StartWifiPortal) ||
          command.value == 300 + static_cast<std::int32_t>(
                                     rtos::UiActionType::ResetWifiCredentials)) {
        lv_label_set_text(objects.wifi_settings_status, command.text);
      } else if (command.value ==
                     300 + static_cast<std::int32_t>(rtos::UiActionType::TareScale) ||
                 command.value == 300 + static_cast<std::int32_t>(
                                            rtos::UiActionType::StartScaleCalibration) ||
                 command.value == 300 + static_cast<std::int32_t>(
                                            rtos::UiActionType::ResetScaleCalibration)) {
        lv_label_set_text(objects.scale_settings_calibration, command.text);
      } else if (command.value == 300 + static_cast<std::int32_t>(
                                            rtos::UiActionType::PrepareRestart)) {
        lv_label_set_text(objects.device_settings_storage, command.text);
      } else if (command.value == 300 + static_cast<std::int32_t>(
                                            rtos::UiActionType::RefreshDiagnostics)) {
        char text[72];
        std::snprintf(text, sizeof(text), "Heap: %lu Bytes frei",
                      static_cast<unsigned long>(ESP.getFreeHeap()));
        lv_label_set_text(objects.diagnostics_settings_heap, text);
        std::snprintf(text, sizeof(text), "PSRAM: %lu Bytes frei",
                      static_cast<unsigned long>(ESP.getFreePsram()));
        lv_label_set_text(objects.diagnostics_settings_psram, text);
        lv_label_set_text(objects.diagnostics_settings_tasks,
                          "Tasks: 8 | Diagnose aktualisiert");
      } else if (command.value == 300 + static_cast<std::int32_t>(
                                            rtos::UiActionType::CheckFirmwareUpdate)) {
        lv_label_set_text(objects.firmware_settings_status, command.text);
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
