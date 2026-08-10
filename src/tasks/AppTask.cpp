#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "config/ScaleConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {

rtos::UiScreenId currentScreen = rtos::UiScreenId::Boot;
rtos::UiScreenId previousScreen = rtos::UiScreenId::Home;
bool uiStartupReady = false;
bool storageStartupReady = false;
bool startupNavigationSent = false;
rtos::UiOverlayKind pendingOverlay = rtos::UiOverlayKind::None;
constexpr std::uint32_t kScaleLoadRequestId = 0x53430001U;
std::int32_t scaleCounts = 0;
std::int32_t scaleOffsetCounts = 0;
float scaleFactorCountsPerGram = 1.0F;
bool scaleCalibrated = false;
bool scaleStable = false;
bool scaleError = true;

float scaleWeightGrams() {
  if (!scaleCalibrated || scaleFactorCountsPerGram == 0.0F) return 0.0F;
  return static_cast<float>(scaleCounts - scaleOffsetCounts) /
         scaleFactorCountsPerGram;
}

struct SpoolmanDraft {
  char name[32] = "Werkstatt";
  char protocol[8] = "http";
  char host[64] = "spoolman.local";
  char port[8] = "7912";
  char basePath[32] = "/api/v1";
  char timeoutMs[8] = "5000";
};

SpoolmanDraft spoolmanDraft{};
struct PrinterDraft {
  rtos::PrinterId id = 1;
  char name[32] = "P1S Werkstatt";
  char host[64] = "192.168.1.50";
  char serial[32] = "01P123456789";
  char accessCode[16] = "12345678";
};
PrinterDraft printerDraft{};

void loadPrinterDraft(rtos::PrinterId id) {
  printerDraft.id = id;
  if (id == 2) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "X1C Labor");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.51");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "00M987654321");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "87654321");
  } else if (id == 3) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "A1 Mini Büro");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.52");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "030123456789");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "11223344");
  } else if (id == 4) {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "Neuer Drucker");
    printerDraft.host[0] = '\0';
    printerDraft.serial[0] = '\0';
    printerDraft.accessCode[0] = '\0';
  } else {
    std::snprintf(printerDraft.name, sizeof(printerDraft.name), "P1S Werkstatt");
    std::snprintf(printerDraft.host, sizeof(printerDraft.host), "192.168.1.50");
    std::snprintf(printerDraft.serial, sizeof(printerDraft.serial), "01P123456789");
    std::snprintf(printerDraft.accessCode, sizeof(printerDraft.accessCode), "12345678");
  }
}

char* printerField(std::int32_t field) {
  switch (field) {
    case 1: return printerDraft.name;
    case 2: return printerDraft.host;
    case 3: return printerDraft.serial;
    case 4: return printerDraft.accessCode;
    default: return nullptr;
  }
}

std::size_t printerFieldCapacity(std::int32_t field) {
  switch (field) {
    case 1: return sizeof(printerDraft.name);
    case 2: return sizeof(printerDraft.host);
    case 3: return sizeof(printerDraft.serial);
    case 4: return sizeof(printerDraft.accessCode);
    default: return 0;
  }
}

const char* validatePrinterDraft() {
  if (printerDraft.name[0] == '\0') return "Fehler: Anzeigename fehlt";
  if (printerDraft.host[0] == '\0' || std::strchr(printerDraft.host, ' ') != nullptr)
    return "Fehler: Host/IP ungültig";
  if (printerDraft.serial[0] == '\0') return "Fehler: Seriennummer fehlt";
  if (std::strlen(printerDraft.accessCode) != 8)
    return "Fehler: LAN-Code muss 8 Zeichen haben";
  return nullptr;
}

char* spoolmanField(std::int32_t field) {
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
      return nullptr;
  }
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

bool validNumber(const char* text, long minimum, long maximum) {
  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);
  return text[0] != '\0' && end != nullptr && *end == '\0' &&
         value >= minimum && value <= maximum;
}

const char* validateSpoolmanDraft() {
  if (spoolmanDraft.name[0] == '\0') {
    return "Fehler: Verbindungsname fehlt";
  }
  if (std::strcmp(spoolmanDraft.protocol, "http") != 0 &&
      std::strcmp(spoolmanDraft.protocol, "https") != 0) {
    return "Fehler: Protokoll ungültig";
  }
  if (spoolmanDraft.host[0] == '\0' ||
      std::strchr(spoolmanDraft.host, ' ') != nullptr) {
    return "Fehler: Host/IP ungültig";
  }
  if (!validNumber(spoolmanDraft.port, 1, 65535)) {
    return "Fehler: Port muss 1..65535 sein";
  }
  if (spoolmanDraft.basePath[0] != '/') {
    return "Fehler: Basispfad muss mit / beginnen";
  }
  if (!validNumber(spoolmanDraft.timeoutMs, 1000, 60000)) {
    return "Fehler: Timeout muss 1000..60000 ms sein";
  }
  return nullptr;
}

bool sendUiCommand(rtos::RtosContext& ctx, const rtos::UiCommand& command,
                   const char* failureMessage) {
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(1000)) == pdPASS) {
    return true;
  }
  rtos::logLine(failureMessage);
  return false;
}

bool sendScaleCommand(rtos::RtosContext& ctx,
                      const rtos::ScaleCommand& command) {
  if (xQueueSend(ctx.scaleCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    rtos::logLine("AppTask: scaleCommandQueue timeout/overflow");
    return false;
  }
  xTaskNotifyGive(ctx.scaleTask);
  return true;
}

void sendScaleUiState(rtos::RtosContext& ctx, std::uint32_t requestId = 0) {
  static TickType_t lastUpdateTick = 0;
  static std::int32_t lastFlags = -1;
  const std::int32_t flags = (scaleStable ? 1 : 0) |
                             (scaleCalibrated ? 2 : 0) |
                             (scaleError ? 4 : 0);
  const TickType_t now = xTaskGetTickCount();
  const TickType_t minimumInterval =
      pdMS_TO_TICKS(config::kScaleUiUpdateIntervalMs);
  const bool stateChanged = flags != lastFlags;
  if (requestId == 0 && !stateChanged &&
      static_cast<TickType_t>(now - lastUpdateTick) < minimumInterval) {
    return;
  }

  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::UpdateWeight;
  command.requestId = requestId;
  command.weightGrams = scaleWeightGrams();
  command.value = flags;
  std::snprintf(command.text, sizeof(command.text), "%s",
                scaleError ? "Waagenfehler"
                           : (!scaleCalibrated ? "nicht kalibriert"
                                               : (scaleStable ? "stabil" : "instabil")));
  if (sendUiCommand(ctx, command, "AppTask: weight UI queue overflow")) {
    lastUpdateTick = now;
    lastFlags = flags;
  }
}

bool requestScaleConfiguration(rtos::RtosContext& ctx) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::LoadJson;
  command.requestId = kScaleLoadRequestId;
  command.documentType = rtos::StorageDocumentType::Scale;
  std::snprintf(command.path, sizeof(command.path), "/config/scale.json");
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    rtos::logLine("AppTask: scale config load queue timeout/overflow");
    return false;
  }
  return true;
}

bool persistScaleConfiguration(rtos::RtosContext& ctx,
                               const rtos::AppEvent& event) {
  rtos::StorageCommand command{};
  command.type = rtos::StorageCommandType::SaveJson;
  command.requestId = event.requestId;
  command.documentType = rtos::StorageDocumentType::Scale;
  std::snprintf(command.path, sizeof(command.path), "/config/scale.json");
  const int length = std::snprintf(
      command.json, sizeof(command.json),
      "{\"schemaVersion\":1,\"updatedAt\":\"1970-01-01T00:00:00Z\","
      "\"documentType\":\"scale\",\"calibrated\":%s,"
      "\"tareOffsetCounts\":%ld,\"factorCountsPerGram\":%.9g}",
      event.scaleCalibrated ? "true" : "false",
      static_cast<long>(event.scaleOffsetCounts),
      static_cast<double>(event.scaleFactorCountsPerGram));
  if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(command.json)) {
    rtos::logLine("AppTask: scale configuration serialization failed");
    return false;
  }
  command.jsonLength = static_cast<std::uint16_t>(length);
  if (xQueueSend(ctx.storageCommandQueue, &command, pdMS_TO_TICKS(1000)) !=
      pdPASS) {
    rtos::logLine("AppTask: scale config save queue timeout/overflow");
    return false;
  }
  return true;
}

void sendOverlay(rtos::RtosContext& ctx, rtos::UiCommandType type,
                 rtos::UiOverlayKind kind, std::uint32_t requestId,
                 const char* title, const char* text) {
  rtos::UiCommand command{};
  command.type = type;
  command.overlayKind = kind;
  command.requestId = requestId;
  std::snprintf(command.title, sizeof(command.title), "%s", title);
  std::snprintf(command.text, sizeof(command.text), "%s", text);
  if (sendUiCommand(ctx, command, "AppTask: overlay queue overflow")) {
    pendingOverlay = kind;
  }
}

void showHomeWhenStartupReady(rtos::RtosContext& ctx) {
  if (startupNavigationSent || !uiStartupReady || !storageStartupReady) return;
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::HideProgress;
  sendUiCommand(ctx, command, "AppTask: boot overlay queue overflow");
  command.type = rtos::UiCommandType::ShowScreen;
  command.screenId = rtos::UiScreenId::Home;
  if (sendUiCommand(ctx, command, "AppTask: startup navigation queue overflow")) {
    startupNavigationSent = true;
    currentScreen = rtos::UiScreenId::Home;
    previousScreen = rtos::UiScreenId::Home;
  }
}

void handleUiAction(rtos::RtosContext& ctx, const rtos::UiAction& action) {
  rtos::UiCommand command{};
  command.requestId = action.requestId;
  command.printerId = action.printerId;
  command.spoolId = action.spoolId;
  command.amsId = action.amsId;
  command.trayId = action.trayId;
  command.value = action.value;

  switch (action.type) {
    case rtos::UiActionType::Cancel:
      command.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, command, "AppTask: hide overlay queue overflow");
      pendingOverlay = rtos::UiOverlayKind::None;
      return;

    case rtos::UiActionType::Confirm: {
      const auto confirmedOverlay = pendingOverlay;
      command.type = rtos::UiCommandType::HideProgress;
      sendUiCommand(ctx, command, "AppTask: hide confirmed overlay queue overflow");
      pendingOverlay = rtos::UiOverlayKind::None;
      const char* result = "Mock-Aktion best\xC3\xA4tigt; keine reale Funktion ausgef\xC3\xBChrt.";
      if (confirmedOverlay == rtos::UiOverlayKind::RestartConfirmation) {
        result = "Neustart best\xC3\xA4tigt; im Mock nicht ausgef\xC3\xBChrt.";
      } else if (confirmedOverlay == rtos::UiOverlayKind::WifiResetConfirmation) {
        result = "WLAN-Reset best\xC3\xA4tigt; Zugangsdaten bleiben im Mock erhalten.";
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                  rtos::UiOverlayKind::Success, action.requestId,
                  "Vorgang erfolgreich", result);
      return;
    }

    case rtos::UiActionType::SelectPrinter:
      if (action.value == 1) {
        previousScreen = currentScreen;
        currentScreen = rtos::UiScreenId::PrinterSelect;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = rtos::UiScreenId::PrinterSelect;
        sendUiCommand(ctx, command,
                      "AppTask: printer-select command queue overflow");
        return;
      }
      command.type = rtos::UiCommandType::UpdateHeader;
      if (!sendUiCommand(ctx, command,
                         "AppTask: header update command queue overflow")) {
        return;
      }
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = previousScreen;
      currentScreen = previousScreen;
      sendUiCommand(ctx, command,
                    "AppTask: printer return command queue overflow");
      return;

    case rtos::UiActionType::OpenSettings:
      if (currentScreen == rtos::UiScreenId::SettingsHome) {
        return;
      }
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::SettingsHome;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = rtos::UiScreenId::SettingsHome;
      sendUiCommand(ctx, command,
                    "AppTask: settings command queue overflow");
      return;

    case rtos::UiActionType::Back:
    case rtos::UiActionType::Close:
      command.type = rtos::UiCommandType::ShowScreen;
      if (currentScreen == rtos::UiScreenId::StagingActions) {
        command.screenId = rtos::UiScreenId::StagingDetails;
        currentScreen = rtos::UiScreenId::StagingDetails;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::StagingDetails) {
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = rtos::UiScreenId::Home;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TrayActions) {
        command.screenId = rtos::UiScreenId::TrayDetails;
        currentScreen = rtos::UiScreenId::TrayDetails;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TrayDetails) {
        command.screenId = rtos::UiScreenId::Home;
        currentScreen = rtos::UiScreenId::Home;
        previousScreen = rtos::UiScreenId::Home;
      } else if (currentScreen == rtos::UiScreenId::TraySelect) {
        command.screenId = rtos::UiScreenId::StagingActions;
        currentScreen = rtos::UiScreenId::StagingActions;
        previousScreen = rtos::UiScreenId::StagingDetails;
      } else if (currentScreen == rtos::UiScreenId::SettingsSpoolman) {
        command.screenId = rtos::UiScreenId::SettingsHome;
        currentScreen = rtos::UiScreenId::SettingsHome;
      } else if (currentScreen == rtos::UiScreenId::SettingsPrinterEdit) {
        command.screenId = rtos::UiScreenId::SettingsPrinters;
        currentScreen = rtos::UiScreenId::SettingsPrinters;
      } else if (currentScreen == rtos::UiScreenId::SettingsPrinters) {
        command.screenId = rtos::UiScreenId::SettingsHome;
        currentScreen = rtos::UiScreenId::SettingsHome;
      } else if (currentScreen == rtos::UiScreenId::SettingsWifi ||
                 currentScreen == rtos::UiScreenId::SettingsScale ||
                 currentScreen == rtos::UiScreenId::SettingsDevice ||
                 currentScreen == rtos::UiScreenId::SettingsDiagnostics ||
                 currentScreen == rtos::UiScreenId::SettingsFirmware) {
        command.screenId = rtos::UiScreenId::SettingsHome;
        currentScreen = rtos::UiScreenId::SettingsHome;
      } else {
        command.screenId = previousScreen;
        currentScreen = previousScreen;
      }
      sendUiCommand(ctx, command, "AppTask: back command queue overflow");
      return;

    case rtos::UiActionType::OpenWifiSettings:
    case rtos::UiActionType::OpenScaleSettings:
    case rtos::UiActionType::OpenDeviceSettings:
    case rtos::UiActionType::OpenDiagnostics:
    case rtos::UiActionType::OpenFirmwareSettings: {
      switch (action.type) {
        case rtos::UiActionType::OpenWifiSettings:
          currentScreen = rtos::UiScreenId::SettingsWifi;
          break;
        case rtos::UiActionType::OpenScaleSettings:
          currentScreen = rtos::UiScreenId::SettingsScale;
          break;
        case rtos::UiActionType::OpenDeviceSettings:
          currentScreen = rtos::UiScreenId::SettingsDevice;
          break;
        case rtos::UiActionType::OpenDiagnostics:
          currentScreen = rtos::UiScreenId::SettingsDiagnostics;
          break;
        case rtos::UiActionType::OpenFirmwareSettings:
          currentScreen = rtos::UiScreenId::SettingsFirmware;
          break;
        default:
          break;
      }
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command,
                    "AppTask: settings navigation queue overflow");
      return;
    }

    case rtos::UiActionType::StartWifiPortal:
    case rtos::UiActionType::ResetWifiCredentials:
    case rtos::UiActionType::PrepareRestart:
    case rtos::UiActionType::RefreshDiagnostics:
    case rtos::UiActionType::CheckFirmwareUpdate: {
      if (action.type == rtos::UiActionType::StartWifiPortal) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::ConnectionProgress,
                    action.requestId, "Verbindung wird vorbereitet",
                    "WLAN-Konfiguration wird gestartet (Mock)." );
        return;
      }
      if (action.type == rtos::UiActionType::ResetWifiCredentials) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::WifiResetConfirmation,
                    action.requestId, "WLAN zur\xC3\xBC" "cksetzen?",
                    "Gespeicherte WLAN-Zugangsdaten wirklich entfernen?");
        return;
      }
      if (action.type == rtos::UiActionType::PrepareRestart) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::RestartConfirmation,
                    action.requestId, "Ger\xC3\xA4t neu starten?",
                    "Der Neustart wird erst nach Best\xC3\xA4tigung ausgel\xC3\xB6st (Mock)." );
        return;
      }
      command.type = rtos::UiCommandType::ShowToast;
      command.value = 300 + static_cast<std::int32_t>(action.type);
      const char* text = "Mock-Aktion vorgemerkt";
      if (action.type == rtos::UiActionType::StartWifiPortal) text = "WLAN-Konfiguration vorgemerkt";
      else if (action.type == rtos::UiActionType::ResetWifiCredentials) text = "WLAN-Reset nicht ausgeführt (Mock)";
      else if (action.type == rtos::UiActionType::PrepareRestart) text = "Neustart nicht ausgeführt (Mock)";
      else if (action.type == rtos::UiActionType::RefreshDiagnostics) text = "Diagnose aktualisiert";
      else if (action.type == rtos::UiActionType::CheckFirmwareUpdate) text = "Update-Prüfung nicht ausgeführt (Mock)";
      std::snprintf(command.text, sizeof(command.text), "%s", text);
      sendUiCommand(ctx, command, "AppTask: settings mock action queue overflow");
      return;
    }

    case rtos::UiActionType::TareScale:
    case rtos::UiActionType::StartScaleCalibration:
    case rtos::UiActionType::ResetScaleCalibration: {
      rtos::ScaleCommand scaleCommand{};
      scaleCommand.requestId = action.requestId;
      if (action.type == rtos::UiActionType::TareScale) {
        scaleCommand.type = rtos::ScaleCommandType::Tare;
      } else if (action.type == rtos::UiActionType::StartScaleCalibration) {
        scaleCommand.type = rtos::ScaleCommandType::StartCalibration;
        scaleCommand.referenceWeightGrams = static_cast<float>(action.value);
      } else {
        scaleCommand.type = rtos::ScaleCommandType::ResetCalibration;
      }
      sendScaleCommand(ctx, scaleCommand);
      return;
    }

    case rtos::UiActionType::OpenPrinterSettings:
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer settings queue overflow");
      return;

    case rtos::UiActionType::AddPrinter:
    case rtos::UiActionType::EditPrinter:
      loadPrinterDraft(action.type == rtos::UiActionType::AddPrinter ? 4 : action.printerId);
      currentScreen = rtos::UiScreenId::SettingsPrinterEdit;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      command.printerId = printerDraft.id;
      sendUiCommand(ctx, command, "AppTask: printer editor queue overflow");
      return;

    case rtos::UiActionType::SetActivePrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 3;
      sendUiCommand(ctx, command, "AppTask: active printer queue overflow");
      return;
    case rtos::UiActionType::TogglePrinterEnabled:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 1;
      sendUiCommand(ctx, command, "AppTask: enabled printer queue overflow");
      return;
    case rtos::UiActionType::SetDefaultPrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 2;
      sendUiCommand(ctx, command, "AppTask: default printer queue overflow");
      return;
    case rtos::UiActionType::SelectManagedPrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 0;
      sendUiCommand(ctx, command, "AppTask: printer selection queue overflow");
      return;
    case rtos::UiActionType::DeletePrinter:
      command.type = rtos::UiCommandType::UpdatePrinterList;
      command.value = 4;
      sendUiCommand(ctx, command, "AppTask: delete printer queue overflow");
      currentScreen = rtos::UiScreenId::SettingsPrinters;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: printer return queue overflow");
      return;
    case rtos::UiActionType::EditPrinterField: {
      char* destination = printerField(action.value);
      const std::size_t capacity = printerFieldCapacity(action.value);
      if (destination == nullptr || capacity == 0) return;
      std::snprintf(destination, capacity, "%s", action.text);
      command.type = rtos::UiCommandType::UpdateSettings;
      command.value = 20 + action.value;
      std::snprintf(command.text, sizeof(command.text), "%s", destination);
      sendUiCommand(ctx, command, "AppTask: printer field queue overflow");
      return;
    }
    case rtos::UiActionType::TestPrinterConnection:
    case rtos::UiActionType::SavePrinterSettings: {
      const char* error = validatePrinterDraft();
      if (error != nullptr) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Eingabefehler", error);
      } else if (action.type == rtos::UiActionType::TestPrinterConnection) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::BambuConnection,
                    action.requestId, "Bambu-Verbindung",
                    "Verbindung zum gew\xC3\xA4hlten Drucker wird gepr\xC3\xBC" "ft (Mock)." );
      } else {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, action.requestId,
                    "Einstellungen g\xC3\xBCltig",
                    "Druckerkonfiguration wurde validiert (Mock)." );
      }
      return;
    }

    case rtos::UiActionType::OpenSpoolmanSettings:
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::SettingsSpoolman;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command,
                    "AppTask: Spoolman settings queue overflow");
      return;

    case rtos::UiActionType::EditSpoolmanSetting: {
      char* destination = spoolmanField(action.value);
      const std::size_t capacity = spoolmanFieldCapacity(action.value);
      if (destination == nullptr || capacity == 0) {
        return;
      }
      std::snprintf(destination, capacity, "%s", action.text);
      command.type = rtos::UiCommandType::UpdateSettings;
      std::snprintf(command.text, sizeof(command.text), "%s", destination);
      sendUiCommand(ctx, command,
                    "AppTask: Spoolman field update queue overflow");
      return;
    }

    case rtos::UiActionType::TestSpoolmanConnection:
    case rtos::UiActionType::SaveSpoolmanSettings: {
      const char* error = validateSpoolmanDraft();
      if (error != nullptr) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, action.requestId,
                    "Eingabefehler", error);
      } else if (action.type == rtos::UiActionType::TestSpoolmanConnection) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::SpoolmanRequest,
                    action.requestId, "Spoolman-Anfrage",
                    "Serverstatus wird abgefragt (Mock)." );
      } else {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Success, action.requestId,
                    "Einstellungen g\xC3\xBCltig",
                    "Spoolman-Konfiguration wurde validiert (Mock)." );
      }
      return;
    }

    case rtos::UiActionType::SelectAms:
      command.type = rtos::UiCommandType::UpdateAmsOverview;
      sendUiCommand(ctx, command,
                    "AppTask: AMS overview command queue overflow");
      return;

    case rtos::UiActionType::SelectTray:
      if (action.value == 0 || action.value == 1) {
        previousScreen = action.value == 1 ? rtos::UiScreenId::TrayDetails
                                            : currentScreen;
        currentScreen = action.value == 1 ? rtos::UiScreenId::TrayActions
                                           : rtos::UiScreenId::TrayDetails;
        command.type = rtos::UiCommandType::ShowScreen;
        command.screenId = currentScreen;
      } else {
        command.type = rtos::UiCommandType::UpdateTrayDetails;
      }
      sendUiCommand(ctx, command, "AppTask: tray command queue overflow");
      return;

    case rtos::UiActionType::ConfigureSlotFromStaging:
    case rtos::UiActionType::ConfigureSlot:
      previousScreen = currentScreen;
      currentScreen = rtos::UiScreenId::TraySelect;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command, "AppTask: tray-select queue overflow");
      return;

    case rtos::UiActionType::ResetSlot:
    case rtos::UiActionType::UntagSlot:
    case rtos::UiActionType::ReapplySlot:
    case rtos::UiActionType::RefreshSlot:
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text),
                    "Slot-Aktion vorgemerkt (%u/%u)", action.amsId,
                    action.trayId);
      sendUiCommand(ctx, command, "AppTask: slot action queue overflow");
      return;

    case rtos::UiActionType::SelectStaging:
      previousScreen = action.value == 1 ? rtos::UiScreenId::StagingDetails
                                         : currentScreen;
      currentScreen = action.value == 1 ? rtos::UiScreenId::StagingActions
                                        : rtos::UiScreenId::StagingDetails;
      command.type = rtos::UiCommandType::ShowScreen;
      command.screenId = currentScreen;
      sendUiCommand(ctx, command,
                    "AppTask: staging screen command queue overflow");
      return;

    case rtos::UiActionType::QuickWeight:
    case rtos::UiActionType::AdvancedWeight:
    case rtos::UiActionType::ClearStaging:
    case rtos::UiActionType::WriteTag:
    case rtos::UiActionType::LinkTag:
    case rtos::UiActionType::UnlinkTag:
    case rtos::UiActionType::EraseTag:
    case rtos::UiActionType::SearchSpool:
    case rtos::UiActionType::SelectSpool:
      if (action.type == rtos::UiActionType::QuickWeight ||
          action.type == rtos::UiActionType::AdvancedWeight) {
        if (scaleError) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Waage nicht bereit",
                      "Der HX711 liefert derzeit keine Messwerte.");
        } else if (!scaleCalibrated) {
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Error, action.requestId,
                      "Kalibrierung erforderlich",
                      "Die Waage muss vor dem Wiegen kalibriert werden.");
        } else if (!scaleStable) {
          sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                      rtos::UiOverlayKind::WeightStabilizing,
                      action.requestId, "Gewicht stabilisieren",
                      "Der reale Messwert ist noch instabil.");
        } else {
          char measurement[96];
          std::snprintf(measurement, sizeof(measurement),
                        "%s: %.1f g\nMesswert stabil.",
                        action.type == rtos::UiActionType::QuickWeight
                            ? "Quick Weight"
                            : "Advanced Weight",
                        static_cast<double>(scaleWeightGrams()));
          sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                      rtos::UiOverlayKind::Success, action.requestId,
                      "Waagenmessung", measurement);
        }
        return;
      }
      if (action.type == rtos::UiActionType::LinkTag) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::NfcRead, action.requestId,
                    "NFC-Tag lesen", "Tag bitte am Leser belassen (Mock)." );
        return;
      }
      if (action.type == rtos::UiActionType::WriteTag) {
        sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                    rtos::UiOverlayKind::NfcWrite, action.requestId,
                    "NFC-Tag schreiben",
                    "Tag wird geschrieben und anschlie\xC3\x9F" "end verifiziert (Mock)." );
        return;
      }
      if (action.type == rtos::UiActionType::ClearStaging ||
          action.type == rtos::UiActionType::EraseTag ||
          action.type == rtos::UiActionType::UnlinkTag) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Confirmation, action.requestId,
                    "Aktion best\xC3\xA4tigen",
                    "Diese Mock-Aktion kann eine Zuordnung entfernen. Fortfahren?");
        return;
      }
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text),
                    "Staging-Aktion vorgemerkt (Spule %lu)",
                    static_cast<unsigned long>(action.spoolId));
      sendUiCommand(ctx, command,
                    "AppTask: staging action queue overflow");
      return;

    default:
      rtos::logLine("AppTask: unhandled UiAction");
      return;
  }
}

}  // namespace

void appTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);
  rtos::AppEvent event{};
  for (;;) {
    if (xQueueReceive(ctx.appEventQueue, &event, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    if (event.type == rtos::AppEventType::UiAction) {
      handleUiAction(ctx, event.uiAction);
    } else if (event.type == rtos::AppEventType::ScaleMeasurement) {
      scaleCounts = event.value;
      scaleError = false;
      sendScaleUiState(ctx, event.requestId);
    } else if (event.type == rtos::AppEventType::ScaleStable ||
               event.type == rtos::AppEventType::ScaleUnstable) {
      scaleCounts = event.value;
      scaleStable = event.type == rtos::AppEventType::ScaleStable;
      scaleError = false;
      sendScaleUiState(ctx, event.requestId);
    } else if (event.type == rtos::AppEventType::ScaleTared ||
               event.type == rtos::AppEventType::ScaleCalibrated ||
               event.type == rtos::AppEventType::ScaleCalibrationReset) {
      scaleOffsetCounts = event.scaleOffsetCounts;
      scaleFactorCountsPerGram = event.scaleFactorCountsPerGram;
      scaleCalibrated = event.scaleCalibrated;
      scaleStable = false;
      scaleError = false;
      persistScaleConfiguration(ctx, event);
      sendScaleUiState(ctx, event.requestId);
      rtos::UiCommand result{};
      result.type = rtos::UiCommandType::ShowToast;
      result.requestId = event.requestId;
      result.value = 300 + static_cast<std::int32_t>(
          event.type == rtos::AppEventType::ScaleTared
              ? rtos::UiActionType::TareScale
              : (event.type == rtos::AppEventType::ScaleCalibrated
                     ? rtos::UiActionType::StartScaleCalibration
                     : rtos::UiActionType::ResetScaleCalibration));
      std::snprintf(result.text, sizeof(result.text), "%s",
                    event.type == rtos::AppEventType::ScaleTared
                        ? "Waage tariert"
                        : (event.type == rtos::AppEventType::ScaleCalibrated
                               ? "Kalibrierung gespeichert"
                               : "Kalibrierung zur\xC3\xBC" "ckgesetzt"));
      sendUiCommand(ctx, result, "AppTask: scale result UI queue overflow");
    } else if (event.type == rtos::AppEventType::ScaleReady ||
               event.type == rtos::AppEventType::ScaleError) {
      if (event.type == rtos::AppEventType::ScaleReady) {
        scaleCounts = event.value;
        scaleError = false;
      } else {
        scaleError = true;
        scaleStable = false;
      }
      sendScaleUiState(ctx, event.requestId);
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "Scale");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status, "AppTask: scale status UI queue overflow");
      if (event.type == rtos::AppEventType::ScaleError &&
          event.requestId != 0) {
        sendOverlay(ctx, rtos::UiCommandType::ShowDialog,
                    rtos::UiOverlayKind::Error, event.requestId,
                    "Waagenaktion fehlgeschlagen", event.text);
      }
    } else if (event.type == rtos::AppEventType::UiCommunicationTest) {
      uiStartupReady = true;
      rtos::UiCommand response{};
      response.type = rtos::UiCommandType::CommunicationTestResponse;
      response.requestId = event.requestId;
      std::snprintf(response.title, sizeof(response.title), "RTOS test");
      std::snprintf(response.text, sizeof(response.text), "AppTask acknowledged event");
      if (sendUiCommand(ctx, response,
                        "AppTask: uiCommandQueue timeout/overflow")) {
        rtos::logLine("AppTask: response sent");
      }
      sendOverlay(ctx, rtos::UiCommandType::ShowProgress,
                  rtos::UiOverlayKind::BootProgress, event.requestId,
                  "FilamentStation startet",
                  "Display bereit. SD-Karte und Konfiguration werden gepr\xC3\xBC" "ft." );
      showHomeWhenStartupReady(ctx);
    } else if (event.type == rtos::AppEventType::SdMounted ||
               event.type == rtos::AppEventType::SdRemoved ||
               event.type == rtos::AppEventType::SdReinserted ||
               event.type == rtos::AppEventType::SdError ||
               event.type == rtos::AppEventType::StorageReadCompleted ||
               event.type == rtos::AppEventType::StorageWriteCompleted ||
               event.type == rtos::AppEventType::StorageRequestError) {
      if (event.type == rtos::AppEventType::StorageReadCompleted &&
          event.requestId == kScaleLoadRequestId) {
        scaleOffsetCounts = event.scaleOffsetCounts;
        scaleFactorCountsPerGram = event.scaleFactorCountsPerGram;
        scaleCalibrated = event.scaleCalibrated;
        rtos::ScaleCommand scaleCommand{};
        scaleCommand.type = rtos::ScaleCommandType::ApplyCalibration;
        scaleCommand.requestId = event.requestId;
        scaleCommand.offsetCounts = event.scaleOffsetCounts;
        scaleCommand.factorCountsPerGram = event.scaleFactorCountsPerGram;
        scaleCommand.calibrated = event.scaleCalibrated;
        sendScaleCommand(ctx, scaleCommand);
        sendScaleUiState(ctx, event.requestId);
      }
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "Storage");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status,
                    "AppTask: storage status UI queue timeout/overflow");
      if (event.type == rtos::AppEventType::SdMounted) {
        storageStartupReady = true;
        requestScaleConfiguration(ctx);
        showHomeWhenStartupReady(ctx);
      }
    }
  }
}
}  // namespace filament_station::tasks
