#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <soc/soc_memory_types.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdio>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "drivers/DisplayDriver.h"
#include "drivers/TouchDriver.h"
#include "services/Logger.h"
#include "ui/generated/images.h"
#include "ui/generated/ui.h"
#include "ui/models/MockUiDataProvider.h"

extern "C" {
extern const lv_font_t ui_font_ui_german16;
}

namespace filament_station::ui {
namespace {

// GUI-Farbpalette (Nutzerwunsch, 2026-08-22): jede in der UI verwendete
// Farbe als benannte, kommentierte Konstante statt als verstreutes
// 0xRRGGBB-Literal. Werte unveraendert aus dem bisherigen Code uebernommen,
// nur benannt/dokumentiert.
constexpr std::uint32_t kColorPrimaryBlue = 0x1565C0;    // Primaerfarbe: aktive/verfuegbare Buttons, "Zurueck"-Aktionen, Standardfarbe
constexpr std::uint32_t kColorNeutralGrey = 0x455A64;    // Sekundaerfarbe: Abbrechen/neutrale Buttons, leerer Slot/keine Daten
constexpr std::uint32_t kColorDisabledGrey = 0x616161;   // Deaktiviert/nicht verfuegbar (AMS-Button, freier Druckerplatz, ...)
constexpr std::uint32_t kColorDangerRed = 0xC62828;      // Destruktive Aktion (Loeschen/Entfernen/Leeren), Fehlerzustand
constexpr std::uint32_t kColorSuccessGreen = 0x2E7D32;   // Erfolg: Drucker verbunden, Gewicht stabil
constexpr std::uint32_t kColorWarningAmber = 0xF9A825;   // Warnung/Hervorhebung: ausgewaehltes AMS (Rand), Gewicht nicht stabil
constexpr std::uint32_t kColorWarningAmberDark = 0xB26A00;  // Textvariante zu kColorWarningAmber auf hellem Grund
constexpr std::uint32_t kColorManagedPrinterOrange = 0xEF6C00;  // Hervorhebung: der in den Druckereinstellungen aktuell bearbeitete Drucker
constexpr std::uint32_t kColorTextWhite = 0xFFFFFF;      // Text/Rahmen auf dunklem Grund
constexpr std::uint32_t kColorTextDark = 0x101820;       // Text/Rahmen auf hellem Grund
constexpr std::uint32_t kColorTextDisabled = 0xD7DCE0;   // Beschriftung auf deaktivierten Buttons
constexpr std::uint32_t kColorPanelLight = 0xECEFF1;     // Helle Panel-/Listenzeilen-Hintergruende
constexpr std::uint32_t kColorPanelLightAlt = 0xB8BDC0;  // Abwechselnde (ungerade) Tabellenzeile
constexpr std::uint32_t kColorPanelLightest = 0xF4F6F8;  // Overlay-Panel-Hintergrund
constexpr std::uint32_t kColorOverlayBackdrop = 0x000000;   // Abgedunkelter Hintergrund hinter Overlays/Dialogen
constexpr std::uint32_t kColorSpoolPickerRow = 0xB0BEC5;    // Default-Hintergrund einer Zeile im Spulen-Picker
constexpr std::uint32_t kColorOverlayCancelButton = 0x607D8B;  // Neutrale Abbrechen-Schaltflaeche im Overlay
constexpr std::uint32_t kColorInactivePrinterRow = 0x78909C;   // Deaktivierter/nicht existierender Drucker in Einstellungslisten
constexpr std::uint32_t kColorAmsButtonBackground = 0x263238;  // Neutraler Hintergrund der Home-AMS-Buttons (Farbe kommt aus den Slot-Feldern)
// Touch-Kalibrierungsmarker (Diagnose-Screen): 5 gut unterscheidbare Farben im Zyklus.
constexpr std::uint32_t kColorTouchMarker1 = 0xFFEB3B;
constexpr std::uint32_t kColorTouchMarker2 = 0x00E676;
constexpr std::uint32_t kColorTouchMarker3 = 0x00BCD4;
constexpr std::uint32_t kColorTouchMarker4 = 0xFF4081;
constexpr std::uint32_t kColorTouchMarker5 = 0xFF9100;

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
  models::UiConnectionState connectionState = models::UiConnectionState::Offline;
  std::uint8_t amsCount = 0;
};
// Empty by default (matches the empty bambu.json roster, Phase 8.2); real
// entries arrive from AppTask via UpdatePrinterList (value >= 100, see
// AppTask::syncPrinterEntryToUi) after boot-load and after every CRUD
// action, so this no longer pre-seeds fake printers.
std::array<PrinterUiEntry, 4> printerEntries{{
    {1, "", true, false, false, false},
    {2, "", true, false, false, false},
    {3, "", true, false, false, false},
    {4, "", true, false, false, false},
}};
struct AmsUiEntry {
  bool present = false;
  std::uint8_t occupiedTrayCount = 0;
};
// Real AMS/tray data synced from AppTask (see AppTask::syncAmsToUi), fed via
// UpdateAmsOverview/UpdateTrayDetails; empty by default (no AMS present)
// instead of the former static MockUiDataProvider data.
std::array<AmsUiEntry, 4> amsEntries{};
struct TrayUiEntry {
  bool occupied = false;
  char material[12]{};
  char colorHex[9]{};
  rtos::SpoolId spoolId = 0;
};
std::array<std::array<TrayUiEntry, 4>, 4> trayEntries{};
TrayUiEntry externalTrayEntry{};
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
models::UiStagingSummary stagingState{};
models::UiSpoolSummary stagingSpoolState{};
lv_obj_t* calibrationEditor = nullptr;
lv_obj_t* calibrationKeyboard = nullptr;
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
constexpr std::size_t kMaximumSpoolPickerResults = 20;
constexpr std::int32_t kSpoolPickerRowWidth = 444;
constexpr std::int32_t kSpoolPickerRowHeight = 46;
constexpr std::int32_t kSpoolPickerRowPitch = 48;
std::array<lv_obj_t*, kMaximumSpoolPickerResults> spoolPickerButtons{};
std::array<std::array<lv_obj_t*,
                      filament_station::models::SpoolmanSpool::kMaximumColors>,
           kMaximumSpoolPickerResults>
    spoolPickerColorPanels{};
std::array<lv_obj_t*, kMaximumSpoolPickerResults> spoolPickerLabels{};
std::array<rtos::SpoolId, kMaximumSpoolPickerResults> spoolPickerIds{};
lv_obj_t* spoolPickerSearch = nullptr;
lv_obj_t* spoolPickerFilterButton = nullptr;
lv_obj_t* spoolPickerList = nullptr;
lv_obj_t* spoolPickerScrollUp = nullptr;
lv_obj_t* spoolPickerScrollDown = nullptr;
lv_obj_t* spoolPickerKeyboard = nullptr;
std::uint8_t spoolPickerFilter = 0;
bool spoolPickerInputActive = false;
lv_obj_t* advancedInput = nullptr;
lv_obj_t* advancedKeyboard = nullptr;
std::int32_t advancedInputMode = 0;
rtos::UiOverlayKind activeOverlayKind = rtos::UiOverlayKind::None;
std::uint32_t activeOverlayRequestId = 0;
filament_station::models::SpoolmanAppState spoolmanAppState =
    filament_station::models::SpoolmanAppState::SpoolmanUnavailable;
bool currentTagCanAssign = false;
bool currentTagCanRemove = false;
bool diagnosticsRefreshedThisSession = false;
// Real WLAN/NFC connection status for the Home status summary -- previously
// this read from models::mock::settings() (always hardcoded "Connected")
// for WLAN/Spoolman, and stagingState.nfcStatus (never written anywhere)
// for NFC, so the whole summary was either fake or blank regardless of
// actual state.
rtos::UiNetworkState currentNetworkState = rtos::UiNetworkState::Offline;
char currentNfcStatusText[48] = "wird initialisiert";

bool sendAction(rtos::UiActionType type, rtos::PrinterId printerId,
                std::uint8_t amsId = 0, std::uint8_t trayId = 0,
                std::int32_t value = 0, rtos::SpoolId spoolId = 0,
                const char* text = nullptr);
void updateWeightDisplays();
void updateTraySelection(rtos::PrinterId printerId, std::uint8_t amsId,
                         std::uint8_t trayId, bool selected);

constexpr const char* kAdvancedNumberMap[] = {
    "1", "2", "3", "Entf.", "\n", "4", "5", "6", "Abbr.", "\n",
    "7", "8", "9", "OK", "\n", "0", ".", ""};

void advancedModeClicked(lv_event_t* event) {
  const auto mode = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::AdvancedWeight, currentPrinterId, 0, 0, mode);
}

void spoolPickerItemClicked(lv_event_t* event) {
  const auto index = static_cast<std::size_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  if (index >= spoolPickerIds.size() || spoolPickerIds[index] == 0) return;
  const rtos::SpoolId spoolId = spoolPickerIds[index];
  sendAction(rtos::UiActionType::SelectSpool, currentPrinterId, 0, 0, 0,
             spoolId);
}

bool parseSpoolPickerColor(const char* hex, std::uint32_t& rgb) {
  if (hex == nullptr) return false;
  while (*hex == '#') ++hex;
  if (std::strlen(hex) != 6 && std::strlen(hex) != 8) return false;
  char value[7]{};
  std::memcpy(value, hex, 6);
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 16);
  if (end == nullptr || *end != '\0') return false;
  rgb = static_cast<std::uint32_t>(parsed);
  return true;
}

void applySpoolPickerColors(std::size_t index,
                            const rtos::UiCommand& command) {
  if (index >= spoolPickerButtons.size()) return;
  std::array<std::uint32_t,
             filament_station::models::SpoolmanSpool::kMaximumColors>
      colors{};
  std::uint8_t count = 0;
  const std::uint8_t requested =
      std::min(command.spoolColorCount,
               filament_station::models::SpoolmanSpool::kMaximumColors);
  for (std::uint8_t color = 0; color < requested; ++color) {
    if (parseSpoolPickerColor(command.spoolColorHex[color], colors[count]))
      ++count;
  }
  lv_obj_set_style_bg_color(spoolPickerButtons[index],
                            lv_color_hex(kColorSpoolPickerRow), LV_PART_MAIN);
  for (lv_obj_t* panel : spoolPickerColorPanels[index])
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  if (count == 0) return;
  for (std::uint8_t color = 0; color < count; ++color) {
    lv_obj_t* panel = spoolPickerColorPanels[index][color];
    const std::int32_t left = (kSpoolPickerRowWidth * color) / count;
    const std::int32_t right = (kSpoolPickerRowWidth * (color + 1)) / count;
    lv_obj_set_pos(panel, left, 0);
    lv_obj_set_size(panel, right - left, kSpoolPickerRowHeight);
    lv_obj_set_style_bg_color(panel, lv_color_hex(colors[color]), LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_move_foreground(spoolPickerLabels[index]);
}

lv_obj_t* createSpoolPickerButton(const char* text, std::int32_t y,
                                  std::size_t index) {
  lv_obj_t* button = lv_button_create(spoolPickerList);
  lv_obj_set_pos(button, 0, y);
  lv_obj_set_size(button, kSpoolPickerRowWidth, kSpoolPickerRowHeight);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorSpoolPickerRow), LV_PART_MAIN);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
  for (std::size_t color = 0;
       color < filament_station::models::SpoolmanSpool::kMaximumColors;
       ++color) {
    lv_obj_t* panel = lv_obj_create(button);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_size(panel, kSpoolPickerRowWidth, kSpoolPickerRowHeight);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    spoolPickerColorPanels[index][color] = panel;
  }
  lv_obj_t* label = lv_label_create(button);
  // Keep a wide color frame plus a segmented color band below the caption.
  lv_obj_set_pos(label, 16, 11);
  lv_obj_set_size(label, kSpoolPickerRowWidth - 32, 24);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_ui_german16, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(kColorTextWhite), LV_PART_MAIN);
  lv_obj_set_style_bg_color(label, lv_color_hex(kColorTextDark), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(label, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_radius(label, 5, LV_PART_MAIN);
  lv_obj_set_style_pad_left(label, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_left(label, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_right(label, 6, LV_PART_MAIN);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_SCROLLABLE);
  spoolPickerLabels[index] = label;
  lv_obj_add_event_cb(
      button, spoolPickerItemClicked, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<std::uintptr_t>(index)));
  lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
  return button;
}

void submitSpoolPickerSearch() {
  if (spoolPickerSearch == nullptr ||
      lv_textarea_get_text(spoolPickerSearch)[0] == '\0')
    return;
  sendAction(rtos::UiActionType::SearchSpool, currentPrinterId, 0, 0,
             10 + spoolPickerFilter, 0,
             lv_textarea_get_text(spoolPickerSearch));
}

void spoolPickerScrollClicked(lv_event_t* event) {
  if (spoolPickerList == nullptr) return;
  const auto direction = static_cast<std::int32_t>(
      reinterpret_cast<std::intptr_t>(lv_event_get_user_data(event)));
  lv_obj_scroll_by(spoolPickerList, 0, direction * (2 * kSpoolPickerRowPitch),
                   LV_ANIM_ON);
}

void spoolPickerFilterChanged(lv_event_t*) {
  if (spoolPickerFilterButton != nullptr)
    spoolPickerFilter = static_cast<std::uint8_t>(
        lv_dropdown_get_selected(spoolPickerFilterButton));
}

void setSpoolPickerInputMode(bool active) {
  spoolPickerInputActive = active;
  if (active) {
    lv_obj_add_flag(spoolPickerList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolPickerFilterButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolPickerScrollUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolPickerScrollDown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(spoolPickerKeyboard);
  } else {
    lv_obj_remove_flag(spoolPickerList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerFilterButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerScrollUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerScrollDown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolPickerKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void spoolPickerKeyboardEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
      spoolPickerKeyboard == nullptr || spoolPickerSearch == nullptr)
    return;
  const std::uint32_t button =
      lv_buttonmatrix_get_selected_button(spoolPickerKeyboard);
  if (button == LV_BUTTONMATRIX_BUTTON_NONE) return;
  const char* key =
      lv_buttonmatrix_get_button_text(spoolPickerKeyboard, button);
  if (key == nullptr) return;
  if (std::strcmp(key, "OK") == 0) {
    setSpoolPickerInputMode(false);
    submitSpoolPickerSearch();
  } else if (std::strcmp(key, "Abbr.") == 0) {
    setSpoolPickerInputMode(false);
  } else if (std::strcmp(key, "Entf.") == 0) {
    lv_textarea_delete_char(spoolPickerSearch);
  } else if (std::strcmp(key, "<") == 0) {
    lv_textarea_cursor_left(spoolPickerSearch);
  } else if (std::strcmp(key, ">") == 0) {
    lv_textarea_cursor_right(spoolPickerSearch);
  } else if (std::strcmp(key, "Leer") == 0) {
    lv_textarea_add_char(spoolPickerSearch, ' ');
  } else if (std::strcmp(key, "ABC") == 0) {
    lv_buttonmatrix_set_map(spoolPickerKeyboard, kKeyboardUpperMap);
  } else if (std::strcmp(key, "abc") == 0) {
    lv_buttonmatrix_set_map(spoolPickerKeyboard, kKeyboardLowerMap);
  } else if (std::strcmp(key, "123") == 0) {
    lv_buttonmatrix_set_map(spoolPickerKeyboard, kKeyboardNumberMap);
  } else {
    lv_textarea_add_text(spoolPickerSearch, key);
  }
}

void spoolPickerTextareaClicked(lv_event_t*) {
  if (spoolPickerKeyboard == nullptr || spoolPickerSearch == nullptr) return;
  lv_buttonmatrix_set_map(spoolPickerKeyboard,
                          spoolPickerFilter == 3 ? kKeyboardNumberMap
                                                 : kKeyboardLowerMap);
  setSpoolPickerInputMode(true);
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
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorPrimaryBlue), LV_PART_MAIN);
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
  lv_obj_set_style_bg_color(overlayBackdrop, lv_color_hex(kColorOverlayBackdrop), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(overlayBackdrop, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_width(overlayBackdrop, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(overlayBackdrop, 0, LV_PART_MAIN);

  overlayPanel = lv_obj_create(overlayBackdrop);
  lv_obj_set_size(overlayPanel, 420, 238);
  lv_obj_center(overlayPanel);
  lv_obj_remove_flag(overlayPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(overlayPanel, lv_color_hex(kColorPanelLightest), LV_PART_MAIN);
  lv_obj_set_style_border_color(overlayPanel, lv_color_hex(kColorPrimaryBlue), LV_PART_MAIN);
  lv_obj_set_style_border_width(overlayPanel, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(overlayPanel, 12, LV_PART_MAIN);
  lv_obj_set_style_text_color(overlayPanel, lv_color_hex(kColorTextDark), LV_PART_MAIN);
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

  overlayCancel = createOverlayButton(overlayPanel, "Abbrechen", 16, kColorOverlayCancelButton,
                                      rtos::UiActionType::Cancel);
  overlayConfirm = createOverlayButton(overlayPanel, "Best\xC3\xA4tigen", 218,
                                       kColorPrimaryBlue,
                                       rtos::UiActionType::Confirm);
  advancedModeButtons = {{
      createAdvancedModeButton("Gebrauchte Spule", 16, 50, 1),
      createAdvancedModeButton("Volle/neue Spule", 218, 50, 2),
      createAdvancedModeButton("Leergewicht", 16, 104, 3),
      createAdvancedModeButton("Ausgangsgewicht", 218, 104, 4),
  }};
  spoolPickerList = lv_obj_create(overlayPanel);
  lv_obj_set_pos(spoolPickerList, 12, 76);
  lv_obj_set_size(spoolPickerList, 456, 196);
  lv_obj_set_scroll_dir(spoolPickerList, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(spoolPickerList, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_opa(spoolPickerList, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(spoolPickerList, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(spoolPickerList, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(spoolPickerList, 0, LV_PART_MAIN);
  for (std::size_t index = 0; index < spoolPickerButtons.size(); ++index) {
    spoolPickerButtons[index] = createSpoolPickerButton(
        "", static_cast<std::int32_t>(index) * kSpoolPickerRowPitch, index);
  }
  spoolPickerSearch = lv_textarea_create(overlayPanel);
  lv_obj_set_pos(spoolPickerSearch, 12, 34);
  lv_obj_set_size(spoolPickerSearch, 280, 36);
  lv_textarea_set_one_line(spoolPickerSearch, true);
  lv_textarea_set_max_length(spoolPickerSearch, 47);
  lv_textarea_set_placeholder_text(spoolPickerSearch, "Suchbegriff");
  lv_obj_set_style_text_font(spoolPickerSearch, &ui_font_ui_german16,
                             LV_PART_MAIN);
  lv_obj_add_event_cb(spoolPickerSearch, spoolPickerTextareaClicked,
                      LV_EVENT_CLICKED, nullptr);
  spoolPickerFilterButton = lv_dropdown_create(overlayPanel);
  lv_obj_set_pos(spoolPickerFilterButton, 296, 34);
  lv_obj_set_size(spoolPickerFilterButton, 100, 36);
  lv_dropdown_set_options(spoolPickerFilterButton,
                          "Name\nMaterial\nHersteller\nID");
  lv_obj_set_style_text_font(spoolPickerFilterButton, &ui_font_ui_german16,
                             LV_PART_MAIN);
  lv_obj_add_event_cb(spoolPickerFilterButton, spoolPickerFilterChanged,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  spoolPickerScrollUp = lv_button_create(overlayPanel);
  lv_obj_set_pos(spoolPickerScrollUp, 400, 34);
  lv_obj_set_size(spoolPickerScrollUp, 32, 36);
  lv_obj_t* scrollUpLabel = lv_label_create(spoolPickerScrollUp);
  lv_label_set_text(scrollUpLabel, "^");
  lv_obj_set_style_text_font(scrollUpLabel, &ui_font_ui_german16,
                             LV_PART_MAIN);
  lv_obj_center(scrollUpLabel);
  lv_obj_add_event_cb(spoolPickerScrollUp, spoolPickerScrollClicked,
                      LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<std::intptr_t>(1)));
  spoolPickerScrollDown = lv_button_create(overlayPanel);
  lv_obj_set_pos(spoolPickerScrollDown, 436, 34);
  lv_obj_set_size(spoolPickerScrollDown, 32, 36);
  lv_obj_t* scrollDownLabel = lv_label_create(spoolPickerScrollDown);
  lv_label_set_text(scrollDownLabel, "v");
  lv_obj_set_style_text_font(scrollDownLabel, &ui_font_ui_german16,
                             LV_PART_MAIN);
  lv_obj_center(scrollDownLabel);
  lv_obj_add_event_cb(spoolPickerScrollDown, spoolPickerScrollClicked,
                      LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<std::intptr_t>(-1)));
  lv_obj_add_flag(spoolPickerSearch, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerFilterButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerList, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerScrollUp, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerScrollDown, LV_OBJ_FLAG_HIDDEN);
  spoolPickerKeyboard = lv_buttonmatrix_create(lv_layer_top());
  lv_obj_set_pos(spoolPickerKeyboard, 0, 84);
  lv_obj_set_size(spoolPickerKeyboard, 480, 236);
  lv_buttonmatrix_set_map(spoolPickerKeyboard, kKeyboardLowerMap);
  lv_obj_set_style_pad_all(spoolPickerKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(spoolPickerKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(spoolPickerKeyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_text_font(spoolPickerKeyboard, LV_FONT_DEFAULT,
                             LV_PART_ITEMS);
  lv_obj_add_event_cb(spoolPickerKeyboard, spoolPickerKeyboardEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(spoolPickerKeyboard, LV_OBJ_FLAG_HIDDEN);
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
  if (spoolPickerKeyboard != nullptr)
    lv_obj_add_flag(spoolPickerKeyboard, LV_OBJ_FLAG_HIDDEN);
  activeOverlayKind = rtos::UiOverlayKind::None;
  activeOverlayRequestId = 0;
}

void styleOverlayNavigation(std::uint32_t color) {
  if (overlayCancel == nullptr) return;
  lv_obj_set_style_bg_color(overlayCancel, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_t* label = lv_obj_get_child(overlayCancel, 0);
  if (label != nullptr)
    lv_obj_set_style_text_color(label, lv_color_hex(kColorTextWhite), LV_PART_MAIN);
}

void showOverlay(const rtos::UiCommand& command, bool progress) {
  ensureOverlay();
  // Restore the standard geometry first because the same overlay objects are
  // reused for every dialog.
  lv_obj_set_size(overlayPanel, 420, 238);
  lv_obj_center(overlayPanel);
  lv_obj_set_style_border_width(overlayPanel, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(overlayPanel, 12, LV_PART_MAIN);
  lv_obj_set_pos(overlayTitle, 16, 12);
  lv_obj_set_size(overlayTitle, 388, 32);
  lv_obj_set_pos(overlayText, 16, 52);
  lv_obj_set_size(overlayText, 388, 100);
  lv_obj_set_pos(overlayCancel, 16, 158);
  lv_obj_set_size(overlayCancel, 170, 50);
  lv_obj_set_pos(overlayConfirm, 218, 158);
  lv_obj_set_size(overlayConfirm, 170, 50);
  styleOverlayNavigation(kColorOverlayCancelButton);
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
  lv_obj_add_flag(spoolPickerSearch, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerFilterButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerList, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerScrollUp, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(spoolPickerScrollDown, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_set_pos(overlayPanel, 0, 0);
    lv_obj_set_size(overlayPanel, 480, 320);
    lv_obj_set_style_border_width(overlayPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlayPanel, 0, LV_PART_MAIN);
    lv_obj_set_pos(overlayTitle, 12, 4);
    lv_obj_set_size(overlayTitle, 456, 28);
    lv_obj_add_flag(overlayText, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(overlayCancel, 12, 276);
    lv_obj_set_size(overlayCancel, 456, 36);
    lv_label_set_text(lv_obj_get_child(overlayCancel, 0),
                      "Zur\xC3\xBC" "ck");
    styleOverlayNavigation(kColorPrimaryBlue);
    spoolPickerIds.fill(0);
    spoolPickerInputActive = false;
    for (lv_obj_t* button : spoolPickerButtons)
      lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(spoolPickerList, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(spoolPickerSearch, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerFilterButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerScrollUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolPickerScrollDown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlayBackdrop);
    return;
  }
  if (progress) {
    lv_obj_remove_flag(overlayProgress, LV_OBJ_FLAG_HIDDEN);
    // Fresh start every time -- overlayProgress is shared across every kind
    // of progress dialog (WiFi, Spoolman, NFC, ...), so without this it
    // shows whatever value a previous, unrelated dialog last animated it
    // to. Only the AssignTray wait (see UpdateProgress below) actually
    // animates it further; every other progress dialog just stays full.
    lv_bar_set_value(overlayProgress, 100, LV_ANIM_OFF);
    lv_obj_remove_flag(overlayCancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlayConfirm, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(
        lv_obj_get_child(overlayCancel, 0),
        command.overlayKind == rtos::UiOverlayKind::ConnectionProgress
            ? "Abbrechen"
            : "Schlie\xC3\x9F" "en");
    if (command.overlayKind != rtos::UiOverlayKind::ConnectionProgress)
      styleOverlayNavigation(kColorPrimaryBlue);
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
      styleOverlayNavigation(kColorPrimaryBlue);
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
      kColorTouchMarker1, kColorTouchMarker2, kColorTouchMarker3, kColorTouchMarker4, kColorTouchMarker5,
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
  lv_obj_set_style_border_color(marker, lv_color_hex(kColorTextDark), LV_PART_MAIN);
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

// Every button target is now a real EEZ LVGLButtonWidget with its own
// content-sized, centered child label (see scripts/convert_label_buttons.py
// and TASKS.md, 2026-08-23) -- EEZ Studio owns that label's position/size/
// alignment, and LVGL auto-recenters a content-sized label as its text
// changes. Forcibly re-measuring/re-centering it here on every text update
// (the former centerButtonLabel()) is therefore both redundant and actively
// harmful: it caused the same visible position-jump bug already fixed for
// headers a turn earlier, here on every text field that updates live
// (Spoolman-/Drucker-Einstellungen). buttonLabel() already
// falls back to `object` itself when it has no label descendant (a plain
// status label, not a button), so one code path covers both cases.
void setControlText(lv_obj_t* object, const char* text) {
  if (object == nullptr) return;
  lv_obj_t* label = buttonLabel(object);
  lv_label_set_text(label == nullptr ? object : label, text);
}

// Standard-Luma-Gewichtung (Rec. 601), Schwelle 150000 entspricht ~59%
// Helligkeit -- ab da gilt ein Hintergrund als hell genug fuer dunklen Text.
bool isLightBackground(std::uint32_t backgroundRgb) {
  const std::uint32_t red = (backgroundRgb >> 16U) & 0xFFU;
  const std::uint32_t green = (backgroundRgb >> 8U) & 0xFFU;
  const std::uint32_t blue = backgroundRgb & 0xFFU;
  return (red * 299U + green * 587U + blue * 114U) > 150000U;
}

void setButtonColors(lv_obj_t* button, std::uint32_t backgroundRgb) {
  lv_obj_set_style_bg_color(button, lv_color_hex(backgroundRgb), LV_PART_MAIN);
  const bool useDarkText = isLightBackground(backgroundRgb);
  lv_obj_t* label = buttonLabel(button);
  if (label != nullptr) {
    lv_obj_set_style_text_color(label,
                                lv_color_hex(useDarkText ? kColorTextDark : kColorTextWhite),
                                LV_PART_MAIN);
  }
}

bool sendAction(rtos::UiActionType type, rtos::PrinterId printerId,
                std::uint8_t amsId, std::uint8_t trayId,
                std::int32_t value, rtos::SpoolId spoolId,
                const char* text) {
  if (rtosContext == nullptr) {
    return false;
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
  if (xQueueSend(rtosContext->appEventQueue, &event,
                 pdMS_TO_TICKS(250)) != pdPASS) {
    FS_LOGW(services::LogComponent::Ui,
            "Action enqueue failed queue=app_event action=%u request_id=%lu",
            static_cast<unsigned>(type),
            static_cast<unsigned long>(event.requestId));
    return false;
  }
  return true;
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
  // Staging ist druckerunabhaengig -- siehe die gleiche Korrektur in
  // updateHomeContent(); stagingState.printerId wird nirgends gesetzt und
  // war dadurch immer 0, sodass hier immer spoolId=0 gesendet wurde statt
  // der tatsaechlich gestagten Spule.
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 0,
             stagingState.spoolId);
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

// Commits whatever text currently sits in the open Spoolman field editor
// into spoolmanDraft, exactly as pressing "OK" on the on-screen keyboard
// would. Test/Speichern/Abbrechen sit lower on the same screen (y=264)
// than the editor+keyboard overlay (y=0-220, no backdrop blocking touches
// to the rest of the screen) -- without this, tapping Test/Speichern while
// a field was still mid-edit (keyboard still open, "OK" never pressed)
// silently used the old, pre-edit value instead of what was visibly typed
// (Nutzer-Bugreport 2026-08-23: "Testen" verband sich zum urspruenglichen
// statt zum gerade eingegebenen Server).
void commitSpoolmanEditorIfOpen() {
  if (spoolmanEditor == nullptr || editorContext != EditorContext::Spoolman)
    return;
  const char* editedText = lv_textarea_get_text(spoolmanEditor);
  if (!sendAction(rtos::UiActionType::EditSpoolmanSetting, currentPrinterId, 0,
                  0, activeSpoolmanField, 0, editedText)) {
    return;
  }
  char* destination = spoolmanFieldDestination(activeSpoolmanField);
  const std::size_t capacity = spoolmanFieldCapacity(activeSpoolmanField);
  if (destination != nullptr && capacity > 0)
    std::snprintf(destination, capacity, "%s", editedText);
  updateSpoolmanSettingsContent();
  lv_label_set_text(objects.spoolman_setting_status,
                    "Status: ge\xC3\xA4ndert, nicht gespeichert");
  lv_label_set_text(objects.spoolman_setting_version, "Server: -");
  closeSpoolmanEditor();
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
    if (editorContext == EditorContext::Printer) {
      const char* editedText = lv_textarea_get_text(spoolmanEditor);
      if (sendAction(rtos::UiActionType::EditPrinterField, editingPrinterId, 0,
                     0, activePrinterField, 0, editedText)) {
        closeSpoolmanEditor();
      }
    } else {
      commitSpoolmanEditorIfOpen();
    }
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
  // Commit a still-open field edit first (see commitSpoolmanEditorIfOpen())
  // so Testen/Speichern/Abbrechen always act on what's currently visible in
  // the editor, not a stale pre-edit value.
  commitSpoolmanEditorIfOpen();
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

TrayUiEntry* trayUiEntry(std::uint8_t amsId, std::uint8_t trayId) {
  if (amsId == 0xFF && trayId == 0xFF) return &externalTrayEntry;
  if (amsId < 1 || amsId > trayEntries.size() || trayId >= 4) return nullptr;
  return &trayEntries[amsId - 1][trayId];
}

// Parses the leading 6 hex digits of a Bambu tray_color (RRGGBB or
// RRGGBBAA) into an 0xRRGGBB value; falls back to a neutral grey for
// missing/malformed input.
std::uint32_t parseTrayColorHex(const char* colorHex) {
  if (colorHex == nullptr || std::strlen(colorHex) < 6) return kColorNeutralGrey;
  char buffer[7];
  std::snprintf(buffer, sizeof(buffer), "%.6s", colorHex);
  char* end = nullptr;
  const unsigned long value = std::strtoul(buffer, &end, 16);
  return (end != nullptr && *end == '\0') ? static_cast<std::uint32_t>(value)
                                          : kColorNeutralGrey;
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
                                               ? kColorManagedPrinterOrange
                                               : (entry.enabled && entry.exists
                                                      ? kColorPrimaryBlue
                                                      : kColorInactivePrinterRow)),
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
  const auto& staging = stagingState;
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 1,
             staging.spoolId);
}

void stagingActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const auto& spool = stagingSpoolState;
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
             stagingState.spoolId,
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
  // "Aus Staging" (ConfigureSlotFromStaging) must commit the spool that is
  // actually staged, not whatever this slot currently reports -- every
  // other slot action here (Reapply/Reset/Untag/Refresh) intentionally
  // keeps using the slot's own current spool (Phase 9.9).
  const rtos::SpoolId spoolId =
      type == rtos::UiActionType::ConfigureSlotFromStaging
          ? stagingState.spoolId
          : selectedTraySpoolId;
  sendAction(type, currentPrinterId, selectedTrayAmsId, selectedTrayId, 0,
             spoolId);
}

void trayTargetClicked(lv_event_t* event) {
  // TraySelect has no separate confirm control in the EEZ design (only
  // Cancel) -- tapping a slot both highlights it (optimistic, local) and
  // commits it (ConfigureSlotFromStaging, AppTask-side), matching how
  // e.g. the printer picker commits on tap elsewhere in this app.
  const auto trayId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const std::uint8_t amsId = trayId == 0xFF ? 0xFF : currentAmsId;
  updateTraySelection(currentPrinterId, amsId, trayId, true);
  sendAction(rtos::UiActionType::ConfigureSlotFromStaging, currentPrinterId,
             amsId, trayId, 0, stagingState.spoolId);
}

void tagActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const rtos::SpoolId spoolId =
      (type == rtos::UiActionType::SelectSpool ||
       type == rtos::UiActionType::AssignTag)
          ? stagingState.spoolId
          : 0;
  sendAction(type, currentPrinterId, 0, 0, 0, spoolId);
}

void lastTagSpoolClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::AssignTag, currentPrinterId, 0, 0, 1);
}

void assignTagWithPickerClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::AssignTag, currentPrinterId);
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

void styleLabelButton(lv_obj_t* object) {
  if (object == nullptr) return;
  // Every former "label pretending to be a button" is now a real
  // LVGLButtonWidget with its own EEZ-defined child label (see
  // scripts/convert_label_buttons.py, TASKS.md 2026-08-23). Color no
  // longer comes from here either (Nutzerwunsch 2026-08-23): every button
  // carries a named EEZ Style (ButtonPrimary/ButtonNeutral/ButtonDanger,
  // see scripts/add_button_role_styles.py) referencing theme colors, set
  // once in the .eez-project and applied by the generated
  // add_style_button_*() call at widget creation -- this only still needs
  // to make the button clickable, since EEZ does not mark these buttons
  // clickable on its own.
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_radius(object, 8, LV_PART_MAIN);
}

void setLabelButtonAvailable(lv_obj_t* object, bool available) {
  if (object == nullptr) return;
  lv_obj_set_flag(object, LV_OBJ_FLAG_CLICKABLE, available);
  if (available) {
    lv_obj_remove_state(object, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(object, LV_STATE_DISABLED);
  }
}

void applySpoolmanAppState(const rtos::UiCommand* command = nullptr) {
  const bool online =
      filament_station::models::spoolmanOperationsAvailable(spoolmanAppState);
  const bool tagReady =
      filament_station::models::spoolmanTagOperationsAvailable(
          spoolmanAppState);

  const std::array<lv_obj_t*, 7> onlineControls{{
      objects.staging_details_quick_weight,
      objects.staging_action_configure,
      objects.staging_action_advanced_weight,
      objects.staging_action_erase_tag,
      objects.tray_action_from_staging,
      objects.tray_action_manual,
      objects.tag_result_quick_weight,
  }};
  for (lv_obj_t* control : onlineControls)
    setLabelButtonAvailable(control, online);
  setLabelButtonAvailable(objects.tag_result_advanced_weight, online);

  setLabelButtonAvailable(objects.staging_action_link_tag,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.staging_action_unlink_tag,
                          tagReady && currentTagCanRemove);
  setLabelButtonAvailable(objects.tag_action_select_spool,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.tag_action_use_last_spool,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.tag_action_erase,
                          tagReady && currentTagCanRemove);
  setLabelButtonAvailable(objects.tag_definition_import_select_spool,
                          tagReady);
  setLabelButtonAvailable(objects.tag_definition_import_spoolman, tagReady);
  setLabelButtonAvailable(objects.tag_legacy_select_spool,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.tag_legacy_import, tagReady);
  setLabelButtonAvailable(objects.tag_legacy_erase,
                          tagReady && currentTagCanRemove);
  setLabelButtonAvailable(objects.tag_unknown_select_spool,
                          tagReady && currentTagCanAssign);

  // Configuration and navigation must remain available even while Spoolman
  // is offline or its tag field is incompatible.
  setLabelButtonAvailable(objects.settings_spoolman, true);
  setLabelButtonAvailable(objects.spoolman_setting_cancel, true);

  if (command != nullptr) {
    lv_label_set_text(objects.home_bottom_status, command->text);
    lv_label_set_text(objects.settings_bottom_status, command->text);
    lv_label_set_text(objects.spoolman_setting_status, command->text);
    char version[64]{};
    std::snprintf(version, sizeof(version), "Server: %s",
                  command->title[0] != '\0' ? command->title : "-");
    lv_label_set_text(objects.spoolman_setting_version, version);
  }
}

// Multicolor-Filament-Anzeige (Nutzerwunsch 2026-08-23): Farbe 1 faerbt den
// Button selbst (siehe setButtonColors()-Aufrufe an den Call-Sites), Farbe 2
// und 3 faerben je einen vom Nutzer im EEZ-Projekt angelegten
// Container-Swatch daneben (home_<button>_1/_2) -- analog zu den vier
// AMS-Slot-Farbcontainern in updateHomeContent(), nicht mehr ueber
// dynamisch zur Laufzeit erzeugte Overlay-Kreise.
void updateHomeColorSwatches(
    lv_obj_t* swatch1, lv_obj_t* swatch2,
    const std::array<std::uint32_t, models::kMaximumFilamentColors>& colors,
    std::uint8_t colorCount) {
  const std::array<lv_obj_t*, 2> swatches{{swatch1, swatch2}};
  for (std::size_t index = 0; index < swatches.size(); ++index) {
    const std::size_t colorIndex = index + 1;  // colors[0] -> button itself
    if (colorIndex >= colorCount) {
      lv_obj_set_style_bg_opa(swatches[index], LV_OPA_TRANSP, LV_PART_MAIN);
      continue;
    }
    lv_obj_set_style_bg_opa(swatches[index], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(swatches[index], lv_color_hex(colors[colorIndex]),
                              LV_PART_MAIN);
  }
}

void createStagingTableDecoration() {
  lv_obj_set_style_bg_opa(objects.staging_details_title, LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.staging_details_title,
                            lv_color_hex(kColorPrimaryBlue), LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.staging_details_title,
                              lv_color_hex(kColorTextWhite), LV_PART_MAIN);
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
      lv_obj_set_style_bg_color(label, lv_color_hex(kColorPanelLightAlt), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(label, lv_color_hex(kColorPanelLight), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(kColorTextDark), LV_PART_MAIN);
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
                            lv_color_hex(kColorPanelLight), LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.tray_details_content,
                              lv_color_hex(kColorTextDark), LV_PART_MAIN);
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
  setControlText(objects.staging_action_link_tag, "Tag zuordnen");
  setControlText(objects.staging_action_unlink_tag,
                 "Tag-Zuordnung entfernen");
  lv_obj_set_pos(objects.staging_action_link_tag, 4, 100);
  lv_obj_set_size(objects.staging_action_link_tag, 232, 52);
  lv_obj_set_pos(objects.staging_action_unlink_tag, 244, 100);
  lv_obj_set_size(objects.staging_action_unlink_tag, 232, 52);
  setControlText(objects.staging_action_write_tag, "Kein NFC-Tag erkannt");
  lv_obj_remove_flag(objects.staging_action_write_tag, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(objects.staging_action_write_tag, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(objects.staging_action_write_tag, 4, 156);
  lv_obj_set_size(objects.staging_action_write_tag, 472, 52);
  lv_obj_set_style_bg_opa(objects.staging_action_write_tag, LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.staging_action_write_tag,
                            lv_color_hex(kColorPanelLight), LV_PART_MAIN);
  lv_obj_set_style_text_color(objects.staging_action_write_tag,
                              lv_color_hex(kColorTextDark), LV_PART_MAIN);
  lv_obj_set_style_text_align(objects.staging_action_write_tag,
                              LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_radius(objects.staging_action_write_tag, 8, LV_PART_MAIN);
  setControlText(objects.staging_action_erase_tag,
                 "Spule ausw\xC3\xA4hlen");
  lv_obj_remove_flag(objects.staging_action_erase_tag, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(objects.staging_action_erase_tag, 4, 212);
  lv_obj_set_size(objects.staging_action_erase_tag, 472, 48);
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

  const std::array<lv_obj_t*, 6> tagSettings{{
      objects.tag_action_settings, objects.tag_review_settings,
      objects.tag_write_settings, objects.tag_result_settings,
      objects.tag_legacy_settings, objects.tag_unknown_settings,
  }};
  //for (lv_obj_t* control : tagSettings) setControlText(control, "Einst.");
  setControlText(objects.tag_action_title, "NFC-Tag zuordnen");
  setControlText(objects.tag_action_select_spool, "Tag zuordnen");
  setControlText(objects.tag_action_use_last_spool,
                 "Zuletzt verwendete Spule");
  lv_obj_add_flag(objects.tag_action_write, LV_OBJ_FLAG_HIDDEN);
  setControlText(objects.tag_action_erase, "Tag-Zuordnung entfernen");
  setControlText(objects.tag_action_back, "Zur\xC3\xBC" "ck");
  setControlText(objects.tag_review_title, "Tag-Zuordnung pr\xC3\xBC" "fen");
  setControlText(objects.tag_review_back, "Zur\xC3\xBC" "ck");
  setControlText(objects.tag_review_cancel, "Abbrechen");
  setControlText(objects.tag_review_confirm, "Best\xC3\xA4tigen");
  setControlText(objects.tag_write_title, "Tag wird zugeordnet");
  setControlText(objects.tag_write_detected, "Tag erkannt");
  setControlText(objects.tag_write_memory, "Zuordnung wird gespeichert");
  setControlText(objects.tag_write_data, "Tag wird bei Bedarf aktualisiert");
  setControlText(objects.tag_write_verify, "Ergebnis wird gepr\xC3\xBC" "ft");
  setControlText(objects.tag_write_cancel, "Abbrechen");
  setControlText(objects.tag_result_title, "NFC-Ergebnis");
  setControlText(objects.tag_result_quick_weight, "Schnell wiegen");
  setControlText(objects.tag_result_advanced_weight, "Erweitert wiegen");
  setControlText(objects.tag_result_close, "Schlie\xC3\x9F" "en");
  //setControlText(objects.tag_definition_import_settings, "Einst.");
  setControlText(objects.tag_definition_import_title,
                 "Tagdefinition erkannt");
  setControlText(objects.tag_definition_import_select_spool,
                 "Tag zuordnen");
  setControlText(objects.tag_definition_import_spoolman,
                 "Nach Spoolman importieren");
  setControlText(objects.tag_definition_import_cancel, "Abbrechen");
  setControlText(objects.tag_legacy_title, "Legacy-Tag erkannt");
  setControlText(objects.tag_legacy_select_spool, "Tag zuordnen");
  setControlText(objects.tag_legacy_import, "Nach Spoolman importieren");
  lv_obj_add_flag(objects.tag_legacy_migrate, LV_OBJ_FLAG_HIDDEN);
  setControlText(objects.tag_legacy_erase, "Tag-Zuordnung entfernen");
  setControlText(objects.tag_legacy_close, "Schlie\xC3\x9F" "en");
  setControlText(objects.tag_unknown_title, "Unbekannter NFC-Tag");
  setControlText(objects.tag_unknown_select_spool, "Tag zuordnen");
  setControlText(objects.tag_unknown_close, "Schlie\xC3\x9F" "en");
  setControlText(objects.bambu_spool_type_title, "Leergewicht ausw\xC3\xA4hlen");
  setControlText(objects.bambu_spool_type_back, "Zur\xC3\xBC" "ck");
  styleLabelButton(objects.tag_definition_import_select_spool);
  styleLabelButton(objects.tag_definition_import_spoolman);
  styleLabelButton(objects.tag_definition_import_cancel);
  styleLabelButton(objects.tag_legacy_select_spool);
  styleLabelButton(objects.tag_legacy_import);
  styleLabelButton(objects.tag_legacy_migrate);
  styleLabelButton(objects.tag_legacy_erase);
  styleLabelButton(objects.tag_legacy_close);
  styleLabelButton(objects.tag_unknown_select_spool);
  styleLabelButton(objects.tag_unknown_close);
  styleLabelButton(objects.bambu_spool_type_low);
  styleLabelButton(objects.bambu_spool_type_high);
  styleLabelButton(objects.bambu_spool_type_manual);
  styleLabelButton(objects.bambu_spool_type_back);

  bindClick(objects.tag_action_header, headerClicked);
  bindClick(objects.tag_review_header, headerClicked);
  bindClick(objects.tag_write_header, headerClicked);
  bindClick(objects.tag_result_header, headerClicked);
  for (lv_obj_t* control : tagSettings) bindClick(control, settingsClicked);
  bindClick(objects.tag_action_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_action_use_last_spool, lastTagSpoolClicked);
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
  bindClick(objects.tag_definition_import_select_spool,
            assignTagWithPickerClicked);
  bindClick(objects.tag_definition_import_spoolman, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_definition_import_cancel, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));
  bindClick(objects.tag_legacy_header, headerClicked);
  bindClick(objects.tag_legacy_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_legacy_import, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_legacy_erase, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.tag_legacy_close, backClicked);
  bindClick(objects.tag_unknown_header, headerClicked);
  bindClick(objects.tag_unknown_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_unknown_close, backClicked);
  bindClick(objects.bambu_spool_type_back, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));

  bindClick(objects.home_header, headerClicked);
  // home_bottom_printers wurde im EEZ-Projekt entfernt (Nutzerwunsch:
  // dieselbe Funktion/Druckerauswahl ist bereits ueber die Titelleiste
  // home_header erreichbar) -- kein Code-seitiges Ausblenden mehr noetig.
  bindClick(objects.select_header, headerClicked);
  bindClick(objects.settings_header, headerClicked);

  bindClick(objects.home_settings, settingsClicked);
  bindClick(objects.select_settings, settingsClicked);
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
  bindClick(objects.staging_action_link_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.staging_action_unlink_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.staging_action_erase_tag, stagingActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::SelectSpool));

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
  // Die vier Slot-Farbcontainer je AMS-Button sind im EEZ-Projekt
  // verschachtelt angelegt (home_ams_<ams>_2 ist Kind von _1, _3 Kind von
  // _2, _4 Kind von _3) und jeweils WEITER RECHTS positioniert als ihr
  // eigener, schmaler Elterncontainer -- LVGL clippt Kinder standardmaessig
  // auf die Grenzen ihres Elternobjekts, wodurch nur der erste Container
  // (_1, noch innerhalb des Buttons selbst) sichtbar war. LV_OBJ_FLAG_
  // OVERFLOW_VISIBLE deaktiviert dieses Clipping je Container.
  for (lv_obj_t* container : std::array<lv_obj_t*, 16>{{
           objects.home_ams_1_1, objects.home_ams_1_2, objects.home_ams_1_3,
           objects.home_ams_1_4, objects.home_ams_2_1, objects.home_ams_2_2,
           objects.home_ams_2_3, objects.home_ams_2_4, objects.home_ams_3_1,
           objects.home_ams_3_2, objects.home_ams_3_3, objects.home_ams_3_4,
           objects.home_ams_4_1, objects.home_ams_4_2, objects.home_ams_4_3,
           objects.home_ams_4_4,
       }}) {
    lv_obj_add_flag(container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  }
  bindClick(objects.home_tray_1__tray, trayClicked, 0);
  bindClick(objects.home_tray_2__tray, trayClicked, 1);
  bindClick(objects.home_tray_3__tray, trayClicked, 2);
  bindClick(objects.home_tray_4__tray, trayClicked, 3);
  bindClick(objects.home_tray_external__tray, trayClicked, 0xFF);
  bindClick(objects.staging__staging, stagingClicked);

  bindClick(objects.select_printer_1, printerClicked, 1);
  bindClick(objects.select_printer_2, printerClicked, 2);
  bindClick(objects.select_printer_3, printerClicked, 3);
  bindClick(objects.select_printer_4, printerClicked, 4);

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
  lv_obj_set_pos(objects.wifi_settings_status, 8, 76);
  lv_obj_set_size(objects.wifi_settings_status, 464, 64);
  lv_obj_set_pos(objects.wifi_settings_ssid, 8, 144);
  lv_obj_set_size(objects.wifi_settings_ssid, 464, 28);
  lv_obj_set_pos(objects.wifi_settings_ip, 8, 176);
  lv_obj_set_size(objects.wifi_settings_ip, 464, 28);
  lv_label_set_text(objects.wifi_settings_status,
                    "Status: Verbindung wird gepr\xC3\xBC" "ft\n"
                    "Neu konfigurieren startet das WLAN-Portal.");
  lv_label_set_text(objects.wifi_settings_ssid, "SSID: -");
  lv_label_set_text(objects.wifi_settings_ip, "IP: - | Signal: -");
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
  styleLabelButton(objects.wifi_settings_back);
  styleLabelButton(objects.scale_settings_back);
  styleLabelButton(objects.device_settings_back);
  styleLabelButton(objects.diagnostics_settings_back);
  styleLabelButton(objects.firmware_settings_back);

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
  styleLabelButton(objects.printer_settings_back);
  styleLabelButton(objects.printer_edit_delete);
  styleLabelButton(objects.printer_edit_cancel);

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
  styleLabelButton(objects.spoolman_setting_cancel);

  const std::array<lv_obj_t*, 11> stagingButtons{{
      objects.staging_details_header,
      objects.staging_details_settings,
      objects.staging_details_quick_weight,
      objects.staging_details_more,
      objects.staging_details_close,
      objects.staging_actions_header,
      objects.staging_actions_settings,
      objects.staging_action_configure,
      objects.staging_action_advanced_weight,
      objects.staging_action_link_tag,
      objects.staging_action_unlink_tag,
  }};
  for (lv_obj_t* button : stagingButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.staging_action_erase_tag);
  styleLabelButton(objects.staging_action_clear);
  styleLabelButton(objects.staging_actions_back);
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
  styleLabelButton(objects.tray_action_untag);
  styleLabelButton(objects.tray_action_reset);
  styleLabelButton(objects.tray_details_close);
  styleLabelButton(objects.tray_actions_back);
  styleLabelButton(objects.tray_select_cancel);
  styleLabelButton(objects.home_active_ams);
  styleLabelButton(objects.home_ams_4);
  const std::array<lv_obj_t*, 8> tagButtons{{
      objects.tag_action_select_spool, objects.tag_action_use_last_spool,
      objects.tag_review_confirm,
      objects.tag_result_quick_weight, objects.tag_result_advanced_weight,
      objects.tag_action_back, objects.tag_review_back,
      objects.tag_result_close,
  }};
  for (lv_obj_t* button : tagButtons) styleLabelButton(button);
  styleLabelButton(objects.tag_action_erase);
  styleLabelButton(objects.tag_review_cancel);
  styleLabelButton(objects.tag_write_cancel);

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
  // Frueher hier: Schriftart von objects.home_ams_1s Label-Kind auf
  // home_active_ams/home_ams_4 uebertragen, fuer eine frueher noch
  // vorhandene "AMS N"-Beschriftung. Die AMS-Buttons zeigen keinen Text
  // mehr (Nutzerwunsch) und haben inzwischen auch kein Label-Kind mehr
  // (durch die vier neuen EEZ-Farbcontainer ersetzt) -- buttonLabel() lief
  // dadurch ins Leere und lv_obj_get_style_text_font(nullptr, ...) hing die
  // UI-Task auf (Watchdog-Reboot). Block ersatzlos entfernt.
  vTaskDelay(pdMS_TO_TICKS(250));
  createStagingTableDecoration();
  vTaskDelay(pdMS_TO_TICKS(250));
  createTrayDetailsDecoration();
}

void updatePrinterList() {
  // select_printer_1/2/3/4 are bound to fixed printerIds 1/2/3/4 (see
  // bindClick calls above), so printerEntries[index] must line up
  // positionally with printerId == index + 1 -- this renders real data
  // (Phase 8.6 CRUD, synced via UpdatePrinterList) instead of the former
  // models::mock::printers().
  const std::array<lv_obj_t*, 4> buttons{{
      objects.select_printer_1,
      objects.select_printer_2,
      objects.select_printer_3,
      objects.select_printer_4,
  }};

  for (std::size_t index = 0; index < buttons.size(); ++index) {
    const auto& printer = printerEntries[index];
    if (!printer.exists) {
      setButtonText(buttons[index], "+ freier Druckerplatz");
      setButtonColors(buttons[index], kColorDisabledGrey);
      continue;
    }
    char text[96];
    std::snprintf(
        text, sizeof(text), "%s%s\n%s | %u AMS%s",
        printer.id == currentPrinterId ? "> " : "", printer.name,
        connectionText(printer.connectionState), printer.amsCount,
        printer.isDefault ? " | Standard" : "");
    setButtonText(buttons[index], text);

    const std::uint32_t color =
        printer.id == currentPrinterId
            ? kColorPrimaryBlue
            : (printer.connectionState == models::UiConnectionState::Connected
                   ? kColorSuccessGreen
                   : kColorDisabledGrey);
    setButtonColors(buttons[index], color);
  }

  setControlText(objects.select_bottom_status, "Drucker verwalten");
}

// Zielobjekte entsprechen den Sub-Widgets der CMP_TRAY_CARD-Komponente
// (ui-project, Nutzerwunsch 2026-08-23): "button" ist das eigentliche
// klickbare EEZ-Objekt (Sub-Widget "tray", nicht der transparente
// Kartenwrapper objects.home_tray_N selbst).
void updateTrayButton(lv_obj_t* button, lv_obj_t* label, lv_obj_t* swatch1,
                      lv_obj_t* swatch2, lv_obj_t* spoolIdContainer,
                      lv_obj_t* spoolIdLabel, lv_obj_t* nozzleIcon,
                      rtos::PrinterId printerId, std::uint8_t amsId,
                      std::uint8_t trayId, const char* title) {
  (void)printerId;  // Real tray data (see AppTask::syncAmsToUi) is only
                    // ever synced for the printer currently in focus.
  (void)title;  // Nutzerwunsch (2026-08-22): kein "Slot N"/"Extern"-Titel
                // mehr -- die feste Bildschirmposition zeigt weiterhin,
                // welcher Slot gemeint ist (analog zu den AMS-Buttons).
  const TrayUiEntry* tray = trayUiEntry(amsId, trayId);
  char text[64];
  if (tray == nullptr || !tray->occupied) {
    std::snprintf(text, sizeof(text), "leer");
    setButtonColors(button, kColorNeutralGrey);
    updateHomeColorSwatches(swatch1, swatch2, {}, 0);
    lv_obj_add_flag(spoolIdContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolIdLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Material kommt real vom Drucker (tray->material). Restgewicht,
    // Spoolman-ID, K-Faktor und "Duese aktiv" sind auf Nutzerwunsch
    // vorerst Mockdaten: der Drucker kennt nur Material/Farbe (siehe
    // docs/bambu-protocol.md); ein echter Abgleich mit der zugeordneten
    // Spoolman-Spule (tray->spoolId, sofern von dieser App zugeordnet) fuer
    // diese Werte ist noch nicht angebunden. externalTrayEntry hat kein
    // sinnvolles amsId/trayId (Sentinel 0xFF/0xFF) -- ein fester Seed 0
    // haelt die Mockformeln dafuer im plausiblen Bereich.
    const std::uint8_t mockSeed = amsId == 0xFF ? 0U : trayId;
    const std::uint32_t mockRemainingWeightGrams =
        500U - (static_cast<std::uint32_t>(mockSeed) * 15U);
    const std::uint32_t mockSpoolmanId =
        1000U +
        (amsId == 0xFF ? 90U : static_cast<std::uint32_t>(amsId) * 10U) +
        static_cast<std::uint32_t>(mockSeed) + 1U;
    // Format "K (#.###)" (Nutzerwunsch): ein Digit vor, drei Nachkommastellen
    // -- passend zur ueblichen Groessenordnung echter Bambu-K-Faktor-Werte
    // (Flow-Dynamics-Kalibrierung, typischerweise 0.000-0.100).
    const std::uint32_t mockKFactorThousandths = 20U + mockSeed;
    std::snprintf(
        text, sizeof(text), "%s\n%ug\nK (%u.%03u)",
        tray->material[0] != '\0' ? tray->material : "belegt",
        static_cast<unsigned>(mockRemainingWeightGrams),
        static_cast<unsigned>(mockKFactorThousandths / 1000U),
        static_cast<unsigned>(mockKFactorThousandths % 1000U));
    const std::array<std::uint32_t, models::kMaximumFilamentColors> colors{
        parseTrayColorHex(tray->colorHex)};
    const std::uint8_t colorCount = tray->colorHex[0] != '\0' ? 1U : 0U;
    const std::uint32_t backgroundColor =
        colorCount > 0 ? colors[0] : kColorNeutralGrey;
    setButtonColors(button, backgroundColor);
    updateHomeColorSwatches(swatch1, swatch2, colors, colorCount);

    lv_obj_remove_flag(spoolIdContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolIdLabel, LV_OBJ_FLAG_HIDDEN);
    char spoolIdText[12];
    std::snprintf(spoolIdText, sizeof(spoolIdText), "%lu",
                  static_cast<unsigned long>(mockSpoolmanId));
    setControlText(spoolIdLabel, spoolIdText);

    // Mock: das erste belegte Fach je AMS (bzw. das externe Fach, sofern
    // belegt) gilt als "aktiv" -- eine echte Zuordnung zur tatsaechlich
    // druckenden Duese existiert im Datenmodell noch nicht.
    const bool mockActive = mockSeed == 0U;
    if (mockActive) {
      lv_obj_remove_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
      lv_image_set_src(nozzleIcon, isLightBackground(backgroundColor)
                                        ? &img_3_d_printer_nozzle
                                        : &img_3_d_printer_nozzle_w);
    } else {
      lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
    }
  }
  setControlText(label, text);
}

void updateHomeContent() {
  const PrinterUiEntry* printer = printerEntry(currentPrinterId);
  if (printer == nullptr || !printer->exists) {
    return;
  }

  const std::array<lv_obj_t*, 4> amsButtons{{
      objects.home_ams_1,
      objects.home_ams_2,
      objects.home_active_ams,
      objects.home_ams_4,
  }};
  // AMS-Buttons zeigen keinen Text mehr, nur einen farbigen Rand am aktuell
  // gewaehlten AMS (Nutzerwunsch). Die eigentliche Faerbung je Slot passiert
  // ueber vier vom Nutzer im EEZ-Projekt angelegte Container-Objekte pro
  // Button (home_ams_<N>_1..4, per Slot-Farbe wenn belegt), nicht mehr ueber
  // dynamisch erzeugte Overlay-Quadrate.
  const std::array<std::array<lv_obj_t*, 4>, 4> amsSlotContainers{{
      {{objects.home_ams_1_1, objects.home_ams_1_2, objects.home_ams_1_3,
        objects.home_ams_1_4}},
      {{objects.home_ams_2_1, objects.home_ams_2_2, objects.home_ams_2_3,
        objects.home_ams_2_4}},
      {{objects.home_ams_3_1, objects.home_ams_3_2, objects.home_ams_3_3,
        objects.home_ams_3_4}},
      {{objects.home_ams_4_1, objects.home_ams_4_2, objects.home_ams_4_3,
        objects.home_ams_4_4}},
  }};
  for (std::uint8_t amsId = 1; amsId <= amsButtons.size(); ++amsId) {
    lv_obj_t* button = amsButtons[amsId - 1U];
    const auto& ams = amsEntries[amsId - 1U];
    const bool available = ams.present;
    setButtonText(button, "");
    lv_obj_set_state(button, LV_STATE_DISABLED, false);
    lv_obj_set_flag(button, LV_OBJ_FLAG_CLICKABLE, available);
    setButtonColors(button, kColorAmsButtonBackground);
    const bool selected = available && amsId == currentAmsId;
    lv_obj_set_style_border_width(button, selected ? 3 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(kColorWarningAmber), LV_PART_MAIN);
    for (std::uint8_t slot = 0; slot < 4; ++slot) {
      lv_obj_t* container = amsSlotContainers[amsId - 1U][slot];
      if (!available) {
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        continue;
      }
      const TrayUiEntry* tray = trayUiEntry(amsId, slot);
      const std::uint32_t color = tray != nullptr && tray->occupied
                                      ? parseTrayColorHex(tray->colorHex)
                                      : kColorNeutralGrey;
      lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_color(container, lv_color_hex(color), LV_PART_MAIN);
    }
  }

  updateTrayButton(objects.home_tray_1__tray, objects.home_tray_1__label,
                   objects.home_tray_1__color_1, objects.home_tray_1__color_2,
                   objects.home_tray_1__spoolmanager_id_container,
                   objects.home_tray_1__spoolmanager_id,
                   objects.home_tray_1__nozzle_icon, currentPrinterId,
                   currentAmsId, 0, "Slot 1");
  updateTrayButton(objects.home_tray_2__tray, objects.home_tray_2__label,
                   objects.home_tray_2__color_1, objects.home_tray_2__color_2,
                   objects.home_tray_2__spoolmanager_id_container,
                   objects.home_tray_2__spoolmanager_id,
                   objects.home_tray_2__nozzle_icon, currentPrinterId,
                   currentAmsId, 1, "Slot 2");
  updateTrayButton(objects.home_tray_3__tray, objects.home_tray_3__label,
                   objects.home_tray_3__color_1, objects.home_tray_3__color_2,
                   objects.home_tray_3__spoolmanager_id_container,
                   objects.home_tray_3__spoolmanager_id,
                   objects.home_tray_3__nozzle_icon, currentPrinterId,
                   currentAmsId, 2, "Slot 3");
  updateTrayButton(objects.home_tray_4__tray, objects.home_tray_4__label,
                   objects.home_tray_4__color_1, objects.home_tray_4__color_2,
                   objects.home_tray_4__spoolmanager_id_container,
                   objects.home_tray_4__spoolmanager_id,
                   objects.home_tray_4__nozzle_icon, currentPrinterId,
                   currentAmsId, 3, "Slot 4");
  updateTrayButton(objects.home_tray_external__tray,
                   objects.home_tray_external__label,
                   objects.home_tray_external__color_1,
                   objects.home_tray_external__color_2,
                   objects.home_tray_external__spoolmanager_id_container,
                   objects.home_tray_external__spoolmanager_id,
                   objects.home_tray_external__nozzle_icon, currentPrinterId,
                   0xFF, 0xFF, "Extern");

  const auto& staging = stagingState;
  char stagingText[64];
  // Staging ist druckerunabhaengig (die AMS-Zuordnung eines gestagten Spools
  // erfolgt separat ueber ConfigureSlotFromStaging mit explizitem
  // printerId) -- stagingState.printerId wird nirgends gesetzt und war
  // dadurch strukturell immer 0, waehrend currentPrinterId nach der
  // Druckerkonfiguration ungleich 0 ist. Der Vergleich war folglich immer
  // falsch und zeigte den Staging-Button auf Home immer als leer an, auch
  // wenn tatsaechlich eine Spule gestagt war.
  const std::uint32_t stagingBackgroundColor =
      staging.colorCount > 0 ? staging.colorRgb[0] : kColorNeutralGrey;
  if (staging.spoolId != 0) {
    // K-Faktor ist auf Nutzerwunsch vorerst Mockdaten (wie bei den
    // AMS-Faechern, siehe updateTrayButton()); staging.spoolId dagegen ist
    // eine echte Spoolman-ID (ueber den Spulen-Picker zugeordnet), keine
    // Mockdaten -- direkt anzeigen statt eine erfundene ID zu generieren.
    const std::uint32_t mockKFactorThousandths =
        20U + static_cast<std::uint32_t>(staging.spoolId % 5U);
    std::snprintf(stagingText, sizeof(stagingText), "%s\n%.0fg\nK (%u.%03u)",
                  staging.material,
                  static_cast<double>(staging.remainingWeightGrams),
                  static_cast<unsigned>(mockKFactorThousandths / 1000U),
                  static_cast<unsigned>(mockKFactorThousandths % 1000U));
    setButtonColors(objects.staging__staging, stagingBackgroundColor);
    updateHomeColorSwatches(objects.staging__color_3, objects.staging__color_4,
                            staging.colorRgb, staging.colorCount);
    lv_obj_remove_flag(objects.staging__spoolmanager_id_container,
                       LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(objects.staging__spoolmanager_id, LV_OBJ_FLAG_HIDDEN);
    char spoolIdText[12];
    std::snprintf(spoolIdText, sizeof(spoolIdText), "%lu",
                  static_cast<unsigned long>(staging.spoolId));
    setControlText(objects.staging__spoolmanager_id, spoolIdText);
  } else {
    std::snprintf(stagingText, sizeof(stagingText), "leer");
    setButtonColors(objects.staging__staging, kColorNeutralGrey);
    updateHomeColorSwatches(objects.staging__color_3, objects.staging__color_4,
                            {}, 0);
    lv_obj_add_flag(objects.staging__spoolmanager_id_container,
                    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.staging__spoolmanager_id, LV_OBJ_FLAG_HIDDEN);
  }
  setControlText(objects.staging__label, stagingText);
  lv_obj_set_style_text_color(
      objects.staging__staging_label,
      lv_color_hex(isLightBackground(stagingBackgroundColor) ? kColorTextDark
                                                              : kColorTextWhite),
      LV_PART_MAIN);

  updateWeightDisplays();
}

void updateWeightDisplays() {
  const models::UiWeightState& weight = liveWeight;
  char text[96];
  if (weight.error) {
    std::snprintf(text, sizeof(text), "Fehler %s", weight.status);
    setButtonColors(objects.home_weight, kColorDangerRed);
  } else if (!weight.calibrated) {
    std::snprintf(text, sizeof(text), "-- g nicht kalibriert");
    setButtonColors(objects.home_weight, kColorWarningAmber);
  } else {
    std::snprintf(text, sizeof(text), "%.1fg %s",
                  static_cast<double>(weight.grossWeightGrams), weight.status);
    setButtonColors(objects.home_weight, weight.stable ? kColorSuccessGreen : kColorWarningAmber);
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
                              lv_color_hex(weight.error ? kColorDangerRed
                                                        : (weight.stable
                                                               ? kColorSuccessGreen
                                                               : kColorWarningAmberDark)),
                              LV_PART_MAIN);
  std::snprintf(text, sizeof(text), "Kalibrierung: %s",
                weight.calibrated ? "geladen" : "nicht vorhanden");
  lv_label_set_text(objects.scale_settings_calibration, text);
}

void updateStagingContent() {
  const auto& staging = stagingState;
  const auto& spool = stagingSpoolState;
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
  std::snprintf(rowText, sizeof(rowText), "Leergewicht: %.0fg",
                static_cast<double>(spool.emptyWeightGrams));
  lv_label_set_text(stagingTableRows[4], rowText);
  std::snprintf(rowText, sizeof(rowText), "Bruttogewicht: %.0fg",
                static_cast<double>(staging.grossWeightGrams));
  lv_label_set_text(stagingTableRows[5], rowText);
  std::snprintf(rowText, sizeof(rowText), "Restgewicht: %.0f g (%.1f %%)",
                static_cast<double>(staging.remainingWeightGrams),
                static_cast<double>(remainingPercent));
  lv_label_set_text(stagingTableRows[6], rowText);
  std::snprintf(rowText, sizeof(rowText), "NFC: %s", currentNfcStatusText);
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
    lv_obj_set_style_border_color(field, lv_color_hex(kColorTextWhite), LV_PART_MAIN);
    lv_obj_set_style_radius(field, 4, LV_PART_MAIN);
    lv_obj_move_foreground(field);
  }
}

void updateTrayDetails() {
  // "Slot"-Tab: Belegung/Material/Farbe direkt aus dem MQTT-Report des
  // Druckers (AppTask::syncAmsToUi). "Spule"-Tab: die Spoolman-ID, die
  // diese Anwendung selbst beim Zuordnen (Phase 8.5/9.1/9.9) gesetzt hat --
  // der Drucker kennt keine Spoolman-IDs (docs/bambu-protocol.md), daher
  // gibt es hier bewusst keine Herstellername/Restgewicht-Anzeige ohne
  // zusaetzlichen Spoolman-Abruf; nur die bekannte ID wird gezeigt, nichts
  // erfunden.
  const TrayUiEntry* tray = trayUiEntry(selectedTrayAmsId, selectedTrayId);
  char title[48];
  if (selectedTrayId == 0xFF) {
    std::snprintf(title, sizeof(title), "Externer Slot");
  } else {
    std::snprintf(title, sizeof(title), "AMS %u | Slot %u",
                  selectedTrayAmsId, selectedTrayId + 1U);
  }
  lv_label_set_text(objects.tray_details_title, title);

  std::array<std::array<char, 96>, 6> rows{};
  const std::uint8_t colorCount = selectedTrayTab == 0 && tray != nullptr &&
                                          tray->occupied &&
                                          tray->colorHex[0] != '\0'
                                      ? 1U
                                      : 0U;
  const std::uint32_t colorRgb =
      colorCount > 0 ? parseTrayColorHex(tray->colorHex) : 0;
  if (tray == nullptr) {
    std::snprintf(rows[0].data(), rows[0].size(),
                  "Keine Slotdaten verf\xC3\xBCgbar");
  } else if (!tray->occupied) {
    std::snprintf(rows[0].data(), rows[0].size(), "Status: leer");
  } else if (selectedTrayTab == 0) {
    std::snprintf(rows[0].data(), rows[0].size(), "Status: belegt");
    std::snprintf(rows[1].data(), rows[1].size(), "Material (Drucker): %s",
                  tray->material[0] != '\0' ? tray->material : "unbekannt");
    if (colorCount > 0) {
      std::snprintf(rows[2].data(), rows[2].size(), "Farbe: #%06lX",
                    static_cast<unsigned long>(colorRgb));
    } else {
      std::snprintf(rows[2].data(), rows[2].size(), "Farbe: unbekannt");
    }
  } else {
    std::snprintf(rows[0].data(), rows[0].size(), "Status: belegt");
    if (tray->spoolId != 0) {
      std::snprintf(rows[1].data(), rows[1].size(), "Spoolman-ID: #%lu",
                    static_cast<unsigned long>(tray->spoolId));
    } else {
      std::snprintf(rows[1].data(), rows[1].size(),
                    "Keine Spoolman-Zuordnung bekannt");
      std::snprintf(rows[2].data(), rows[2].size(),
                    "(z. B. am Drucker manuell best\xC3\xBC" "ckt)");
    }
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
    if (index >= colorCount) {
      lv_obj_add_flag(field, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(field, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(field, lv_color_hex(colorRgb), LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, lv_color_hex(kColorTextDark), LV_PART_MAIN);
    lv_obj_set_style_radius(field, 4, LV_PART_MAIN);
    lv_obj_move_foreground(field);
  }
  lv_obj_set_style_bg_color(objects.tray_details_tab_slot,
                            lv_color_hex(selectedTrayTab == 0 ? kColorWarningAmber : kColorPrimaryBlue),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(objects.tray_details_tab_spool,
                            lv_color_hex(selectedTrayTab == 1 ? kColorWarningAmber : kColorPrimaryBlue),
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
  const auto& staging = stagingState;
  char title[80];
  std::snprintf(title, sizeof(title), "Zielslot für Spule %lu auswählen",
                static_cast<unsigned long>(staging.spoolId));
  lv_label_set_text(objects.tray_select_title, title);

  const std::array<lv_obj_t*, 4> amsButtons{{
      objects.tray_select_ams_1, objects.tray_select_ams_2,
      objects.tray_select_ams_3, objects.tray_select_ams_4,
  }};
  for (std::uint8_t id = 1; id <= amsButtons.size(); ++id) {
    const bool available = amsEntries[id - 1U].present;
    char label[12];
    std::snprintf(label, sizeof(label), "AMS %u", id);
    setControlText(amsButtons[id - 1U], label);
    lv_obj_set_flag(amsButtons[id - 1U], LV_OBJ_FLAG_CLICKABLE, available);
    lv_obj_set_style_bg_color(
        amsButtons[id - 1U],
        lv_color_hex(!available ? kColorDisabledGrey
                                : (id == currentAmsId ? kColorWarningAmber : kColorPrimaryBlue)),
        LV_PART_MAIN);
  }

  const std::array<lv_obj_t*, 4> slotButtons{{
      objects.tray_select_slot_1, objects.tray_select_slot_2,
      objects.tray_select_slot_3, objects.tray_select_slot_4,
  }};
  for (std::uint8_t id = 0; id < slotButtons.size(); ++id) {
    char label[32];
    const auto* tray = trayUiEntry(currentAmsId, id);
    std::snprintf(label, sizeof(label), "Slot %u\n%s", id + 1U,
                  tray != nullptr && tray->occupied ? "belegt" : "frei");
    setControlText(slotButtons[id], label);
    const bool highlighted = trayTargetSelected && selectedTrayAmsId == currentAmsId &&
                             selectedTrayId == id;
    lv_obj_set_style_bg_color(slotButtons[id],
                              lv_color_hex(highlighted ? kColorWarningAmber : kColorPrimaryBlue),
                              LV_PART_MAIN);
  }
  lv_obj_set_style_bg_color(
      objects.tray_select_external,
      lv_color_hex(trayTargetSelected && selectedTrayId == 0xFF ? kColorWarningAmber
                                                                 : kColorPrimaryBlue),
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

// Header-Statusicons (Nutzerwunsch, 2026-08-23): je Header ein Drucker-,
// WLAN- und Spoolman-Icon (scripts/add_header_status_icons.py legt genau
// ein Bild-Objekt je Status an, keine sichtbare/unsichtbare Variantenpaare)
// -- die Bildquelle wird hier zur Laufzeit auf die passende
// verbunden/getrennt-Grafik umgeschaltet, EEZ Studio besitzt weiterhin
// Position/Größe.
void updateHeaderStatusIcons() {
  const std::array<lv_obj_t*, 23> printerIcons{{
      objects.home_header_printer,
      objects.select_header_printer,
      objects.settings_header_printer,
      objects.staging_details_header_printer,
      objects.staging_actions_header_printer,
      objects.tray_details_header_printer,
      objects.tray_actions_header_printer,
      objects.tray_select_header_printer,
      objects.spoolman_settings_header_printer,
      objects.printer_settings_header_printer,
      objects.printer_edit_header_printer,
      objects.wifi_settings_header_printer,
      objects.scale_settings_header_printer,
      objects.device_settings_header_printer,
      objects.diagnostics_settings_header_printer,
      objects.firmware_settings_header_printer,
      objects.tag_action_header_printer,
      objects.tag_review_header_printer,
      objects.tag_write_header_printer,
      objects.tag_result_header_printer,
      objects.tag_definition_import_header_printer,
      objects.tag_legacy_header_printer,
      objects.tag_unknown_header_printer,
  }};
  const std::array<lv_obj_t*, 23> wifiIcons{{
      objects.home_header_wifi,
      objects.select_header_wifi,
      objects.settings_header_wifi,
      objects.staging_details_header_wifi,
      objects.staging_actions_header_wifi,
      objects.tray_details_header_wifi,
      objects.tray_actions_header_wifi,
      objects.tray_select_header_wifi,
      objects.spoolman_settings_header_wifi,
      objects.printer_settings_header_wifi,
      objects.printer_edit_header_wifi,
      objects.wifi_settings_header_wifi,
      objects.scale_settings_header_wifi,
      objects.device_settings_header_wifi,
      objects.diagnostics_settings_header_wifi,
      objects.firmware_settings_header_wifi,
      objects.tag_action_header_wifi,
      objects.tag_review_header_wifi,
      objects.tag_write_header_wifi,
      objects.tag_result_header_wifi,
      objects.tag_definition_import_header_wifi,
      objects.tag_legacy_header_wifi,
      objects.tag_unknown_header_wifi,
  }};
  const std::array<lv_obj_t*, 23> spoolmanIcons{{
      objects.home_header_spoolman,
      objects.select_header_spoolman,
      objects.settings_header_spoolman,
      objects.staging_details_header_spoolman,
      objects.staging_actions_header_spoolman,
      objects.tray_details_header_spoolman,
      objects.tray_actions_header_spoolman,
      objects.tray_select_header_spoolman,
      objects.spoolman_settings_header_spoolman,
      objects.printer_settings_header_spoolman,
      objects.printer_edit_header_spoolman,
      objects.wifi_settings_header_spoolman,
      objects.scale_settings_header_spoolman,
      objects.device_settings_header_spoolman,
      objects.diagnostics_settings_header_spoolman,
      objects.firmware_settings_header_spoolman,
      objects.tag_action_header_spoolman,
      objects.tag_review_header_spoolman,
      objects.tag_write_header_spoolman,
      objects.tag_result_header_spoolman,
      objects.tag_definition_import_header_spoolman,
      objects.tag_legacy_header_spoolman,
      objects.tag_unknown_header_spoolman,
  }};

  const PrinterUiEntry* printer = printerEntry(currentPrinterId);
  const bool printerConnected =
      printer != nullptr &&
      printer->connectionState == models::UiConnectionState::Connected;
  const bool wifiConnected = currentNetworkState == rtos::UiNetworkState::Online;
  const bool spoolmanConnected =
      filament_station::models::spoolmanOperationsAvailable(spoolmanAppState);

  for (lv_obj_t* icon : printerIcons)
    lv_image_set_src(icon, printerConnected ? &img_conneced_w : &img_disconneced_w);
  for (lv_obj_t* icon : wifiIcons)
    lv_image_set_src(icon, wifiConnected ? &img_wifi_connected_w : &img_wifi_disconnected_w);
  for (lv_obj_t* icon : spoolmanIcons)
    lv_image_set_src(icon, spoolmanConnected ? &img_spoolman_connected_w
                                             : &img_spoolman_disconneced);
}

void updateHeaders(rtos::PrinterId printerId) {
  // Printer identity comes from printerEntries, AMS presence/occupancy from
  // amsEntries/trayEntries -- both real, AppTask-synced data (see
  // AppTask::syncPrinterEntryToUi/syncAmsToUi). currentAmsId is a pure UI
  // navigation cursor (which AMS the user is currently looking at); it must
  // NOT be reset here, or every Bambu update would force it back to an
  // invalid "no AMS" state and the tray buttons would show "leer" forever
  // regardless of what syncAmsToUi actually reported.
  const PrinterUiEntry* printer = printerEntry(printerId);
  if (printer == nullptr || !printer->exists) {
    return;
  }
  currentPrinterId = printerId;

  // Titelleiste zeigt nur noch den Druckernamen -- Verbindungsstatus ist
  // jetzt das dedizierte Drucker-Statusicon (updateHeaderStatusIcons()),
  // welches AMS gerade betrachtet wird, ist am farbigen Rand des
  // AMS-Buttons auf Home selbst erkennbar (siehe updateHomeContent()).
  setAllHeaderTexts(printer->name);
  updateHeaderStatusIcons();

  updateHomeContent();
  updatePrinterList();
  updateStagingContent();
  updateTrayDetails();
  updateTraySelection(currentPrinterId, currentAmsId, 0, false);
  updatePrinterSettingsList();
}

void updateAmsOverview(rtos::PrinterId printerId, std::uint8_t amsId) {
  const PrinterUiEntry* printer = printerEntry(printerId);
  if (printerId != currentPrinterId || printer == nullptr || !printer->exists ||
      amsId < 1 || amsId > amsEntries.size() || !amsEntries[amsId - 1].present) {
    return;
  }
  currentAmsId = amsId;

  setAllHeaderTexts(printer->name);
  updateHeaderStatusIcons();
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
      std::snprintf(text, sizeof(text), "Heap: %lu B frei (min. %lu B)",
                    static_cast<unsigned long>(ESP.getFreeHeap()),
                    static_cast<unsigned long>(ESP.getMinFreeHeap()));
      lv_label_set_text(objects.diagnostics_settings_heap, text);
      std::snprintf(text, sizeof(text), "PSRAM: %lu B frei (min. %lu B)",
                    static_cast<unsigned long>(ESP.getFreePsram()),
                    static_cast<unsigned long>(ESP.getMinFreePsram()));
      lv_label_set_text(objects.diagnostics_settings_psram, text);
      // Task/Queue/Event-Bit-Diagnose kommt nur ueber den echten AppTask-
      // Roundtrip (siehe UpdateSettings/RefreshDiagnostics-Handling unten);
      // ohne bisherigen Roundtrip diese Sitzung neutralen Hinweis statt der
      // frueheren "Mock"-Platzhalterbeschriftung zeigen.
      if (!diagnosticsRefreshedThisSession) {
        lv_label_set_text(objects.diagnostics_settings_tasks,
                          "Aktualisieren antippen f\xC3\xBCr Task-Diagnose");
      }
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
  FS_LOGI(services::LogComponent::Ui, "EEZ screens created");
  // A one-tick delay can expire on the next tick boundary without IDLE0 ever
  // running. Reserve a real scheduling window during this one-time startup.
  vTaskDelay(pdMS_TO_TICKS(250));
  applyApplicationFont();
  FS_LOGI(services::LogComponent::Ui, "Application font applied");
  vTaskDelay(pdMS_TO_TICKS(250));
  bindGeneratedWidgets();
  applySpoolmanAppState();
  FS_LOGI(services::LogComponent::Ui, "Widgets bound");
  vTaskDelay(pdMS_TO_TICKS(250));
  updateHeaders(currentPrinterId);
  FS_LOGI(services::LogComponent::Ui, "Initial model applied");
  lv_mem_monitor_t memoryMonitor{};
  lv_mem_monitor(&memoryMonitor);
  FS_LOGD(services::LogComponent::Ui,
          "LVGL memory free_bytes=%lu largest_bytes=%lu fragmentation_pct=%u",
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
    case rtos::UiCommandType::UpdateProgress:
      // Only touch the overlay if it's still the one this update was meant
      // for -- the user may have closed/cancelled it, or a stale event for
      // an already-superseded request could still be in flight.
      if (activeOverlayKind != rtos::UiOverlayKind::None &&
          activeOverlayRequestId == command.requestId) {
        const std::int32_t clamped =
            command.value < 0 ? 0 : (command.value > 100 ? 100 : command.value);
        lv_bar_set_value(overlayProgress, clamped, LV_ANIM_ON);
        if (command.text[0] != '\0') lv_label_set_text(overlayText, command.text);
      }
      break;
    case rtos::UiCommandType::ShowDialog:
      showOverlay(command, false);
      break;
    case rtos::UiCommandType::HideProgress:
      hideOverlay();
      break;
    case rtos::UiCommandType::ShowScreen:
      if (command.screenId == rtos::UiScreenId::StagingActions) {
        const bool canAssign = (command.value & rtos::UI_TAG_CAP_LINK) != 0;
        const bool canRemove =
            (command.value & rtos::UI_TAG_CAP_UNLINK) != 0;
        currentTagCanAssign = canAssign;
        currentTagCanRemove = canRemove;
        setLabelButtonAvailable(objects.staging_action_link_tag, canAssign);
        setLabelButtonAvailable(objects.staging_action_unlink_tag, canRemove);
        char assignmentStatus[64]{};
        if (command.spoolId != 0) {
          std::snprintf(assignmentStatus, sizeof(assignmentStatus),
                        "Zugeordnet zu Spule #%lu",
                        static_cast<unsigned long>(command.spoolId));
        } else if (canAssign) {
          std::snprintf(assignmentStatus, sizeof(assignmentStatus),
                        "Nicht zugeordnet");
        } else {
          std::snprintf(assignmentStatus, sizeof(assignmentStatus),
                        "Kein NFC-Tag erkannt");
        }
        setControlText(objects.staging_action_write_tag, assignmentStatus);
      }
      if (command.screenId == rtos::UiScreenId::TagActionSelect &&
          command.text[0] != '\0') {
        lv_label_set_text(objects.tag_action_info, command.text);
        const bool canAssign = (command.value & rtos::UI_TAG_CAP_LINK) != 0;
        const bool canRemove =
            (command.value & rtos::UI_TAG_CAP_UNLINK) != 0;
        currentTagCanAssign = canAssign;
        currentTagCanRemove = canRemove;
        setLabelButtonAvailable(objects.tag_action_select_spool, canAssign);
        setLabelButtonAvailable(objects.tag_action_use_last_spool, canAssign);
        setLabelButtonAvailable(objects.tag_action_erase, canRemove);
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
        const bool canAssign = (command.value & rtos::UI_TAG_CAP_LINK) != 0;
        const bool canRemove =
            (command.value & rtos::UI_TAG_CAP_UNLINK) != 0;
        currentTagCanAssign = canAssign;
        currentTagCanRemove = canRemove;
        setLabelButtonAvailable(objects.tag_legacy_select_spool, canAssign);
        setLabelButtonAvailable(objects.tag_legacy_erase, canRemove);
      } else if (command.screenId == rtos::UiScreenId::TagUnknown &&
                  command.text[0] != '\0') {
        lv_label_set_text(objects.tag_unknown_summary, command.text);
        currentTagCanAssign =
            (command.value & rtos::UI_TAG_CAP_LINK) != 0;
        currentTagCanRemove = false;
        setLabelButtonAvailable(objects.tag_unknown_select_spool,
                                currentTagCanAssign);
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
      applySpoolmanAppState();
      showScreen(command.screenId);
      break;
    case rtos::UiCommandType::UpdateHeader:
      updateHeaders(command.printerId);
      break;
    case rtos::UiCommandType::UpdateAmsOverview:
      if (command.trayId == 0xFF) {
        // Sync message from AppTask::syncAmsToUi (real data), not a
        // navigation trigger -- see the matching encoding comment in
        // AppTask.cpp's syncAmsToUi.
        if (command.value >= 200 && command.amsId >= 1 &&
            command.amsId <= amsEntries.size()) {
          const std::int32_t flags = command.value - 200;
          auto& entry = amsEntries[command.amsId - 1];
          entry.present = (flags & 1) != 0;
          entry.occupiedTrayCount = static_cast<std::uint8_t>((flags >> 1) & 0x7);
        }
        updateHomeContent();
        if (currentAmsId == command.amsId)
          updateAmsOverview(command.printerId, command.amsId);
      } else {
        updateAmsOverview(command.printerId, command.amsId);
      }
      break;
    case rtos::UiCommandType::UpdateTrayDetails:
      if (command.value >= 300) {
        // Sync message from AppTask::syncAmsToUi (real data).
        TrayUiEntry* entry = nullptr;
        if (command.amsId == 0xFF) {
          entry = &externalTrayEntry;
        } else if (command.amsId >= 1 && command.amsId <= 4 &&
                   command.trayId < 4) {
          entry = &trayEntries[command.amsId - 1][command.trayId];
        }
        if (entry != nullptr) {
          entry->occupied = ((command.value - 300) & 1) != 0;
          entry->spoolId = command.spoolId;
          std::snprintf(entry->material, sizeof(entry->material), "%s",
                        command.title);
          std::snprintf(entry->colorHex, sizeof(entry->colorHex), "%s",
                        command.text);
        }
        updateHomeContent();
        updateTrayDetails();
      } else if (command.value == 2) {
        selectedTraySpoolId = command.spoolId;
        updateTraySelection(command.printerId, command.amsId, command.trayId,
                            true);
      } else {
        selectedTrayTab = command.value == 4 ? 1 : 0;
        updateTrayDetails();
      }
      break;
    case rtos::UiCommandType::UpdateStaging: {
      if (command.spoolId == 0) {
        // Clear Staging (Phase 9.8): reset to the same empty defaults the
        // struct starts with, no spool/weight data to reload or parse.
        stagingState = {};
        stagingSpoolState = {};
        updateStagingContent();
        // Home's staging widget (objects.staging) is only repainted by
        // updateHomeContent(); without this call it stayed stale until some
        // unrelated event happened to redraw Home, even after navigating
        // back there.
        updateHomeContent();
        break;
      }
      stagingState.spoolId = command.spoolId;
      stagingState.state = models::UiStagingState::WeightReady;
      const bool hasReloadedSpool = command.spool.id != 0;
      stagingState.remainingWeightGrams = hasReloadedSpool
                                               ? command.spool.remainingWeightGrams
                                               : command.weightUpdate.remainingWeightGrams;
      stagingState.grossWeightGrams = liveWeight.grossWeightGrams;
      stagingSpoolState.spoolId = command.spoolId;
      stagingSpoolState.remainingWeightGrams = stagingState.remainingWeightGrams;
      if (hasReloadedSpool) {
        stagingSpoolState.emptyWeightGrams = command.spool.emptyWeightGrams;
        stagingSpoolState.initialWeightGrams = command.spool.initialWeightGrams;
        std::snprintf(stagingState.vendor, sizeof(stagingState.vendor), "%s",
                      command.spool.vendor);
        std::snprintf(stagingState.material, sizeof(stagingState.material), "%s",
                      command.spool.material);
        std::snprintf(stagingSpoolState.vendor,
                      sizeof(stagingSpoolState.vendor), "%s",
                      command.spool.vendor);
        std::snprintf(stagingSpoolState.filament,
                      sizeof(stagingSpoolState.filament), "%s",
                      command.spool.filament);
        std::snprintf(stagingSpoolState.material,
                      sizeof(stagingSpoolState.material), "%s",
                      command.spool.material);
      }
      char vendor[32]{}, filament[40]{}, material[24]{};
      float emptyWeightGrams = command.weightUpdate.emptySpoolWeightGrams;
      float initialWeightGrams = command.weightUpdate.initialWeightGrams;
      const int parsed = std::sscanf(
          command.text, "%31[^|]|%39[^|]|%23[^|]|%f|%f", vendor, filament,
          material, &emptyWeightGrams, &initialWeightGrams);
      if (parsed >= 3) {
        std::snprintf(stagingState.vendor, sizeof(stagingState.vendor), "%s",
                      vendor);
        std::snprintf(stagingState.material,
                      sizeof(stagingState.material), "%s", material);
        std::snprintf(stagingSpoolState.vendor,
                      sizeof(stagingSpoolState.vendor), "%s", vendor);
        std::snprintf(stagingSpoolState.filament,
                      sizeof(stagingSpoolState.filament), "%s", filament);
        std::snprintf(stagingSpoolState.material,
                      sizeof(stagingSpoolState.material), "%s", material);
      }
      stagingSpoolState.emptyWeightGrams = emptyWeightGrams;
      stagingSpoolState.initialWeightGrams = initialWeightGrams;
      stagingState.colorCount = 0;
      stagingSpoolState.colorCount = 0;
      if (hasReloadedSpool) {
        for (std::uint8_t index = 0;
             index < command.spool.colorCount &&
             index < models::kMaximumFilamentColors;
             ++index) {
          std::uint32_t rgb = 0;
          if (!parseSpoolPickerColor(command.spool.colorHex[index], rgb))
            continue;
          stagingState.colorRgb[stagingState.colorCount++] = rgb;
          stagingSpoolState.colorRgb[stagingSpoolState.colorCount++] = rgb;
        }
      }
      for (std::uint8_t index = 0;
           !hasReloadedSpool && index < command.spoolColorCount &&
           index < models::kMaximumFilamentColors;
           ++index) {
        std::uint32_t rgb = 0;
        if (!parseSpoolPickerColor(command.spoolColorHex[index], rgb))
          continue;
        stagingState.colorRgb[stagingState.colorCount] = rgb;
        stagingSpoolState.colorRgb[stagingSpoolState.colorCount] = rgb;
        ++stagingState.colorCount;
        ++stagingSpoolState.colorCount;
      }
      updateStagingContent();
      // See the matching comment on the clear-staging branch above.
      updateHomeContent();
      break;
    }
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
    case rtos::UiCommandType::UpdateNetworkStatus: {
      currentNetworkState = command.networkState;
      updateHeaderStatusIcons();
      const char* statusText = "Offline";
      if (command.networkState == rtos::UiNetworkState::Connecting)
        statusText = "Verbunden, IP-Adresse wird bezogen";
      else if (command.networkState == rtos::UiNetworkState::Online)
        statusText = "Online";
      else if (command.networkState == rtos::UiNetworkState::PortalActive)
        statusText = "Konfigurationsportal aktiv";
      else if (command.networkState ==
               rtos::UiNetworkState::CredentialsCleared)
        statusText = "Zugangsdaten gel\xC3\xB6scht";

      char text[192]{};
      std::snprintf(text, sizeof(text),
                    "Status: %s\nNeu konfigurieren startet das WLAN-Portal.",
                    statusText);
      lv_label_set_text(objects.wifi_settings_status, text);
      std::snprintf(text, sizeof(text), "SSID: %s",
                    command.title[0] != '\0' ? command.title : "-");
      lv_label_set_text(objects.wifi_settings_ssid, text);
      if (command.networkState == rtos::UiNetworkState::Online) {
        std::snprintf(text, sizeof(text), "IP: %s | Signal: %ld dBm",
                      command.text[0] != '\0' ? command.text : "-",
                      static_cast<long>(command.value));
      } else {
        std::snprintf(text, sizeof(text), "IP: %s | Signal: -",
                      command.text[0] != '\0' ? command.text : "-");
      }
      lv_label_set_text(objects.wifi_settings_ip, text);
      break;
    }
    case rtos::UiCommandType::UpdateSpoolmanState:
      spoolmanAppState = command.spoolmanAppState;
      updateHeaderStatusIcons();
      applySpoolmanAppState(&command);
      break;
    case rtos::UiCommandType::UpdateSpoolPicker: {
      if (activeOverlayKind != rtos::UiOverlayKind::SpoolPicker) break;
      if (command.value == -2) {
        spoolPickerIds.fill(0);
        lv_obj_scroll_to_y(spoolPickerList, 0, LV_ANIM_OFF);
        for (std::size_t index = 0; index < spoolPickerButtons.size(); ++index) {
          lv_obj_add_flag(spoolPickerButtons[index], LV_OBJ_FLAG_HIDDEN);
          for (lv_obj_t* panel : spoolPickerColorPanels[index])
            lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(overlayTitle, command.text);
      } else if (command.value >= 0 &&
                 command.value < static_cast<std::int32_t>(spoolPickerButtons.size())) {
        const std::size_t index = static_cast<std::size_t>(command.value);
        spoolPickerIds[index] = command.spoolId;
        lv_label_set_text(spoolPickerLabels[index], command.text);
        applySpoolPickerColors(index, command);
        if (!spoolPickerInputActive)
          lv_obj_remove_flag(spoolPickerButtons[index], LV_OBJ_FLAG_HIDDEN);
      } else if (command.value == -1) {
        char title[64]{};
        std::snprintf(title, sizeof(title), "Spule ausw\xC3\xA4hlen (%lu)",
                      static_cast<unsigned long>(command.spoolId));
        lv_label_set_text(overlayTitle, title);
      }
      break;
    }
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
        if (command.value >= 120) {
          // Absolute sync from AppTask's real printerConfigs/printerCollection
          // (Phase 8.6), encoded as 120 + bitmask(enabled=1, isDefault=2,
          // isActive=4) + (connectionState << 3) -- subtracted before
          // extracting bits so the base value's own bit pattern cannot bleed
          // into the flags.
          const std::int32_t flags = command.value - 120;
          entry->exists = true;
          std::snprintf(entry->name, sizeof(entry->name), "%s", command.title);
          entry->enabled = (flags & 1) != 0;
          entry->isDefault = (flags & 2) != 0;
          entry->isActive = (flags & 4) != 0;
          entry->connectionState =
              static_cast<models::UiConnectionState>((flags >> 3) & 0x7);
        }
      }
      updatePrinterSettingsList();
      break;
    }
    case rtos::UiCommandType::ShowStatus:
    case rtos::UiCommandType::ShowToast:
      if (command.type == rtos::UiCommandType::ShowStatus &&
          std::strcmp(command.title, "NFC") == 0 && command.text[0] != '\0') {
        std::snprintf(currentNfcStatusText, sizeof(currentNfcStatusText), "%s",
                      command.text);
      }
      lv_label_set_text(objects.home_bottom_status, command.text);
      lv_label_set_text(objects.settings_bottom_status, command.text);
      if (command.value >= 100) {
        lv_label_set_text(objects.spoolman_setting_status, command.text);
        char version[64];
        std::snprintf(version, sizeof(version), "Server: %s",
                      command.title[0] != '\0' ? command.title : "-");
        lv_label_set_text(objects.spoolman_setting_version, version);
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
        std::snprintf(text, sizeof(text), "Heap: %lu B frei (min. %lu B)",
                      static_cast<unsigned long>(ESP.getFreeHeap()),
                      static_cast<unsigned long>(ESP.getMinFreeHeap()));
        lv_label_set_text(objects.diagnostics_settings_heap, text);
        std::snprintf(text, sizeof(text), "PSRAM: %lu B frei (min. %lu B)",
                      static_cast<unsigned long>(ESP.getFreePsram()),
                      static_cast<unsigned long>(ESP.getMinFreePsram()));
        lv_label_set_text(objects.diagnostics_settings_psram, text);
        // Vollstaendiger Bericht (Stack/Runtime-Status je Task, Queues,
        // Event-Bits) steht als strukturierte Zeilen im Log (siehe
        // AppTask::logTaskDiagnostics, Phase 10.1); command.text traegt nur
        // die Kurzzusammenfassung fuer dieses kleine Label.
        lv_label_set_text(objects.diagnostics_settings_tasks, command.text);
        diagnosticsRefreshedThisSession = true;
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
