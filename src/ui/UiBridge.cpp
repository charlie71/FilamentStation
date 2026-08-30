/**
 * @file
 * @brief Implements the ui::UiBridge.h entry points: LVGL initialization,
 *        the generated-UI event callbacks, and every UiCommand renderer.
 *        This file is UiTask's exclusive gateway to LVGL -- all module
 *        state below is single-task-owned and never touched from any
 *        other task.
 */
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
#include "ui/generated/styles.h"
#include "ui/generated/ui.h"
#include "ui/models/UiModels.h"

extern "C" {
extern const lv_font_t ui_font_ui_german16;  ///< EEZ-generated LVGL font covering the German character set, used across all hand-written UI text.
}

namespace filament_station::ui {
namespace {

// GUI-Farbpalette (Nutzerwunsch, 2026-08-22): jede in der UI verwendete
// Farbe als benannte, kommentierte Konstante statt als verstreutes
// 0xRRGGBB-Literal. Werte unveraendert aus dem bisherigen Code uebernommen,
// nur benannt/dokumentiert.
constexpr std::uint32_t kColorPrimaryBlue = 0x1565C0;    ///< Primaerfarbe: aktive/verfuegbare Buttons, "Zurueck"-Aktionen, Standardfarbe.
constexpr std::uint32_t kColorNeutralGrey = 0x455A64;    ///< Sekundaerfarbe: Abbrechen/neutrale Buttons, leerer Slot/keine Daten.
constexpr std::uint32_t kColorDisabledGrey = 0x616161;   ///< Deaktiviert/nicht verfuegbar (AMS-Button, freier Druckerplatz, ...).
constexpr std::uint32_t kColorDangerRed = 0xC62828;      ///< Destruktive Aktion (Loeschen/Entfernen/Leeren), Fehlerzustand.
constexpr std::uint32_t kColorSuccessGreen = 0x2E7D32;   ///< Erfolg: Drucker verbunden, Gewicht stabil.
constexpr std::uint32_t kColorWarningAmber = 0xF9A825;   ///< Warnung/Hervorhebung: ausgewaehltes AMS (Rand), Gewicht nicht stabil.
constexpr std::uint32_t kColorWarningAmberDark = 0xB26A00;  ///< Textvariante zu kColorWarningAmber auf hellem Grund.
constexpr std::uint32_t kColorManagedPrinterOrange = 0xEF6C00;  ///< Hervorhebung: der in den Druckereinstellungen aktuell bearbeitete Drucker.
constexpr std::uint32_t kColorTextWhite = 0xFFFFFF;      ///< Text/Rahmen auf dunklem Grund.
constexpr std::uint32_t kColorTextDark = 0x101820;       ///< Text/Rahmen auf hellem Grund.
constexpr std::uint32_t kColorTextDisabled = 0xD7DCE0;   ///< Beschriftung auf deaktivierten Buttons.
constexpr std::uint32_t kColorPanelLight = 0xECEFF1;     ///< Helle Panel-/Listenzeilen-Hintergruende.
constexpr std::uint32_t kColorPanelLightAlt = 0xB8BDC0;  ///< Abwechselnde (ungerade) Tabellenzeile.
constexpr std::uint32_t kColorPanelLightest = 0xF4F6F8;  ///< Overlay-Panel-Hintergrund.
constexpr std::uint32_t kColorOverlayBackdrop = 0x000000;   ///< Abgedunkelter Hintergrund hinter Overlays/Dialogen.
constexpr std::uint32_t kColorSpoolPickerRow = 0xB0BEC5;    ///< Default-Hintergrund einer Zeile im Spulen-Picker.
constexpr std::uint32_t kColorOverlayCancelButton = 0x607D8B;  ///< Neutrale Abbrechen-Schaltflaeche im Overlay.
constexpr std::uint32_t kColorInactivePrinterRow = 0x78909C;   ///< Deaktivierter/nicht existierender Drucker in Einstellungslisten.
constexpr std::uint32_t kColorAmsButtonBackground = 0x263238;  ///< Neutraler Hintergrund der Home-AMS-Buttons (Farbe kommt aus den Slot-Feldern).
// Touch-Kalibrierungsmarker (Diagnose-Screen): 5 gut unterscheidbare Farben im Zyklus.
constexpr std::uint32_t kColorTouchMarker1 = 0xFFEB3B;  ///< Touch-Diagnosemarker Farbe 1.
constexpr std::uint32_t kColorTouchMarker2 = 0x00E676;  ///< Touch-Diagnosemarker Farbe 2.
constexpr std::uint32_t kColorTouchMarker3 = 0x00BCD4;  ///< Touch-Diagnosemarker Farbe 3.
constexpr std::uint32_t kColorTouchMarker4 = 0xFF4081;  ///< Touch-Diagnosemarker Farbe 4.
constexpr std::uint32_t kColorTouchMarker5 = 0xFF9100;  ///< Touch-Diagnosemarker Farbe 5.

void* drawBuffer1 = nullptr;               ///< First PSRAM-backed LVGL draw buffer.
void* drawBuffer2 = nullptr;               ///< Second PSRAM-backed LVGL draw buffer.
lv_display_t* lvglDisplay = nullptr;       ///< The registered LVGL display object.
lv_indev_t* touchInput = nullptr;          ///< The registered LVGL touch input device.
rtos::RtosContext* rtosContext = nullptr;  ///< Owning RTOS context, set once by initializeLvgl().
rtos::PrinterId currentPrinterId = 1;      ///< Printer currently shown on Home/Header/AMS.
std::uint8_t currentAmsId = 1;             ///< AMS unit currently shown/selected.
std::uint8_t selectedTrayAmsId = 1;        ///< AMS unit of the currently selected tray.
std::uint8_t selectedTrayId = 0;           ///< Currently selected tray index.
rtos::SpoolId selectedTraySpoolId = 0;     ///< Spool id resolved for the currently selected tray.
std::uint8_t selectedTrayTab = 0;          ///< Currently selected tray-detail tab index.
bool trayTargetSelected = false;           ///< Whether a tray target has been selected for a pending slot assignment.
/// @brief Editable text-field draft backing the Spoolman settings editor widget.
struct SpoolmanUiDraft {
  char name[32] = "Werkstatt";           ///< Display name field.
  char protocol[8] = "http";             ///< "http" or "https" field.
  char host[64] = "spoolman.local";      ///< Hostname/IP field.
  char port[8] = "7912";                 ///< Port field, as text.
  char basePath[32] = "/api/v1";         ///< API base path field.
  char timeoutMs[8] = "5000";            ///< Request timeout field, as text.
};
SpoolmanUiDraft spoolmanDraft{};  ///< Active Spoolman-settings editor draft (mirrors AppTask's own draft for local editing).
lv_obj_t* spoolmanEditor = nullptr;    ///< Spoolman settings editor widget/screen root.
lv_obj_t* spoolmanKeyboard = nullptr;  ///< On-screen keyboard bound to the Spoolman editor.
std::int32_t activeSpoolmanField = 0;  ///< Field index currently being edited in #spoolmanEditor.
/// @brief Which text editor (if any) is currently open, disambiguating the shared keyboard widget's target.
enum class EditorContext : std::uint8_t { None, Spoolman, Printer };
EditorContext editorContext = EditorContext::None;  ///< Currently open editor context.
std::int32_t activePrinterField = 0;    ///< Field index currently being edited in the printer editor.
rtos::PrinterId managedPrinterId = 1;   ///< Printer currently selected in the printer-management list.
rtos::PrinterId editingPrinterId = 1;   ///< Printer currently open in the printer editor.
bool showPrinterAccessCode = false;     ///< Whether the printer editor currently reveals the LAN access code.
/// @brief One printer's display-list entry (name, flags, connection state), local UI copy of AppTask's roster.
struct PrinterUiEntry {
  rtos::PrinterId id;      ///< Printer id.
  char name[32];           ///< Display name.
  bool enabled;             ///< Whether the printer is enabled.
  bool isDefault;           ///< Whether this is the default printer.
  bool isActive;            ///< Whether this printer is currently active/selected.
  bool exists;               ///< Whether this slot corresponds to a real, configured printer.
  models::UiConnectionState connectionState = models::UiConnectionState::Offline;  ///< Current connection state.
  std::uint8_t amsCount = 0;  ///< Number of present AMS units.
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
}};  ///< Fixed-size printer display-list, kept in sync with AppTask's own roster.
/// @brief One AMS unit's display-list summary.
struct AmsUiEntry {
  bool present = false;               ///< Whether this unit is currently present.
  std::uint8_t occupiedTrayCount = 0;  ///< Number of occupied trays.
};
// Real AMS/tray data synced from AppTask (see AppTask::syncAmsToUi), fed via
// UpdateAmsOverview/UpdateTrayDetails; empty by default (no AMS present)
// instead of the former static MockUiDataProvider data.
std::array<AmsUiEntry, 4> amsEntries{};  ///< Display-list of AMS units for #currentPrinterId.
/// @brief One tray's display state (material/color/spool/weight), local UI copy synced from AppTask.
struct TrayUiEntry {
  bool occupied = false;   ///< Whether the tray currently holds filament.
  // 24 bytes to match models::SpoolmanSpool::material -- this is filled via
  // snprintf from command.title (UpdateTrayDetails), which is either the
  // printer-reported Bambu tray_type (fallback, <=16 bytes, see
  // models::PrinterSlotStateData::material) or, once resolved, the actually
  // assigned Spoolman material (AppTask::syncAmsToUi(), Nutzerbericht
  // 2026-08-28 -- e.g. "Support For PLA" vs. the printer's own "PLA-S").
  // A smaller buffer here previously truncated long values (e.g. "Support
  // for PLA" -> "Support for") on the tray-card display.
  char material[24]{};      ///< Material name to display (Spoolman material once resolved, otherwise the printer-reported tray_type).
  char colorHex[9]{};       ///< Color reported by the printer.
  rtos::SpoolId spoolId = 0;  ///< Resolved Spoolman spool id, or 0 if unknown.
  // Restgewicht/K-Faktor aus Spoolman -- nur aussagekraeftig, wenn
  // detailsLoaded true ist (AppTask::resolveTraySpoolDetails() hat den
  // asynchronen Spoolman-Abruf abgeschlossen); vorher/bei unbekannter
  // Zuordnung bleiben sie auf 0/false und duerfen nicht angezeigt werden
  // (0.0F waere sonst nicht von einem echten Restgewicht 0 unterscheidbar),
  // siehe UiBridge.cpp::updateTrayButton().
  bool detailsLoaded = false;         ///< Whether #remainingWeightGrams/#kFactor are valid yet.
  float remainingWeightGrams = 0.0F;  ///< Resolved remaining weight, only valid if #detailsLoaded.
  bool kFactorValid = false;          ///< Whether #kFactor is valid.
  float kFactor = 0.0F;               ///< Resolved flow-dynamics K-factor, only valid if #kFactorValid.
  // Ob dieses Fach laut Drucker gerade in der Duese aktiv ist ("tray_now",
  // Nutzerwunsch 2026-08-24) -- siehe AppTask::syncAmsToUi()'s
  // UpdateTrayDetails-value-Kodierung fuer die Gegenseite.
  bool isActiveNozzle = false;  ///< Whether this tray is currently loaded into the nozzle.
};
std::array<std::array<TrayUiEntry, 4>, 4> trayEntries{};  ///< Display-list of trays, indexed [amsId][trayId], for #currentPrinterId.
TrayUiEntry externalTrayEntry{};  ///< Display state of the external/manual spool holder.
/// @brief Editable text-field draft backing the printer add/edit screen widget.
struct PrinterUiDraft {
  char name[32];        ///< Display name field.
  char host[64];        ///< Host/IP field.
  char serial[32];      ///< Serial number field.
  char accessCode[16];  ///< LAN access code field.
};
PrinterUiDraft printerUiDraft{};  ///< Active printer-settings editor draft.
constexpr const char* kKeyboardLowerMap[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "Entf.", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "OK", "\n",
    "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
    "ABC", "123", "<", "Leer", ">", "Abbr.", ""};  ///< Lowercase on-screen keyboard layout.
constexpr const char* kKeyboardUpperMap[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "Entf.", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "OK", "\n",
    "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
    "abc", "123", "<", "Leer", ">", "Abbr.", ""};  ///< Uppercase on-screen keyboard layout.
constexpr const char* kKeyboardNumberMap[] = {
    "1", "2", "3", "Entf.", "\n", "4", "5", "6", "Abbr.", "\n",
    "7", "8", "9", "OK", "\n", "ABC", "0", ".", "<", ">", ""};  ///< Numeric on-screen keyboard layout.
std::uint32_t nextRequestId = 100;  ///< Next locally-generated UiAction correlation id.
models::UiWeightState liveWeight{0.0F, 0.0F, false, false, true,
                                 "wartet auf Messwert"};  ///< Current scale display state.
models::UiStagingSummary stagingState{};      ///< Current staged-spool display state.
models::UiSpoolSummary stagingSpoolState{};   ///< Full spool data for the currently staged spool.
lv_obj_t* calibrationEditor = nullptr;   ///< Scale calibration editor widget/screen root.
lv_obj_t* calibrationKeyboard = nullptr;  ///< On-screen keyboard bound to the calibration editor.
std::array<lv_obj_t*, 9> stagingTableRows{};  ///< Row widgets of the staging detail table.
bool touchWasPressed = false;            ///< Previous-frame touch press state, for edge detection.
std::size_t touchMarkerColorIndex = 0;   ///< Index into the touch-marker color cycle.
lv_obj_t* overlayBackdrop = nullptr;  ///< Dimmed backdrop behind the active overlay.
lv_obj_t* overlayPanel = nullptr;     ///< Active overlay's panel container.
lv_obj_t* overlayTitle = nullptr;     ///< Active overlay's title label.
lv_obj_t* overlayText = nullptr;      ///< Active overlay's body text label.
lv_obj_t* overlayProgress = nullptr;  ///< Active overlay's progress indicator, if any.
lv_obj_t* overlayCancel = nullptr;    ///< Active overlay's cancel button, if any.
lv_obj_t* overlayConfirm = nullptr;   ///< Active overlay's confirm button, if any.
std::array<lv_obj_t*, 4> advancedModeButtons{};  ///< Mode-select buttons on the advanced weighing screen.
constexpr std::size_t kMaximumSpoolPickerResults = 20;  ///< Maximum spool-picker rows rendered.
constexpr std::int32_t kSpoolPickerRowWidth = 444;   ///< Spool-picker row width in pixels.
constexpr std::int32_t kSpoolPickerRowHeight = 46;   ///< Spool-picker row height in pixels.
constexpr std::int32_t kSpoolPickerRowPitch = 48;    ///< Spool-picker row vertical pitch in pixels.
std::array<lv_obj_t*, kMaximumSpoolPickerResults> spoolPickerButtons{};  ///< Spool-picker row button widgets.
std::array<std::array<lv_obj_t*,
                      filament_station::models::SpoolmanSpool::kMaximumColors>,
           kMaximumSpoolPickerResults>
    spoolPickerColorPanels{};  ///< Per-row, per-color swatch widgets in the spool picker.
std::array<lv_obj_t*, kMaximumSpoolPickerResults> spoolPickerLabels{};  ///< Spool-picker row label widgets.
std::array<rtos::SpoolId, kMaximumSpoolPickerResults> spoolPickerIds{};  ///< Spool id shown on each spool-picker row.
lv_obj_t* spoolPickerSearch = nullptr;        ///< Spool-picker search text field.
lv_obj_t* spoolPickerFilterButton = nullptr;  ///< Spool-picker search-filter toggle button.
lv_obj_t* spoolPickerList = nullptr;          ///< Spool-picker scrollable list container.
lv_obj_t* spoolPickerScrollUp = nullptr;      ///< Spool-picker scroll-up button.
lv_obj_t* spoolPickerScrollDown = nullptr;    ///< Spool-picker scroll-down button.
lv_obj_t* spoolPickerKeyboard = nullptr;      ///< On-screen keyboard bound to the spool-picker search field.
std::uint8_t spoolPickerFilter = 0;      ///< Currently selected search filter (rtos::SpoolmanSearchFilter).
bool spoolPickerInputActive = false;     ///< Whether the spool-picker search field currently has input focus.
lv_obj_t* advancedInput = nullptr;       ///< Active numeric input field on the advanced weighing screen.
lv_obj_t* advancedKeyboard = nullptr;    ///< On-screen keyboard bound to #advancedInput.
std::int32_t advancedInputMode = 0;      ///< Which value #advancedInput currently edits.
rtos::UiOverlayKind activeOverlayKind = rtos::UiOverlayKind::None;  ///< Currently shown overlay kind.
std::uint32_t activeOverlayRequestId = 0;  ///< Correlation id of the currently shown overlay.
filament_station::models::SpoolmanAppState spoolmanAppState =
    filament_station::models::SpoolmanAppState::SpoolmanUnavailable;  ///< Current Spoolman connection/readiness state.
bool currentTagCanAssign = false;   ///< Whether the currently present tag can be assigned to a spool.
bool currentTagCanRemove = false;   ///< Whether the currently present tag's assignment can be removed.
// TagActionSelect's "ins Staging laden" button (TASKS.md Nachtrag
// 2026-08-29): unlike currentTagCanAssign/Remove (capability bits), this is
// the tag's already-resolved spool id itself (AppTask's authoritativeSpoolId,
// sent as ShowScreen's command.spoolId) -- 0 means the tag is not (yet)
// assigned to any Spoolman spool.
rtos::SpoolId currentTagSpoolId = 0;  ///< Spoolman spool the currently present tag already resolves to, or 0.
// CMP_TOP_PRINTER_BAR's "nfc" image (Nutzerwunsch 2026-08-30): unlike
// currentTagCanAssign/currentTagSpoolId (only meaningful on the tag-flow
// screens, updated from ShowScreen's command.value/spoolId), this drives a
// header icon shown on all 23 screens regardless of which one is active --
// set from the dedicated UpdateNfcPresence UiCommand AppTask sends on every
// physical tag detect/remove, independent of any resolution/navigation
// logic. See updateHeaderStatusIcons().
bool nfcTagPresent = false;  ///< Whether a tag is currently physically present on the NFC reader.
bool diagnosticsRefreshedThisSession = false;  ///< Whether the diagnostics screen has been refreshed at least once this session.
// Real WLAN/NFC connection status for the Home status summary -- previously
// this read from models::mock::settings() (always hardcoded "Connected")
// for WLAN/Spoolman, and stagingState.nfcStatus (never written anywhere)
// for NFC, so the whole summary was either fake or blank regardless of
// actual state.
rtos::UiNetworkState currentNetworkState = rtos::UiNetworkState::Offline;  ///< Current WiFi connection state, for the Home status summary.
char currentNfcStatusText[48] = "wird initialisiert";  ///< Current NFC status text, for the Home status summary.

/// @brief Sends a UiAction to AppTask.
/// @param type Action type.
/// @param printerId Target printer, if applicable.
/// @param amsId Target AMS index, if applicable.
/// @param trayId Target tray index, if applicable.
/// @param value Generic numeric payload.
/// @param spoolId Target spool, if applicable.
/// @param text Text payload, or null.
/// @return false if the app event queue was full.
bool sendAction(rtos::UiActionType type, rtos::PrinterId printerId,
                std::uint8_t amsId = 0, std::uint8_t trayId = 0,
                std::int32_t value = 0, rtos::SpoolId spoolId = 0,
                const char* text = nullptr);
/// @brief Re-renders the weight card from #liveWeight.
void updateWeightDisplays();
/// @brief Updates a tray button's selected-highlight state.
/// @param printerId Owning printer.
/// @param amsId AMS index.
/// @param trayId Tray index.
/// @param selected Whether the tray should be shown as selected.
void updateTraySelection(rtos::PrinterId printerId, std::uint8_t amsId,
                         std::uint8_t trayId, bool selected);

constexpr const char* kAdvancedNumberMap[] = {
    "1", "2", "3", "Entf.", "\n", "4", "5", "6", "Abbr.", "\n",
    "7", "8", "9", "OK", "\n", "0", ".", ""};  ///< Numeric on-screen keyboard layout for the advanced weighing input.

/// @brief LVGL click handler: selects a weighing mode on the advanced weighing screen.
/// @param event LVGL click event.
void advancedModeClicked(lv_event_t* event) {
  const auto mode = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::AdvancedWeight, currentPrinterId, 0, 0, mode);
}

/// @brief LVGL click handler: selects a spool row in the spool picker.
/// @param event LVGL click event.
void spoolPickerItemClicked(lv_event_t* event) {
  const auto index = static_cast<std::size_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  if (index >= spoolPickerIds.size() || spoolPickerIds[index] == 0) return;
  const rtos::SpoolId spoolId = spoolPickerIds[index];
  sendAction(rtos::UiActionType::SelectSpool, currentPrinterId, 0, 0, 0,
             spoolId);
}

/// @brief Parses a "#RRGGBB"/"RRGGBB" color string into a packed RGB888 value.
/// @param hex Color string to parse.
/// @param rgb Out parameter receiving the packed value.
/// @return false if `hex` is not a valid 6-digit hex color.
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

/// @brief Renders a spool picker row's color swatch panel(s) from a command's spool colors.
/// @param index Row index into #spoolPickerButtons/#spoolPickerColorPanels.
/// @param command Command carrying the spool's color(s).
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

/// @brief Creates one spool-picker row button with its color-swatch panels and label.
/// @param text Initial row label text.
/// @param y Row's vertical position within the picker list.
/// @param index Row index, used as the button's click-event user data.
/// @return The created button widget.
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

/// @brief Sends a SearchSpool action with the spool-picker's current search text/filter.
void submitSpoolPickerSearch() {
  if (spoolPickerSearch == nullptr ||
      lv_textarea_get_text(spoolPickerSearch)[0] == '\0')
    return;
  sendAction(rtos::UiActionType::SearchSpool, currentPrinterId, 0, 0,
             10 + spoolPickerFilter, 0,
             lv_textarea_get_text(spoolPickerSearch));
}

/// @brief LVGL click handler: scrolls the spool-picker list up/down.
/// @param event LVGL click event; its user data encodes the scroll direction.
void spoolPickerScrollClicked(lv_event_t* event) {
  if (spoolPickerList == nullptr) return;
  const auto direction = static_cast<std::int32_t>(
      reinterpret_cast<std::intptr_t>(lv_event_get_user_data(event)));
  lv_obj_scroll_by(spoolPickerList, 0, direction * (2 * kSpoolPickerRowPitch),
                   LV_ANIM_ON);
}

/// @brief LVGL change handler: updates #spoolPickerFilter from the filter dropdown.
void spoolPickerFilterChanged(lv_event_t*) {
  if (spoolPickerFilterButton != nullptr)
    spoolPickerFilter = static_cast<std::uint8_t>(
        lv_dropdown_get_selected(spoolPickerFilterButton));
}

/// @brief Toggles the spool picker between list-browsing and search-text-input mode.
/// @param active true to show the on-screen keyboard and hide the list, false for the reverse.
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

/// @brief LVGL keyboard handler: applies one keypress to the spool-picker search field.
/// @param event LVGL value-changed event from the on-screen keyboard.
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

/// @brief LVGL click handler: opens the on-screen keyboard for the spool-picker search field.
void spoolPickerTextareaClicked(lv_event_t*) {
  if (spoolPickerKeyboard == nullptr || spoolPickerSearch == nullptr) return;
  lv_buttonmatrix_set_map(spoolPickerKeyboard,
                          spoolPickerFilter == 3 ? kKeyboardNumberMap
                                                 : kKeyboardLowerMap);
  setSpoolPickerInputMode(true);
}

/// @brief LVGL keyboard handler: applies one keypress to the advanced-weighing numeric input.
/// @param event LVGL value-changed event from the on-screen keyboard.
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

/// @brief Creates one weighing-mode select button on the advanced weighing overlay.
/// @param text Button label.
/// @param x Button x position.
/// @param y Button y position.
/// @param mode Mode value, used as the button's click-event user data.
/// @return The created button widget.
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

/// @brief LVGL click handler: sends the action bound to a generic overlay button.
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
void overlayActionClicked(lv_event_t* event) {
  const auto action = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(action, currentPrinterId, currentAmsId, 0,
             static_cast<std::int32_t>(activeOverlayKind), 0, nullptr);
}

/// @brief Creates one generic action button on the overlay panel.
/// @param parent Parent widget (the overlay panel).
/// @param text Button label.
/// @param x Button x position.
/// @param color Button background color.
/// @param action Action to send when clicked.
/// @return The created button widget.
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

/// @brief Lazily creates the shared overlay widget tree (backdrop, panel, title/text/progress/buttons) on first use.
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

/// @brief Hides the active overlay/keyboard and clears the tracked overlay kind/requestId.
void hideOverlay() {
  if (overlayBackdrop != nullptr) lv_obj_add_flag(overlayBackdrop, LV_OBJ_FLAG_HIDDEN);
  if (spoolPickerKeyboard != nullptr)
    lv_obj_add_flag(spoolPickerKeyboard, LV_OBJ_FLAG_HIDDEN);
  activeOverlayKind = rtos::UiOverlayKind::None;
  activeOverlayRequestId = 0;
}

/// @brief Recolors the overlay's cancel button.
/// @param color Background color to apply.
void styleOverlayNavigation(std::uint32_t color) {
  if (overlayCancel == nullptr) return;
  lv_obj_set_style_bg_color(overlayCancel, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_t* label = lv_obj_get_child(overlayCancel, 0);
  if (label != nullptr)
    lv_obj_set_style_text_color(label, lv_color_hex(kColorTextWhite), LV_PART_MAIN);
}

/// @brief Configures and shows the shared overlay for a ShowDialog/ShowProgress command.
/// @param command Command carrying the overlay kind, title, text, and buttons to show.
/// @param progress Whether to show the progress-style layout instead of the dialog-style layout.
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
  lv_obj_set_pos(overlayProgress, 16, 126);
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
  if (command.overlayKind == rtos::UiOverlayKind::BootProgress) {
    // Up to four status lines ("Display bereit." plus SD/NFC/Waage, see
    // AppTask::refreshBootProgress()) need more room than the 100 px
    // default text box -- at the default size overlayText's bottom 26 px
    // sat behind overlayProgress (drawn afterwards, so on top), visibly
    // cutting off the fourth line (Nutzerbericht 2026-08-26). Unlike
    // AdvancedWeight above, the progress bar itself must stay visible here,
    // so it is pushed down together with the buttons instead of hidden.
    lv_obj_set_size(overlayPanel, 420, 300);
    lv_obj_center(overlayPanel);
    lv_obj_set_pos(overlayText, 16, 46);
    lv_obj_set_size(overlayText, 388, 150);
    lv_obj_set_pos(overlayProgress, 16, 204);
    lv_obj_set_y(overlayCancel, 228);
    lv_obj_set_y(overlayConfirm, 228);
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
        command.overlayKind == rtos::UiOverlayKind::ScaleResetConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::QuickWeightConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::AdvancedWeightConfirmation ||
        command.overlayKind == rtos::UiOverlayKind::TagDefinitionImport ||
        // Firmware-Update (TASKS.md Phase 13.3/13.8-Fehlerkorrektur): fehlte
        // hier, wodurch der Bestaetigen-Knopf fuer diesen Dialog unsichtbar
        // blieb (nur "Schliessen"/Cancel war zu sehen) -- der eigentliche
        // Installationsauftrag wurde dadurch nie ausgeloest.
        command.overlayKind == rtos::UiOverlayKind::UpdateInstallConfirmation;
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

/// @brief LVGL timer callback: deletes a touch-diagnostic marker after its display timeout.
/// @param timer LVGL timer whose user data is the marker widget to delete.
void deleteTouchMarker(lv_timer_t* timer) {
  auto* marker = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
  if (marker != nullptr) {
    lv_obj_delete(marker);
  }
}

/// @brief Shows a brief, color-cycling dot at a touch point, for touch-calibration diagnostics.
/// @param x Touch X coordinate.
/// @param y Touch Y coordinate.
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

/// @brief LVGL tick source callback.
/// @return Milliseconds since boot.
std::uint32_t tickMilliseconds() { return millis(); }

/// @brief LVGL display flush callback: pushes one dirty area to the physical display.
/// @param display Display being flushed.
/// @param area Dirty pixel area.
/// @param pixelMap RGB565 pixel data for `area`.
void flushDisplay(lv_display_t* display, const lv_area_t* area,
                  std::uint8_t* pixelMap) {
  const std::int32_t width = area->x2 - area->x1 + 1;
  const std::int32_t height = area->y2 - area->y1 + 1;
  drivers::displayDevice().pushImage(
      area->x1, area->y1, width, height,
      reinterpret_cast<const lgfx::rgb565_t*>(pixelMap));
  lv_display_flush_ready(display);
}

/// @brief LVGL touch input-device read callback.
/// @param data Out parameter receiving the current touch point/state.
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

/// @brief German display text for a connection state.
/// @param state State to describe.
/// @return Static, NUL-terminated German text.
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

/// @brief Recursively finds the first lv_label descendant of a widget.
/// @param object Widget to search.
/// @return The first label found, or nullptr.
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

/// @brief Finds a button's caption label.
/// @param button Button widget.
/// @return The button's label descendant, or nullptr.
lv_obj_t* buttonLabel(lv_obj_t* button) {
  return firstLabelDescendant(button);
}

/// @brief Sets a button's caption text.
/// @param button Button widget.
/// @param text Text to set.
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
/// @brief Sets a widget's text: its label descendant if it's a button, or itself if it's a plain label.
/// @param object Widget to update.
/// @param text Text to set.
void setControlText(lv_obj_t* object, const char* text) {
  if (object == nullptr) return;
  lv_obj_t* label = buttonLabel(object);
  lv_label_set_text(label == nullptr ? object : label, text);
}

// Standard-Luma-Gewichtung (Rec. 601), Schwelle 150000 entspricht ~59%
// Helligkeit -- ab da gilt ein Hintergrund als hell genug fuer dunklen Text.
/// @brief Whether a background color is light enough to need dark text (Rec. 601 luma).
/// @param backgroundRgb Packed RGB888 background color.
/// @return true if the background is light.
bool isLightBackground(std::uint32_t backgroundRgb) {
  const std::uint32_t red = (backgroundRgb >> 16U) & 0xFFU;
  const std::uint32_t green = (backgroundRgb >> 8U) & 0xFFU;
  const std::uint32_t blue = backgroundRgb & 0xFFU;
  return (red * 299U + green * 587U + blue * 114U) > 150000U;
}

/// @brief Sets a button's background color and picks a matching light/dark caption color.
/// @param button Button widget.
/// @param backgroundRgb Background color to apply.
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

/// @brief LVGL click handler: opens the printer switcher from the header.
void headerClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::SelectPrinter, currentPrinterId, 0, 0, 1);
}

/// @brief LVGL click handler: opens the settings screen.
void settingsClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::OpenSettings, currentPrinterId);
}

/// @brief LVGL click handler: navigates back to the previous screen.
void backClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::Back, currentPrinterId);
}

/// @brief LVGL click handler: opens the staging detail screen.
void stagingClicked(lv_event_t*) {
  // Staging ist druckerunabhaengig -- siehe die gleiche Korrektur in
  // updateHomeContent(); stagingState.printerId wird nirgends gesetzt und
  // war dadurch immer 0, sodass hier immer spoolId=0 gesendet wurde statt
  // der tatsaechlich gestagten Spule.
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 0,
             stagingState.spoolId);
}

/// @brief LVGL click handler: selects an AMS unit on Home.
/// @param event LVGL click event; its user data encodes the AMS index.
void amsClicked(lv_event_t* event) {
  const auto amsId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectAms, currentPrinterId, amsId);
}

/// @brief LVGL click handler: selects a tray.
/// @param event LVGL click event; its user data encodes the tray index.
void trayClicked(lv_event_t* event) {
  const auto trayId = static_cast<std::uint8_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  // spoolId wurde frueher aus models::mock::findTray() befuellt (TASKS.md
  // Phase 12.3) -- AppTasks SelectTray-Handler liest action.spoolId
  // nachweislich nie (AppTask.cpp, SelectTray-Case), toter Code ohne
  // Verhaltensaenderung entfernt.
  const std::uint8_t amsId = trayId == 0xFF ? 0xFF : currentAmsId;
  sendAction(rtos::UiActionType::SelectTray, currentPrinterId, amsId, trayId);
}

/// @brief LVGL click handler: selects a printer from the switcher.
/// @param event LVGL click event; its user data encodes the printer id.
void printerClicked(lv_event_t* event) {
  const auto printerId = static_cast<rtos::PrinterId>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectPrinter, printerId);
}

/// @brief LVGL click handler: navigates to a settings category screen.
/// @param event LVGL click event; its user data encodes the target rtos::UiActionType.
void settingsCategoryClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, currentPrinterId);
}

/// @brief Maps a UI field index to its #spoolmanDraft text value.
/// @param field UI-defined field index (1-6).
/// @return The corresponding value, or "" if unrecognized.
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

/// @brief Mutable-pointer variant of spoolmanFieldValue(), for direct in-place editing.
/// @param field UI-defined field index (1-6).
/// @return Writable pointer to the corresponding buffer.
char* spoolmanFieldDestination(std::int32_t field) {
  return const_cast<char*>(spoolmanFieldValue(field));
}

/// @brief Buffer capacity for a #spoolmanDraft field, matching spoolmanFieldValue().
/// @param field UI-defined field index (1-6).
/// @return Buffer size in bytes, or 0 if unrecognized.
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

/// @brief Re-renders the Spoolman settings screen's labels from #spoolmanDraft.
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

/// @brief Closes and destroys the Spoolman field editor/keyboard widgets, if open.
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

/// @brief Commits whatever text currently sits in the open Spoolman field
///        editor into #spoolmanDraft, exactly as pressing "OK" on the
///        on-screen keyboard would.
// Test/Speichern/Abbrechen sit lower on the same screen (y=264)
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

/// @brief LVGL keyboard handler: applies one keypress to the open Spoolman/printer field editor.
/// @param event LVGL value-changed event from the on-screen keyboard.
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

/// @brief LVGL click handler: opens the text editor for one Spoolman
///        settings field (or directly toggles the protocol for field 2).
/// @param event LVGL click event; its user data encodes the field index.
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

/// @brief LVGL click handler: commits any open field edit, then sends the
///        Spoolman settings action (Test/Save/Cancel) bound to the button.
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
void spoolmanActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  // Commit a still-open field edit first (see commitSpoolmanEditorIfOpen())
  // so Testen/Speichern/Abbrechen always act on what's currently visible in
  // the editor, not a stale pre-edit value.
  commitSpoolmanEditorIfOpen();
  sendAction(type, currentPrinterId);
}

/// @brief Closes and destroys the calibration weight editor/keyboard widgets, if open.
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

/// @brief LVGL keyboard handler: applies one keypress to the calibration reference-weight editor.
/// @param event LVGL value-changed event from the on-screen keyboard.
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

/// @brief LVGL click handler: opens the reference-weight editor for scale calibration.
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

/// @brief Finds a printer's display-list entry by id.
/// @param id Printer id to find.
/// @return Pointer to the entry, or nullptr if not found.
PrinterUiEntry* printerEntry(rtos::PrinterId id) {
  for (auto& entry : printerEntries) {
    if (entry.id == id) return &entry;
  }
  return nullptr;
}

/// @brief Finds a tray's display-list entry by (amsId, trayId), including the external slot (0xFF, 0xFF).
/// @param amsId AMS index, or 0xFF for the external slot.
/// @param trayId Tray index, or 0xFF for the external slot.
/// @return Pointer to the entry, or nullptr if out of range.
TrayUiEntry* trayUiEntry(std::uint8_t amsId, std::uint8_t trayId) {
  if (amsId == 0xFF && trayId == 0xFF) return &externalTrayEntry;
  if (amsId < 1 || amsId > trayEntries.size() || trayId >= 4) return nullptr;
  return &trayEntries[amsId - 1][trayId];
}

/// @brief Parses the leading 6 hex digits of a Bambu tray_color (RRGGBB or
///        RRGGBBAA) into an 0xRRGGBB value.
/// @param colorHex Color string to parse.
/// @return The parsed value, or #kColorNeutralGrey for missing/malformed input.
std::uint32_t parseTrayColorHex(const char* colorHex) {
  if (colorHex == nullptr || std::strlen(colorHex) < 6) return kColorNeutralGrey;
  char buffer[7];
  std::snprintf(buffer, sizeof(buffer), "%.6s", colorHex);
  char* end = nullptr;
  const unsigned long value = std::strtoul(buffer, &end, 16);
  return (end != nullptr && *end == '\0') ? static_cast<std::uint32_t>(value)
                                          : kColorNeutralGrey;
}

/// @brief Maps a UI field index to its #printerUiDraft text value.
/// @param field UI-defined field index (1-4).
/// @return The corresponding value, or "" if unrecognized.
const char* printerDraftValue(std::int32_t field) {
  switch (field) {
    case 1: return printerUiDraft.name;
    case 2: return printerUiDraft.host;
    case 3: return printerUiDraft.serial;
    case 4: return printerUiDraft.accessCode;
    default: return "";
  }
}

/// @brief Mutable-pointer variant of printerDraftValue(), for direct in-place editing.
/// @param field UI-defined field index (1-4).
/// @return Writable pointer to the corresponding buffer.
char* printerDraftDestination(std::int32_t field) {
  return const_cast<char*>(printerDraftValue(field));
}

/// @brief Buffer capacity for a #printerUiDraft field, matching printerDraftValue().
/// @param field UI-defined field index (1-4).
/// @return Buffer size in bytes, or 0 if unrecognized.
std::size_t printerDraftCapacity(std::int32_t field) {
  switch (field) {
    case 1: return sizeof(printerUiDraft.name);
    case 2: return sizeof(printerUiDraft.host);
    case 3: return sizeof(printerUiDraft.serial);
    case 4: return sizeof(printerUiDraft.accessCode);
    default: return 0;
  }
}

/// @brief Resets #printerUiDraft to blank placeholders for a printer id,
///        pending the real values arriving via sendPrinterDraftToUi().
/// @param id Printer being edited.
void loadPrinterUiDraft(rtos::PrinterId id) {
  editingPrinterId = id;
  showPrinterAccessCode = false;
  // Nur ein neutraler Platzhalter, bis AppTasks sendPrinterDraftToUi() im
  // selben UI-Command-Burst direkt nach ShowScreen die echten Werte
  // nachliefert (Nutzerwunsch 2026-08-25) -- vorher standen hier erfundene
  // Beispieldrucker (Name/IP/Seriennummer/Access-Code) je nach ID.
  printerUiDraft.name[0] = '\0';
  printerUiDraft.host[0] = '\0';
  printerUiDraft.serial[0] = '\0';
  printerUiDraft.accessCode[0] = '\0';
}

/// @brief Re-renders the printer editor screen's labels from #printerUiDraft.
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

/// @brief Re-renders the printer management list's 4 rows from #printerEntries.
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

/// @brief LVGL click handler: selects a printer row in the management list.
/// @param event LVGL click event; its user data encodes the printer id.
void printerRowClicked(lv_event_t* event) {
  const auto id = static_cast<rtos::PrinterId>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectManagedPrinter, id);
}

/// @brief LVGL click handler: sends an add/edit/delete action for the managed printer list.
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
void printerListActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  const rtos::PrinterId id = type == rtos::UiActionType::AddPrinter ? 4 : managedPrinterId;
  sendAction(type, id);
}

/// @brief LVGL click handler: opens the text editor for one printer settings field.
/// @param event LVGL click event; its user data encodes the field index.
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

/// @brief LVGL click handler: toggles whether the LAN access code is shown in plain text.
void printerMaskClicked(lv_event_t*) {
  showPrinterAccessCode = !showPrinterAccessCode;
  updatePrinterEditorContent();
}

/// @brief LVGL click handler: sends the printer editor's action (Save/Test/Cancel/Delete).
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
void printerEditActionClicked(lv_event_t* event) {
  const auto type = static_cast<rtos::UiActionType>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(type, editingPrinterId);
}

/// @brief LVGL click handler: opens the printer management screen.
void managePrintersClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::OpenPrinterSettings, currentPrinterId);
}

/// @brief LVGL click handler: opens the full staging detail screen.
void stagingMoreClicked(lv_event_t*) {
  const auto& staging = stagingState;
  sendAction(rtos::UiActionType::SelectStaging, currentPrinterId, 0, 0, 1,
             staging.spoolId);
}

/// @brief LVGL click handler: sends a staging action (weigh/assign/clear), carrying the staged spool's identity.
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
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

/// @brief LVGL click handler: selects a tab on the tray detail screen.
/// @param event LVGL click event; its user data encodes the tab value.
void trayDetailsClicked(lv_event_t* event) {
  const auto value = static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event)));
  sendAction(rtos::UiActionType::SelectTray, currentPrinterId,
             selectedTrayAmsId, selectedTrayId, value, selectedTraySpoolId);
}

/// @brief LVGL click handler: sends a tray slot action (assign/reapply/reset/untag/refresh).
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
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

/// @brief LVGL click handler: selects and immediately commits a tray as the
///        target for the staged spool (no separate confirm step in this screen).
/// @param event LVGL click event; its user data encodes the tray index.
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

/// @brief LVGL click handler: sends a tag action (select spool/assign/remove) for the currently present tag.
/// @param event LVGL click event; its user data encodes the rtos::UiActionType.
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

/// @brief LVGL click handler: assigns the last-used tag spool to the currently present tag.
void lastTagSpoolClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::AssignTag, currentPrinterId, 0, 0, 1);
}

/// @brief LVGL click handler: loads the currently present tag's
///        already-resolved spool directly into staging (no tag-identity
///        write involved, see rtos::UiActionType::LoadTagSpoolToStaging).
void loadTagSpoolToStagingClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::LoadTagSpoolToStaging, currentPrinterId, 0,
             0, 0, currentTagSpoolId);
}

/// @brief LVGL click handler: opens the spool picker to assign a tag.
void assignTagWithPickerClicked(lv_event_t*) {
  sendAction(rtos::UiActionType::AssignTag, currentPrinterId);
}

/// @brief Recursively disables click/scroll on every descendant, so clicks pass through to the parent widget.
/// @param object Widget whose descendants to make touch-transparent.
void makeDescendantsTouchTransparent(lv_obj_t* object) {
  const std::uint32_t childCount = lv_obj_get_child_count(object);
  for (std::uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t* child = lv_obj_get_child(object, static_cast<std::int32_t>(index));
    lv_obj_remove_flag(child, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(child, LV_OBJ_FLAG_SCROLLABLE);
    makeDescendantsTouchTransparent(child);
  }
}

/// @brief Registers a click callback on a widget, making its descendants touch-transparent first.
/// @param object Widget to bind.
/// @param callback Click callback.
/// @param userData User data passed to `callback`.
void bindClick(lv_obj_t* object, lv_event_cb_t callback,
               std::uintptr_t userData = 0) {
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  makeDescendantsTouchTransparent(object);
  lv_obj_add_event_cb(object, callback, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(userData));
}

/// @brief Applies the base clickable/centered/rounded styling shared by every EEZ label-button.
/// @param object Widget to style.
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

/// @brief Enables/disables a label-button's clickability and disabled visual state.
/// @param object Widget to update.
/// @param available Whether the button should be clickable/enabled.
void setLabelButtonAvailable(lv_obj_t* object, bool available) {
  if (object == nullptr) return;
  lv_obj_set_flag(object, LV_OBJ_FLAG_CLICKABLE, available);
  if (available) {
    lv_obj_remove_state(object, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(object, LV_STATE_DISABLED);
  }
}

/// @brief Enables/disables every Spoolman-dependent control based on #spoolmanAppState, and re-renders the status text.
/// @param command Optional command carrying an updated Spoolman state/version to apply first.
void applySpoolmanAppState(const rtos::UiCommand* command = nullptr) {
  const bool online =
      filament_station::models::spoolmanOperationsAvailable(spoolmanAppState);
  const bool tagReady =
      filament_station::models::spoolmanTagOperationsAvailable(
          spoolmanAppState);

  const std::array<lv_obj_t*, 5> onlineControls{{
      objects.staging_details_quick_weight,
      objects.staging_action_erase_tag,
      objects.tray_action_from_staging,
      objects.tray_action_manual,
      objects.tag_result_quick_weight,
  }};
  for (lv_obj_t* control : onlineControls)
    setLabelButtonAvailable(control, online);
  setLabelButtonAvailable(objects.tag_result_advanced_weight, online);

  // staging_action_configure/advanced_weight/clear operate on the spool
  // currently loaded into staging (Nutzerwunsch 2026-08-30) -- disabled
  // whenever staging is empty, not just when Spoolman is offline.
  const bool stagingHasSpool = stagingState.spoolId != 0;
  setLabelButtonAvailable(objects.staging_action_configure,
                          online && stagingHasSpool);
  setLabelButtonAvailable(objects.staging_action_advanced_weight,
                          online && stagingHasSpool);
  setLabelButtonAvailable(objects.staging_action_clear,
                          online && stagingHasSpool);

  // staging_action_link_tag/unlink_tag act on the physically present NFC
  // tag, not on whatever screen last reported tag capabilities (Nutzerwunsch
  // 2026-08-30) -- currentTagCanAssign/Remove are shared with
  // TagActionSelect/TagLegacy/TagUnknown and go stale for this screen once
  // the tag is removed/replaced without a fresh StagingActions navigation.
  // nfcTagPresent is refreshed on every physical detect/remove independent
  // of navigation (see its declaration), so it's the reliable source here.
  setLabelButtonAvailable(objects.staging_action_link_tag,
                          tagReady && nfcTagPresent);
  setLabelButtonAvailable(objects.staging_action_unlink_tag,
                          tagReady && nfcTagPresent);
  setLabelButtonAvailable(objects.tag_action_select_spool,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.tag_action_use_last_spool,
                          tagReady && currentTagCanAssign);
  setLabelButtonAvailable(objects.tag_action_link_staging,
                          tagReady && currentTagCanAssign &&
                              stagingState.spoolId != 0);
  setLabelButtonAvailable(objects.tag_action_load_to_staging,
                          online && currentTagSpoolId != 0);
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

/// @brief Colors a button's two secondary color-swatch containers for a
///        multi-color filament (the first color already colors the button
///        itself at the call site).
/// @param swatch1 Second color's swatch widget.
/// @param swatch2 Third color's swatch widget.
/// @param colors Full color array; index 0 colors the button itself, not used here.
/// @param colorCount Number of valid entries in `colors`.
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

/// @brief Creates the staging detail screen's striped table rows (one time), styling the title bar.
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

/// @brief Styles the tray detail screen's content panel, reusing the EEZ-owned label to conserve LVGL heap.
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

/// @brief Applies the German-character-set font to every generated screen.
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

/// @brief One-time setup of the whole generated UI: binds every click
///        handler, applies dynamic layout/positioning EEZ Studio can't
///        express, and creates the shared overlay/decoration widgets.
///        Called once from initializeLvgl().
void bindGeneratedWidgets() {
  // EEZ identifiers remain stable; only the user-facing wording is localized.
  // Ausnahme (Nutzerwunsch 2026-08-25): auf SCR_SETTINGS_HOME,
  // SCR_STAGING_DETAILS, SCR_STAGING_ACTIONS, SCR_TRAY_ACTIONS,
  // SCR_TAG_ACTION_SELECT, SCR_TAG_LEGACY, SCR_TAG_DEFINITION_IMPORT und
  // SCR_TAG_RESULT bleibt die Beschriftung exakt wie in EEZ Studio gesetzt
  // (Ausnahme: Header-Button) -- keine setControlText()-Ueberschreibung
  // mehr fuer diese Screens, siehe TASKS.md "Allgemeine Regeln".
  lv_obj_set_pos(objects.staging_action_link_tag, 4, 100);
  lv_obj_set_size(objects.staging_action_link_tag, 232, 52);
  lv_obj_set_pos(objects.staging_action_unlink_tag, 244, 100);
  lv_obj_set_size(objects.staging_action_unlink_tag, 232, 52);
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
  // staging_action_write_tag bleibt eine echte Laufzeit-Statusanzeige (nicht
  // klickbar, siehe oben) -- sie wird ab handleUiAction()/processUiCommand()
  // dynamisch mit dem Tag-Schreibstatus befuellt (assignmentStatus), das
  // faellt nicht unter die Button-Beschriftungsregel.
  lv_obj_remove_flag(objects.staging_action_erase_tag, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(objects.staging_action_erase_tag, 4, 212);
  lv_obj_set_size(objects.staging_action_erase_tag, 472, 48);
  setControlText(objects.tray_select_external, "Extern");
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
      objects.obj32__cmp_settings_button_content,
      objects.obj34__cmp_settings_button_content,
      objects.obj36__cmp_settings_button_content,
      objects.obj38__cmp_settings_button_content,
      objects.obj42__cmp_settings_button_content,
      objects.obj44__cmp_settings_button_content,
  }};
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
  lv_obj_add_flag(objects.tag_legacy_migrate, LV_OBJ_FLAG_HIDDEN);
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

  bindClick(objects.obj31__home_header_1, headerClicked);
  bindClick(objects.obj33__home_header_1, headerClicked);
  bindClick(objects.obj35__home_header_1, headerClicked);
  bindClick(objects.obj37__home_header_1, headerClicked);
  for (lv_obj_t* control : tagSettings) bindClick(control, settingsClicked);
  bindClick(objects.tag_action_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_action_use_last_spool, lastTagSpoolClicked);
  bindClick(objects.tag_action_link_staging, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::AssignTag));
  bindClick(objects.tag_action_load_to_staging, loadTagSpoolToStagingClicked);
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
  bindClick(objects.obj39__home_header_1, headerClicked);
  bindClick(objects.obj40__cmp_settings_button_content, settingsClicked);
  bindClick(objects.tag_definition_import_select_spool,
            assignTagWithPickerClicked);
  bindClick(objects.tag_definition_import_spoolman, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_definition_import_cancel, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));
  bindClick(objects.obj41__home_header_1, headerClicked);
  bindClick(objects.tag_legacy_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_legacy_import, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ImportTagDefinition));
  bindClick(objects.tag_legacy_erase, tagActionClicked,
            static_cast<std::uintptr_t>(
                rtos::UiActionType::RemoveTagAssignment));
  bindClick(objects.tag_legacy_close, backClicked);
  bindClick(objects.obj43__home_header_1, headerClicked);
  bindClick(objects.tag_unknown_select_spool, assignTagWithPickerClicked);
  bindClick(objects.tag_unknown_close, backClicked);
  bindClick(objects.bambu_spool_type_back, tagActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::Cancel));

  bindClick(objects.obj0__home_header_1, headerClicked);
  // home_bottom_printers wurde im EEZ-Projekt entfernt (Nutzerwunsch:
  // dieselbe Funktion/Druckerauswahl ist bereits ueber die Titelleiste
  // home_header erreichbar) -- kein Code-seitiges Ausblenden mehr noetig.
  bindClick(objects.obj2__home_header_1, headerClicked);
  bindClick(objects.obj4__home_header_1, headerClicked);

  bindClick(objects.obj1__cmp_settings_button_content, settingsClicked);
  bindClick(objects.obj3__cmp_settings_button_content, settingsClicked);
  bindClick(objects.select_back, backClicked);
  bindClick(objects.select_bottom_status, managePrintersClicked);
  bindClick(objects.settings_back, backClicked);

  bindClick(objects.obj5__home_header_1, headerClicked);
  bindClick(objects.obj7__home_header_1, headerClicked);
  bindClick(objects.obj6__cmp_settings_button_content, settingsClicked);
  bindClick(objects.obj8__cmp_settings_button_content, settingsClicked);
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

  bindClick(objects.obj9__home_header_1, headerClicked);
  bindClick(objects.obj11__home_header_1, headerClicked);
  bindClick(objects.obj13__home_header_1, headerClicked);
  bindClick(objects.obj10__cmp_settings_button_content, settingsClicked);
  bindClick(objects.obj12__cmp_settings_button_content, settingsClicked);
  bindClick(objects.obj14__cmp_settings_button_content, settingsClicked);
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

  bindClick(objects.ams1__ams, amsClicked, 1);
  bindClick(objects.ams2__ams, amsClicked, 2);
  bindClick(objects.ams3__ams, amsClicked, 3);
  bindClick(objects.ams4__ams, amsClicked, 4);
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

  bindClick(objects.obj15__home_header_1, headerClicked);
  bindClick(objects.obj16__cmp_settings_button_content, settingsClicked);
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

  bindClick(objects.obj17__home_header_1, headerClicked);
  bindClick(objects.obj18__cmp_settings_button_content, settingsClicked);
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

  bindClick(objects.obj19__home_header_1, headerClicked);
  bindClick(objects.obj20__cmp_settings_button_content, settingsClicked);
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

  bindClick(objects.obj21__home_header_1, headerClicked);
  bindClick(objects.obj22__cmp_settings_button_content, settingsClicked);
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
  bindClick(objects.obj23__home_header_1, headerClicked);
  bindClick(objects.obj24__cmp_settings_button_content, settingsClicked);
  bindClick(objects.scale_settings_tare, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::TareScale));
  bindClick(objects.scale_settings_calibrate, calibrationClicked);
  bindClick(objects.scale_settings_reset, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::ResetScaleCalibration));
  bindClick(objects.scale_settings_back, backClicked);
  bindClick(objects.obj25__home_header_1, headerClicked);
  bindClick(objects.obj26__cmp_settings_button_content, settingsClicked);
  bindClick(objects.device_settings_restart, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::PrepareRestart));
  bindClick(objects.device_settings_back, backClicked);
  bindClick(objects.obj27__home_header_1, headerClicked);
  bindClick(objects.obj28__cmp_settings_button_content, settingsClicked);
  bindClick(objects.diagnostics_settings_refresh, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::RefreshDiagnostics));
  bindClick(objects.diagnostics_settings_back, backClicked);
  bindClick(objects.obj29__home_header_1, headerClicked);
  bindClick(objects.obj30__cmp_settings_button_content, settingsClicked);
  bindClick(objects.firmware_settings_check, spoolmanActionClicked,
            static_cast<std::uintptr_t>(rtos::UiActionType::CheckFirmwareUpdate));
  bindClick(objects.firmware_settings_back, backClicked);

  const std::array<lv_obj_t*, 12> additionalSettingsButtons{{
      objects.obj21__home_header_1,
      objects.wifi_settings_portal, objects.wifi_settings_reset,
      objects.obj23__home_header_1,
      objects.scale_settings_tare, objects.scale_settings_calibrate,
      objects.scale_settings_reset, objects.obj25__home_header_1,
      objects.device_settings_restart,
      objects.obj27__home_header_1,
      objects.diagnostics_settings_refresh, objects.obj29__home_header_1,
  }};
  for (lv_obj_t* button : additionalSettingsButtons) styleLabelButton(button);
  styleLabelButton(objects.firmware_settings_check);
  styleLabelButton(objects.wifi_settings_back);
  styleLabelButton(objects.scale_settings_back);
  styleLabelButton(objects.device_settings_back);
  styleLabelButton(objects.diagnostics_settings_back);
  styleLabelButton(objects.firmware_settings_back);

  const std::array<lv_obj_t*, 18> printerButtons{{
      objects.obj17__home_header_1,
      objects.printer_settings_row_1, objects.printer_settings_row_2,
      objects.printer_settings_row_3, objects.printer_settings_row_4,
      objects.printer_settings_active, objects.printer_settings_enabled,
      objects.printer_settings_default, objects.printer_settings_add,
      objects.printer_settings_edit, objects.obj19__home_header_1,
      objects.printer_edit_name,
      objects.printer_edit_host, objects.printer_edit_serial,
      objects.printer_edit_access_code, objects.printer_edit_mask,
      objects.printer_edit_test, objects.printer_edit_save,
  }};
  for (lv_obj_t* button : printerButtons) styleLabelButton(button);
  styleLabelButton(objects.printer_settings_back);
  styleLabelButton(objects.printer_edit_delete);
  styleLabelButton(objects.printer_edit_cancel);

  const std::array<lv_obj_t*, 9> spoolmanButtons{{
      objects.obj15__home_header_1,
      objects.spoolman_setting_name, objects.spoolman_setting_protocol,
      objects.spoolman_setting_host, objects.spoolman_setting_port,
      objects.spoolman_setting_base_path, objects.spoolman_setting_timeout,
      objects.spoolman_setting_test, objects.spoolman_setting_save,
  }};
  for (lv_obj_t* button : spoolmanButtons) {
    styleLabelButton(button);
  }
  styleLabelButton(objects.spoolman_setting_cancel);

  const std::array<lv_obj_t*, 9> stagingButtons{{
      objects.obj5__home_header_1,
      objects.staging_details_quick_weight,
      objects.staging_details_more,
      objects.staging_details_close,
      objects.obj7__home_header_1,
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
  const std::array<lv_obj_t*, 20> trayButtons{{
      objects.obj9__home_header_1,
      objects.tray_details_tab_slot, objects.tray_details_tab_spool,
      objects.tray_details_more, objects.tray_details_refresh,
      objects.obj11__home_header_1,
      objects.tray_action_from_staging, objects.tray_action_manual,
      objects.tray_action_reapply, objects.tray_action_refresh,
      objects.obj13__home_header_1,
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
  const std::array<lv_obj_t*, 8> tagButtons{{
      objects.tag_action_select_spool, objects.tag_action_use_last_spool,
      objects.tag_review_confirm,
      objects.tag_result_quick_weight, objects.tag_result_advanced_weight,
      objects.tag_action_back, objects.tag_review_back,
      objects.tag_result_close,
  }};
  for (lv_obj_t* button : tagButtons) styleLabelButton(button);
  styleLabelButton(objects.tag_action_link_staging);
  styleLabelButton(objects.tag_action_load_to_staging);
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
  vTaskDelay(pdMS_TO_TICKS(250));
  createStagingTableDecoration();
  vTaskDelay(pdMS_TO_TICKS(250));
  createTrayDetailsDecoration();
}

/// @brief Re-renders the printer switcher's entries from #printerEntries.
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

// Waehlt je nach Helligkeit von `backgroundColor` (erste Filamentfarbe bzw.
// Kartenhintergrund) zwischen der hellen ("Standart"/"Header", dunkler
// Text) und dunklen ("Standart_W"/"Header_W", heller Text) Style-Variante
// fuer ein Label (Nutzerwunsch 2026-08-24): dunkler Hintergrund -> _W,
// heller Hintergrund -> ohne _W. `headerStyle` waehlt zwischen
// LabelStandart(_W) (material/weight/k_factor) und LabelHeader(_W)
// (STAGING_LABEL). Beide Varianten immer zuerst entfernt, dann die
// passende hinzugefuegt, damit beim Umschalten kein alter Style haengen
// bleibt (LVGL haengt Styles sonst an, statt sie zu ersetzen).
/// @brief Colors a label's text for readability against its background, optionally applying header emphasis.
/// @param label Label widget to style.
/// @param lightBackground Whether the label sits on a light background.
/// @param headerStyle Whether to apply the header-emphasis variant.
void applyBackgroundAwareLabelStyle(lv_obj_t* label, bool lightBackground,
                                    bool headerStyle) {
  if (headerStyle) {
    remove_style_label_header(label);
    remove_style_label_header_w(label);
    if (lightBackground) {
      add_style_label_header(label);
    } else {
      add_style_label_header_w(label);
    }
  } else {
    remove_style_label_standart(label);
    remove_style_label_standart_w(label);
    if (lightBackground) {
      add_style_label_standart(label);
    } else {
      add_style_label_standart_w(label);
    }
  }
}

/// @brief Re-renders one tray card's material/weight/K-factor labels,
///        color swatches, spool id badge, and nozzle-active icon.
/// @param button The tray's clickable EEZ sub-widget (not the transparent card wrapper).
/// @param materialLabel Material text label.
/// @param weightLabel Remaining-weight text label.
/// @param kFactorLabel K-factor text label.
/// @param swatch1 Second color's swatch widget.
/// @param swatch2 Third color's swatch widget.
/// @param spoolIdContainer Spool id badge container, shown/hidden based on resolution.
/// @param spoolIdLabel Spool id badge text label.
/// @param nozzleIcon "Currently in nozzle" indicator icon.
/// @param printerId Owning printer (currently unused; real data is only ever synced for the focused printer).
/// @param amsId AMS index, or 0xFF for the external slot.
/// @param trayId Tray index, or 0xFF for the external slot.
/// @param title Unused; no per-slot title is shown (Nutzerwunsch 2026-08-22).
// Zielobjekte entsprechen den Sub-Widgets der CMP_TRAY_CARD-Komponente
// (ui-project, Nutzerwunsch 2026-08-23; auf drei eigene Labels
// material/weight/k_factor umgebaut, Nutzerwunsch 2026-08-24 -- vorher ein
// einzelnes mehrzeiliges "label"): "button" ist das eigentliche klickbare
// EEZ-Objekt (Sub-Widget "tray", nicht der transparente Kartenwrapper
// objects.home_tray_N selbst).
void updateTrayButton(lv_obj_t* button, lv_obj_t* materialLabel,
                      lv_obj_t* weightLabel, lv_obj_t* kFactorLabel,
                      lv_obj_t* swatch1, lv_obj_t* swatch2,
                      lv_obj_t* spoolIdContainer, lv_obj_t* spoolIdLabel,
                      lv_obj_t* nozzleIcon, rtos::PrinterId printerId,
                      std::uint8_t amsId, std::uint8_t trayId,
                      const char* title) {
  (void)printerId;  // Real tray data (see AppTask::syncAmsToUi) is only
                    // ever synced for the printer currently in focus.
  (void)title;  // Nutzerwunsch (2026-08-22): kein "Slot N"/"Extern"-Titel
                // mehr -- die feste Bildschirmposition zeigt weiterhin,
                // welcher Slot gemeint ist (analog zu den AMS-Buttons).
  const TrayUiEntry* tray = trayUiEntry(amsId, trayId);
  if (tray == nullptr || !tray->occupied) {
    const bool lightBackground = isLightBackground(kColorNeutralGrey);
    applyBackgroundAwareLabelStyle(materialLabel, lightBackground, false);
    applyBackgroundAwareLabelStyle(weightLabel, lightBackground, false);
    applyBackgroundAwareLabelStyle(kFactorLabel, lightBackground, false);
    setControlText(materialLabel, "leer");
    setControlText(weightLabel, "");
    setControlText(kFactorLabel, "");
    setButtonColors(button, kColorNeutralGrey);
    updateHomeColorSwatches(swatch1, swatch2, {}, 0);
    lv_obj_add_flag(spoolIdContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spoolIdLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Material kommt real vom Drucker (tray->material). tray->spoolId kommt
    // aus dem lokal persistierten Cache (models/TraySpoolCache.h), geprueft
    // gegen material/colorHex zum Zuordnungszeitpunkt -- 0 heisst "belegt,
    // aber keine (mehr) vertrauenswuerdige Spoolman-Zuordnung" und zeigt
    // ein "?" statt einer Zahl. Sobald die Spule identifiziert ist, laedt
    // AppTask Restgewicht/K-Faktor asynchron aus Spoolman nach
    // (Nutzerwunsch 2026-08-24) -- tray->detailsLoaded unterscheidet
    // "noch nicht geladen" von einem echten Restgewicht 0. K-Faktor kommt
    // aus dem projektspezifischen Spoolman-Filament-Extra-Feld
    // "flow_dynamics_k_factor" und wird nur gezeigt, wenn dort hinterlegt.
    const char* material =
        tray->material[0] != '\0' ? tray->material : "belegt";
    setControlText(materialLabel, material);
    if (tray->spoolId != 0 && tray->detailsLoaded) {
      char weightText[24];
      std::snprintf(weightText, sizeof(weightText), "%.0fg",
                    static_cast<double>(tray->remainingWeightGrams));
      setControlText(weightLabel, weightText);
      if (tray->kFactorValid) {
        char kFactorText[24];
        std::snprintf(kFactorText, sizeof(kFactorText), "K (%.3f)",
                      static_cast<double>(tray->kFactor));
        setControlText(kFactorLabel, kFactorText);
      } else {
        setControlText(kFactorLabel, "");
      }
    } else {
      setControlText(weightLabel, "");
      setControlText(kFactorLabel, "");
    }
    const std::array<std::uint32_t, models::kMaximumFilamentColors> colors{
        parseTrayColorHex(tray->colorHex)};
    const std::uint8_t colorCount = tray->colorHex[0] != '\0' ? 1U : 0U;
    const std::uint32_t backgroundColor =
        colorCount > 0 ? colors[0] : kColorNeutralGrey;
    setButtonColors(button, backgroundColor);
    updateHomeColorSwatches(swatch1, swatch2, colors, colorCount);
    const bool lightBackground = isLightBackground(backgroundColor);
    applyBackgroundAwareLabelStyle(materialLabel, lightBackground, false);
    applyBackgroundAwareLabelStyle(weightLabel, lightBackground, false);
    applyBackgroundAwareLabelStyle(kFactorLabel, lightBackground, false);

    lv_obj_remove_flag(spoolIdContainer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spoolIdLabel, LV_OBJ_FLAG_HIDDEN);
    char spoolIdText[12];
    if (tray->spoolId != 0) {
      std::snprintf(spoolIdText, sizeof(spoolIdText), "%lu",
                    static_cast<unsigned long>(tray->spoolId));
    } else {
      std::snprintf(spoolIdText, sizeof(spoolIdText), "?");
    }
    setControlText(spoolIdLabel, spoolIdText);

    // Duese nur beim laut Drucker tatsaechlich aktiven Fach zeigen
    // ("tray_now", Nutzerwunsch 2026-08-24) -- vorher eine Mockformel
    // (erstes belegtes Fach je AMS).
    if (tray->isActiveNozzle) {
      lv_obj_remove_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
      lv_image_set_src(nozzleIcon, lightBackground ? &img_3_d_printer_nozzle
                                                    : &img_3_d_printer_nozzle_w);
    } else {
      lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

/// @brief Re-renders the entire Home screen (printer header, AMS buttons, tray cards, staging card, status).
void updateHomeContent() {
  const PrinterUiEntry* printer = printerEntry(currentPrinterId);
  if (printer == nullptr || !printer->exists) {
    return;
  }

  const std::array<lv_obj_t*, 4> amsButtons{{
      objects.ams1__ams,
      objects.ams2__ams,
      objects.ams3__ams,
      objects.ams4__ams,
  }};
  // AMS-Buttons zeigen keinen Text mehr, nur einen farbigen Rand am aktuell
  // gewählten AMS (Nutzerwunsch). Die eigentliche Färbung je Slot passiert
  // über vier CMP_AMS_TRAY_OVERVIEW-Instanzen (ams1..ams4, je vier
  // Container tray1..tray4, per Slot-Farbe wenn belegt).
  const std::array<std::array<lv_obj_t*, 4>, 4> amsSlotContainers{{
      {{objects.ams1__tray1, objects.ams1__tray2, objects.ams1__tray3,
        objects.ams1__tray4}},
      {{objects.ams2__tray1, objects.ams2__tray2, objects.ams2__tray3,
        objects.ams2__tray4}},
      {{objects.ams3__tray1, objects.ams3__tray2, objects.ams3__tray3,
        objects.ams3__tray4}},
      {{objects.ams4__tray1, objects.ams4__tray2, objects.ams4__tray3,
        objects.ams4__tray4}},
  }};
  // Düsen-Icon je Slot, sichtbar wenn dieses Fach laut Drucker das aktuell
  // druckende ist (analog CMP_TRAY_CARD/updateTrayButton's nozzleIcon,
  // Nutzerwunsch 2026-08-30, Variante korrigiert 2026-08-30). Variante nach
  // Filamentfarbe (gleiches Kontrastprinzip wie CMP_TRAY_CARD): helles
  // Filament -> "3D Printer Nozzle" (ohne W), dunkles Filament ->
  // "3D Printer Nozzle W".
  const std::array<std::array<lv_obj_t*, 4>, 4> amsNozzleIcons{{
      {{objects.ams1__nozzle1, objects.ams1__nozzle2, objects.ams1__nozzle3,
        objects.ams1__nozzle4}},
      {{objects.ams2__nozzle1, objects.ams2__nozzle2, objects.ams2__nozzle3,
        objects.ams2__nozzle4}},
      {{objects.ams3__nozzle1, objects.ams3__nozzle2, objects.ams3__nozzle3,
        objects.ams3__nozzle4}},
      {{objects.ams4__nozzle1, objects.ams4__nozzle2, objects.ams4__nozzle3,
        objects.ams4__nozzle4}},
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
      lv_obj_t* nozzleIcon = amsNozzleIcons[amsId - 1U][slot];
      if (!available) {
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      const TrayUiEntry* tray = trayUiEntry(amsId, slot);
      const bool occupied = tray != nullptr && tray->occupied;
      const std::uint32_t color =
          occupied ? parseTrayColorHex(tray->colorHex) : kColorNeutralGrey;
      lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_color(container, lv_color_hex(color), LV_PART_MAIN);
      if (occupied && tray->isActiveNozzle) {
        lv_obj_remove_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
        lv_image_set_src(nozzleIcon, isLightBackground(color)
                                          ? &img_3_d_printer_nozzle
                                          : &img_3_d_printer_nozzle_w);
      } else {
        lv_obj_add_flag(nozzleIcon, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  updateTrayButton(objects.home_tray_1__tray, objects.home_tray_1__material,
                   objects.home_tray_1__weight, objects.home_tray_1__k_factor,
                   objects.home_tray_1__color_1, objects.home_tray_1__color_2,
                   objects.home_tray_1__spoolmanager_id_container,
                   objects.home_tray_1__spoolmanager_id,
                   objects.home_tray_1__nozzle_icon, currentPrinterId,
                   currentAmsId, 0, "Slot 1");
  updateTrayButton(objects.home_tray_2__tray, objects.home_tray_2__material,
                   objects.home_tray_2__weight, objects.home_tray_2__k_factor,
                   objects.home_tray_2__color_1, objects.home_tray_2__color_2,
                   objects.home_tray_2__spoolmanager_id_container,
                   objects.home_tray_2__spoolmanager_id,
                   objects.home_tray_2__nozzle_icon, currentPrinterId,
                   currentAmsId, 1, "Slot 2");
  updateTrayButton(objects.home_tray_3__tray, objects.home_tray_3__material,
                   objects.home_tray_3__weight, objects.home_tray_3__k_factor,
                   objects.home_tray_3__color_1, objects.home_tray_3__color_2,
                   objects.home_tray_3__spoolmanager_id_container,
                   objects.home_tray_3__spoolmanager_id,
                   objects.home_tray_3__nozzle_icon, currentPrinterId,
                   currentAmsId, 2, "Slot 3");
  updateTrayButton(objects.home_tray_4__tray, objects.home_tray_4__material,
                   objects.home_tray_4__weight, objects.home_tray_4__k_factor,
                   objects.home_tray_4__color_1, objects.home_tray_4__color_2,
                   objects.home_tray_4__spoolmanager_id_container,
                   objects.home_tray_4__spoolmanager_id,
                   objects.home_tray_4__nozzle_icon, currentPrinterId,
                   currentAmsId, 3, "Slot 4");
  updateTrayButton(objects.home_tray_external__tray,
                   objects.home_tray_external__material,
                   objects.home_tray_external__weight,
                   objects.home_tray_external__k_factor,
                   objects.home_tray_external__color_1,
                   objects.home_tray_external__color_2,
                   objects.home_tray_external__spoolmanager_id_container,
                   objects.home_tray_external__spoolmanager_id,
                   objects.home_tray_external__nozzle_icon, currentPrinterId,
                   0xFF, 0xFF, "Extern");

  const auto& staging = stagingState;
  // Staging ist druckerunabhaengig (die AMS-Zuordnung eines gestagten Spools
  // erfolgt separat ueber ConfigureSlotFromStaging mit explizitem
  // printerId) -- stagingState.printerId wird nirgends gesetzt und war
  // dadurch strukturell immer 0, waehrend currentPrinterId nach der
  // Druckerkonfiguration ungleich 0 ist. Der Vergleich war folglich immer
  // falsch und zeigte den Staging-Button auf Home immer als leer an, auch
  // wenn tatsaechlich eine Spule gestagt war.
  const std::uint32_t stagingBackgroundColor =
      staging.colorCount > 0 ? staging.colorRgb[0] : kColorNeutralGrey;
  const bool stagingLightBackground = isLightBackground(stagingBackgroundColor);
  applyBackgroundAwareLabelStyle(objects.staging__material,
                                 stagingLightBackground, false);
  applyBackgroundAwareLabelStyle(objects.staging__weight,
                                 stagingLightBackground, false);
  applyBackgroundAwareLabelStyle(objects.staging__k_factor,
                                 stagingLightBackground, false);
  if (staging.spoolId != 0) {
    // K-Faktor kommt jetzt echt aus Spoolman (UpdateStaging-Handler oben,
    // AppTask::sendStagingUpdate() via LoadFilament) statt wie zuvor aus
    // Mockdaten (Nutzer-Report 2026-08-24) -- staging.kFactorValid
    // unterscheidet "noch nicht geladen/nicht hinterlegt" von einem
    // tatsaechlichen K-Faktor 0, gleiches Muster wie updateTrayButton().
    // CMP_STAGING_CARD hat wie CMP_TRAY_CARD drei eigene Labels
    // material/weight/k_factor statt eines einzelnen mehrzeiligen "label"
    // (Nutzerumbau 2026-08-24).
    setControlText(objects.staging__material, staging.material);
    char weightText[24];
    std::snprintf(weightText, sizeof(weightText), "%.0fg",
                  static_cast<double>(staging.remainingWeightGrams));
    setControlText(objects.staging__weight, weightText);
    if (staging.kFactorValid) {
      char kFactorText[24];
      std::snprintf(kFactorText, sizeof(kFactorText), "K (%.3f)",
                    static_cast<double>(staging.kFactor));
      FS_LOGD(services::LogComponent::Ui,
              "Staging card k_factor label set text=\"%s\" obj=%p",
              kFactorText, static_cast<void*>(objects.staging__k_factor));
      setControlText(objects.staging__k_factor, kFactorText);
    } else {
      FS_LOGD(services::LogComponent::Ui,
              "Staging card k_factor label cleared (kFactorValid=false) "
              "obj=%p",
              static_cast<void*>(objects.staging__k_factor));
      setControlText(objects.staging__k_factor, "");
    }
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
    setControlText(objects.staging__material, "leer");
    setControlText(objects.staging__weight, "");
    setControlText(objects.staging__k_factor, "");
    setButtonColors(objects.staging__staging, kColorNeutralGrey);
    updateHomeColorSwatches(objects.staging__color_3, objects.staging__color_4,
                            {}, 0);
    lv_obj_add_flag(objects.staging__spoolmanager_id_container,
                    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.staging__spoolmanager_id, LV_OBJ_FLAG_HIDDEN);
  }
  applyBackgroundAwareLabelStyle(objects.staging__staging_label,
                                 stagingLightBackground, true);

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

/// @brief Re-renders the staging card/detail screen from #stagingState/#stagingSpoolState.
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
  // K-Faktor kommt aus Spoolman (AppTask::sendStagingUpdate() via
  // LoadFilament, Nutzer-Report 2026-08-24) -- kFactorValid unterscheidet
  // "noch nicht geladen/nicht hinterlegt" von einem tatsaechlichen K-Faktor
  // 0, siehe updateTrayButton() fuer dasselbe Muster bei den AMS-Faechern.
  if (staging.kFactorValid) {
    std::snprintf(rowText, sizeof(rowText), "K-Faktor: %.3f",
                  static_cast<double>(staging.kFactor));
  } else {
    std::snprintf(rowText, sizeof(rowText), "K-Faktor: nicht hinterlegt");
  }
  lv_label_set_text(stagingTableRows[7], rowText);
  std::snprintf(rowText, sizeof(rowText), "NFC: %s", currentNfcStatusText);
  lv_label_set_text(stagingTableRows[8], rowText);

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

/// @brief Re-renders the tray detail screen's table rows for the currently selected tray.
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

/// @brief Sets the printer-name header text on every screen that shows one.
/// @param text Header text to set.
void setAllHeaderTexts(const char* text) {
  // CMP_TOP_PRINTER_BAR (Nutzerwunsch 2026-08-30): one shared component
  // instantiated on all 23 screens instead of a per-screen header button --
  // printer_label is now a directly named widget, no more setControlText()
  // child-lookup needed. Array order matches the component's instantiation
  // order in screens.c (obj0 = Home ... obj22 = TagUnknown), verified by
  // matching each create_user_widget_cmp_top_printer_bar() call site
  // against its enclosing create_screen_scr_*() function.
  const std::array<lv_obj_t*, 23> printerLabels{{
      objects.obj0__printer_label,
      objects.obj2__printer_label,
      objects.obj4__printer_label,
      objects.obj5__printer_label,
      objects.obj7__printer_label,
      objects.obj9__printer_label,
      objects.obj11__printer_label,
      objects.obj13__printer_label,
      objects.obj15__printer_label,
      objects.obj17__printer_label,
      objects.obj19__printer_label,
      objects.obj21__printer_label,
      objects.obj23__printer_label,
      objects.obj25__printer_label,
      objects.obj27__printer_label,
      objects.obj29__printer_label,
      objects.obj31__printer_label,
      objects.obj33__printer_label,
      objects.obj35__printer_label,
      objects.obj37__printer_label,
      objects.obj39__printer_label,
      objects.obj41__printer_label,
      objects.obj43__printer_label,
  }};
  for (lv_obj_t* label : printerLabels) lv_label_set_text(label, text);
}

// Header-Statusicons (Nutzerwunsch, 2026-08-23): je Header ein Drucker-,
// WLAN- und Spoolman-Icon (scripts/add_header_status_icons.py legt genau
// ein Bild-Objekt je Status an, keine sichtbare/unsichtbare Variantenpaare)
// -- die Bildquelle wird hier zur Laufzeit auf die passende
// verbunden/getrennt-Grafik umgeschaltet, EEZ Studio besitzt weiterhin
// Position/Größe.
/// @brief Re-renders the header's WiFi/Spoolman/NFC status icons from #currentNetworkState/#spoolmanAppState/#currentNfcStatusText.
void updateHeaderStatusIcons() {
  // CMP_TOP_PRINTER_BAR (Nutzerwunsch 2026-08-30): see setAllHeaderTexts()'s
  // comment -- same shared component, same verified obj0..obj22 order.
  const std::array<lv_obj_t*, 23> printerIcons{{
      objects.obj0__printer, objects.obj2__printer, objects.obj4__printer,
      objects.obj5__printer, objects.obj7__printer, objects.obj9__printer,
      objects.obj11__printer, objects.obj13__printer, objects.obj15__printer,
      objects.obj17__printer, objects.obj19__printer, objects.obj21__printer,
      objects.obj23__printer, objects.obj25__printer, objects.obj27__printer,
      objects.obj29__printer, objects.obj31__printer, objects.obj33__printer,
      objects.obj35__printer, objects.obj37__printer, objects.obj39__printer,
      objects.obj41__printer, objects.obj43__printer,
  }};
  const std::array<lv_obj_t*, 23> wifiIcons{{
      objects.obj0__wifi, objects.obj2__wifi, objects.obj4__wifi,
      objects.obj5__wifi, objects.obj7__wifi, objects.obj9__wifi,
      objects.obj11__wifi, objects.obj13__wifi, objects.obj15__wifi,
      objects.obj17__wifi, objects.obj19__wifi, objects.obj21__wifi,
      objects.obj23__wifi, objects.obj25__wifi, objects.obj27__wifi,
      objects.obj29__wifi, objects.obj31__wifi, objects.obj33__wifi,
      objects.obj35__wifi, objects.obj37__wifi, objects.obj39__wifi,
      objects.obj41__wifi, objects.obj43__wifi,
  }};
  const std::array<lv_obj_t*, 23> spoolmanIcons{{
      objects.obj0__spoolman, objects.obj2__spoolman, objects.obj4__spoolman,
      objects.obj5__spoolman, objects.obj7__spoolman, objects.obj9__spoolman,
      objects.obj11__spoolman, objects.obj13__spoolman, objects.obj15__spoolman,
      objects.obj17__spoolman, objects.obj19__spoolman, objects.obj21__spoolman,
      objects.obj23__spoolman, objects.obj25__spoolman, objects.obj27__spoolman,
      objects.obj29__spoolman, objects.obj31__spoolman, objects.obj33__spoolman,
      objects.obj35__spoolman, objects.obj37__spoolman, objects.obj39__spoolman,
      objects.obj41__spoolman, objects.obj43__spoolman,
  }};
  // Neues "nfc"-Bild im Component (Nutzerwunsch 2026-08-30): sichtbar
  // genau dann, wenn aktuell ein Tag auf dem Reader liegt -- siehe
  // #nfcTagPresent's Kommentar fuer die AppTask-seitige Herkunft.
  const std::array<lv_obj_t*, 23> nfcIcons{{
      objects.obj0__nfc, objects.obj2__nfc, objects.obj4__nfc,
      objects.obj5__nfc, objects.obj7__nfc, objects.obj9__nfc,
      objects.obj11__nfc, objects.obj13__nfc, objects.obj15__nfc,
      objects.obj17__nfc, objects.obj19__nfc, objects.obj21__nfc,
      objects.obj23__nfc, objects.obj25__nfc, objects.obj27__nfc,
      objects.obj29__nfc, objects.obj31__nfc, objects.obj33__nfc,
      objects.obj35__nfc, objects.obj37__nfc, objects.obj39__nfc,
      objects.obj41__nfc, objects.obj43__nfc,
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
    lv_image_set_src(icon, wifiConnected ? &img_wifi_connected_w : &img_wifi_disconnected_g);
  for (lv_obj_t* icon : spoolmanIcons)
    lv_image_set_src(icon, spoolmanConnected ? &img_conneced_w
                                             : &img_disconneced_w);
  for (lv_obj_t* icon : nfcIcons)
    lv_obj_set_flag(icon, LV_OBJ_FLAG_HIDDEN, !nfcTagPresent);
}

/// @brief Updates the header for a printer id change: name text and status icons.
/// @param printerId Printer now shown in the header.
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

/// @brief Re-renders the AMS overview if the update concerns the currently focused printer.
/// @param printerId Printer the AMS update concerns.
/// @param amsId AMS unit updated (currently unused; the whole overview is re-rendered).
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

/// @brief Navigates to a screen, refreshing its content and closing any open editors/overlays first.
/// @param screenId Screen to show.
void showScreen(rtos::UiScreenId screenId) {
  switch (screenId) {
    case rtos::UiScreenId::Boot: {
      // Programmatisch statt nur EEZ-Studio-Statisch (wie
      // SettingsDevice/SettingsFirmware) -- vermeidet Drift, sobald ein
      // echtes Firmware-Update config::kApplicationVersion aendert
      // (Nutzerbericht 2026-08-30: release.ps1 bumpt AppConfig.h, aber der
      // Boot-Screen zeigte weiterhin die zuletzt in EEZ Studio gespeicherte
      // Version).
      char versionText[48];
      std::snprintf(versionText, sizeof(versionText), "Version %s",
                    config::kApplicationVersion);
      lv_label_set_text(objects.boot_version, versionText);
      loadScreen(SCREEN_ID_SCR_BOOT);
      break;
    }
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
      // Neutral, bis entweder ein echter Feld-Update (Nutzer tippt etwas,
      // markiert per command.amsId in UpdateSettings) oder gar nichts
      // passiert -- der stille Erstladevorgang (AppTask::
      // sendPrinterDraftToUi(), direkt nach diesem ShowScreen) setzt diesen
      // Status bewusst nicht mehr auf "ge\xC3\xA4ndert" (Nutzerwunsch 2026-08-25,
      // vorher hart codiert "Status: Mock-Daten").
      lv_label_set_text(objects.printer_edit_status, "Status: -");
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
    case rtos::UiScreenId::SettingsDevice: {
      // Programmatisch statt nur EEZ-Studio-Statisch (TASKS.md Phase 13.1)
      // -- vermeidet Drift, sobald ein echtes Firmware-Update
      // config::kApplicationVersion aendert.
      char versionText[48];
      std::snprintf(versionText, sizeof(versionText), "Version: %s",
                    config::kApplicationVersion);
      lv_label_set_text(objects.device_settings_version, versionText);
      loadScreen(SCREEN_ID_SCR_SETTINGS_DEVICE);
      break;
    }
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
    case rtos::UiScreenId::SettingsFirmware: {
      char installedText[48];
      std::snprintf(installedText, sizeof(installedText), "Installiert: %s",
                    config::kApplicationVersion);
      lv_label_set_text(objects.firmware_settings_current, installedText);
      loadScreen(SCREEN_ID_SCR_SETTINGS_FIRMWARE);
      break;
    }
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

/// @brief Frees the PSRAM-allocated LVGL draw buffers.
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
  // See the matching comment on the Boot case in showScreen() -- this is
  // the very first paint, before AppTask ever sends a ShowScreen(Boot)
  // command, so it needs the same dynamic update here too.
  {
    char versionText[48];
    std::snprintf(versionText, sizeof(versionText), "Version %s",
                  config::kApplicationVersion);
    lv_label_set_text(objects.boot_version, versionText);
  }
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

std::uint32_t inputInactiveMs() {
  return lv_display_get_inactive_time(lvglDisplay);
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
        setLabelButtonAvailable(objects.staging_action_link_tag,
                                nfcTagPresent);
        setLabelButtonAvailable(objects.staging_action_unlink_tag,
                                nfcTagPresent);
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
        currentTagSpoolId = command.spoolId;
        setLabelButtonAvailable(objects.tag_action_select_spool, canAssign);
        setLabelButtonAvailable(objects.tag_action_use_last_spool, canAssign);
        setLabelButtonAvailable(objects.tag_action_link_staging,
                                canAssign && stagingState.spoolId != 0);
        setLabelButtonAvailable(objects.tag_action_load_to_staging,
                                currentTagSpoolId != 0);
        setLabelButtonAvailable(objects.tag_action_erase, canRemove);
      } else if (command.screenId == rtos::UiScreenId::TagReview &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_review_summary, command.text);
      } else if (command.screenId == rtos::UiScreenId::TagResult &&
                 command.text[0] != '\0') {
        lv_label_set_text(objects.tag_result_message, command.text);
        const bool hideWeighActions =
            (command.value & rtos::UI_TAG_RESULT_NO_SPOOL_ACTIONS) != 0;
        lv_obj_set_flag(objects.tag_result_quick_weight, LV_OBJ_FLAG_HIDDEN,
                        hideWeighActions);
        lv_obj_set_flag(objects.tag_result_advanced_weight,
                        LV_OBJ_FLAG_HIDDEN, hideWeighActions);
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
    case rtos::UiCommandType::UpdateNfcPresence:
      nfcTagPresent = command.value != 0;
      updateHeaderStatusIcons();
      // staging_action_link_tag/unlink_tag must react immediately while the
      // user is already on StagingActions, not just on the next navigation
      // (Nutzerwunsch 2026-08-30).
      applySpoolmanAppState();
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
          entry->isActiveNozzle = ((command.value - 300) & 2) != 0;
          entry->spoolId = command.spoolId;
          std::snprintf(entry->material, sizeof(entry->material), "%s",
                        command.title);
          std::snprintf(entry->colorHex, sizeof(entry->colorHex), "%s",
                        command.text);
          // command.spool.id is only set (by AppTask::syncAmsToUi(), via
          // resolveTraySpoolDetails()) once the async Spoolman fetch for
          // this spool has actually completed -- command.spoolId != 0 alone
          // only means "identified", not "weight/K-factor loaded yet".
          // K-Faktor ist eine Spoolman-*Filament*-Eigenschaft
          // (Nutzerhinweis 2026-08-24), daher command.kFactor* statt
          // command.spool (das nur Spulen-Felder traegt).
          entry->detailsLoaded = command.spool.id != 0;
          entry->remainingWeightGrams = command.weightGrams;
          entry->kFactorValid = command.kFactorValid;
          entry->kFactor = command.kFactor;
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
        applySpoolmanAppState();
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
      stagingState.kFactorValid = command.kFactorValid;
      stagingState.kFactor = command.kFactor;
      FS_LOGD(services::LogComponent::Ui,
              "UpdateStaging received request_id=%lu spool_id=%lu "
              "kfactor_valid=%d kfactor=%.3f",
              static_cast<unsigned long>(command.requestId),
              static_cast<unsigned long>(command.spoolId),
              command.kFactorValid, static_cast<double>(command.kFactor));
      stagingSpoolState.spoolId = command.spoolId;
      stagingSpoolState.remainingWeightGrams = stagingState.remainingWeightGrams;
      if (hasReloadedSpool) {
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
      // Bug (Nutzer-Report 2026-08-24): diese beiden Defaults kamen bisher
      // *immer* aus command.weightUpdate, auch im hasReloadedSpool-Zweig --
      // dort ist command.weightUpdate nie gesetzt (0), wodurch ein per
      // Spoolman-Reload korrekt geladenes emptyWeightGrams/initialWeightGrams
      // hier direkt wieder auf 0 ueberschrieben wurde ("Leergewicht immer
      // 0g"). Jetzt quellenspezifisch vorbelegt; command.text bleibt im
      // hasReloadedSpool-Fall leer, sscanf() unten liefert dann parsed<3 und
      // ruehrt die Werte nicht an.
      float emptyWeightGrams = hasReloadedSpool
                                    ? command.spool.emptyWeightGrams
                                    : command.weightUpdate.emptySpoolWeightGrams;
      float initialWeightGrams = hasReloadedSpool
                                      ? command.spool.initialWeightGrams
                                      : command.weightUpdate.initialWeightGrams;
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
      // Bruttogewicht = Leergewicht + Restgewicht (Spulentara + verbleibendes
      // Filament) -- kein Live-Waagenwert, siehe Nutzer-Report 2026-08-24.
      stagingState.grossWeightGrams =
          stagingSpoolState.emptyWeightGrams + stagingState.remainingWeightGrams;
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
      applySpoolmanAppState();
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
          // amsId dient hier als Marker "echte Nutzeraenderung" (gesetzt in
          // AppTask::EditPrinterField) vs. "stiller Erstladevorgang" (0,
          // AppTask::sendPrinterDraftToUi() nach EditPrinter/AddPrinter) --
          // ohne diese Unterscheidung zeigte selbst ein frisch geoeffneter,
          // unveraenderter Drucker sofort "ge\xC3\xA4ndert" (Nutzerwunsch
          // 2026-08-25).
          if (command.amsId != 0) {
            lv_label_set_text(objects.printer_edit_status,
                              "Status: geändert, nicht gespeichert");
          }
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
      } else if (command.value == 400) {
        // Firmware-Update-Versions-Check (TASKS.md Phase 13.2): eigener
        // Wert, da diese "Verfuegbar: ..."-Zeile ein anderes Label ist als
        // die Statuszeile direkt oberhalb.
        lv_label_set_text(objects.firmware_settings_available, command.text);
      }
      if (command.value == 101 && command.title[0] != '\0') {
        char version[64];
        std::snprintf(version, sizeof(version), "Server: %s", command.title);
        lv_label_set_text(objects.spoolman_setting_version, version);
      } else if (command.value == 100) {
        lv_label_set_text(objects.spoolman_setting_version, "Server: -");
      }
      break;
    case rtos::UiCommandType::SetBrightness: {
      const std::int32_t clamped =
          command.value < 0 ? 0 : (command.value > 255 ? 255 : command.value);
      drivers::displayDevice().setBrightness(static_cast<std::uint8_t>(clamped));
      // Nutzerbericht 2026-08-28: nach einem Touch-Wake aus dem Sleep blieb
      // das Display nur ca. eine Sekunde hell und wurde dann sofort wieder
      // dunkel. Ursache: PowerTask::inactiveMs = 0 setzt nur die eigene,
      // lokale Kopie zurueck -- LVGL fuehrt seinen Inaktivitaets-Zeitstempel
      // (lv_display_get_inactive_time(), von UiTask jede
      // kPowerActivityReportIntervalMs als ReportInactivity gemeldet)
      // vollkommen unabhaengig davon selbst weiter und aktualisiert ihn nur,
      // wenn readTouch() den Touch tatsaechlich noch als gedrueckt liest --
      // war der Finger zum Zeitpunkt des naechsten LVGL-Polls schon wieder
      // angehoben (bei einem kurzen Tipp durchaus wahrscheinlich, siehe
      // TASKS.md), blieb der uralte, sehr hohe Wert stehen und die naechste
      // ReportInactivity-Meldung schickte PowerTask direkt zurueck in
      // Sleep.
      //
      // Nachtrag (2026-08-28, Nutzerbericht: Dimmed<->Active-Endlosschleife):
      // ein erster Versuch loeste dies fuer jede Helligkeit > 0 aus --
      // das feuerte auch beim Uebergang Active->Dimmed (Helligkeit 28), der
      // ja gerade *wegen* Inaktivitaet passiert. Das setzte den Zeitstempel
      // sofort wieder auf "jetzt", die naechste ReportInactivity zeigte
      // praktisch 0 ms und PowerTask sprang sofort zurueck nach Active --
      // alle 30s erneut. Nur ein Uebergang **zur vollen Aktiv-Helligkeit**
      // ist tatsaechlich immer die Folge bereits vorhandener echter
      // Aktivitaet (Touch-Wake aus Sleep, oder Dimmed->Active durch eine
      // schon frische ReportInactivity) -- ausschliesslich dieser Fall
      // darf den Zeitstempel zuruecksetzen.
      if (clamped == config::kDisplayDefaultBrightness) {
        lv_display_trigger_activity(lvglDisplay);
        FS_LOGD(services::LogComponent::Power,
                "Display activity timer reset brightness=%ld",
                static_cast<long>(clamped));
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace filament_station::ui
