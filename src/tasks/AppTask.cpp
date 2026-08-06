#include "tasks/Tasks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {

rtos::UiScreenId currentScreen = rtos::UiScreenId::Home;
rtos::UiScreenId previousScreen = rtos::UiScreenId::Home;

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

void handleUiAction(rtos::RtosContext& ctx, const rtos::UiAction& action) {
  rtos::UiCommand command{};
  command.requestId = action.requestId;
  command.printerId = action.printerId;
  command.spoolId = action.spoolId;
  command.amsId = action.amsId;
  command.trayId = action.trayId;
  command.value = action.value;

  switch (action.type) {
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
      const char* section = "Einstellungen";
      switch (action.type) {
        case rtos::UiActionType::OpenWifiSettings:
          section = "WLAN";
          break;
        case rtos::UiActionType::OpenScaleSettings:
          section = "Waage";
          break;
        case rtos::UiActionType::OpenDeviceSettings:
          section = "Gerät";
          break;
        case rtos::UiActionType::OpenDiagnostics:
          section = "Diagnose";
          break;
        case rtos::UiActionType::OpenFirmwareSettings:
          section = "Firmware";
          break;
        default:
          break;
      }
      command.type = rtos::UiCommandType::ShowToast;
      std::snprintf(command.text, sizeof(command.text), "%s ausgewählt",
                    section);
      sendUiCommand(ctx, command,
                    "AppTask: settings navigation queue overflow");
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
      command.type = rtos::UiCommandType::ShowToast;
      if (error != nullptr) {
        command.value = 200;
        std::snprintf(command.text, sizeof(command.text), "%s", error);
      } else if (action.type == rtos::UiActionType::TestPrinterConnection) {
        command.value = 201;
        std::snprintf(command.text, sizeof(command.text),
                      "Status: Mock-Verbindung erfolgreich");
      } else {
        command.value = 202;
        std::snprintf(command.text, sizeof(command.text),
                      "Status: Drucker validiert");
      }
      sendUiCommand(ctx, command, "AppTask: printer validation queue overflow");
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
      command.type = rtos::UiCommandType::ShowToast;
      if (error != nullptr) {
        command.value = 100;
        std::snprintf(command.text, sizeof(command.text), "%s", error);
      } else if (action.type == rtos::UiActionType::TestSpoolmanConnection) {
        command.value = 101;
        std::snprintf(command.title, sizeof(command.title), "Mock 0.22.1");
        std::snprintf(command.text, sizeof(command.text),
                      "Status: Mock-Verbindung erfolgreich");
      } else {
        command.value = 102;
        std::snprintf(command.text, sizeof(command.text),
                      "Status: Einstellungen validiert");
      }
      sendUiCommand(ctx, command,
                    "AppTask: Spoolman validation queue overflow");
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
    } else if (event.type == rtos::AppEventType::UiCommunicationTest) {
      rtos::UiCommand response{};
      response.type = rtos::UiCommandType::CommunicationTestResponse;
      response.requestId = event.requestId;
      std::snprintf(response.title, sizeof(response.title), "RTOS test");
      std::snprintf(response.text, sizeof(response.text), "AppTask acknowledged event");
      if (sendUiCommand(ctx, response,
                        "AppTask: uiCommandQueue timeout/overflow")) {
        rtos::logLine("AppTask: response sent");
      }
    } else if (event.type == rtos::AppEventType::SdMounted ||
               event.type == rtos::AppEventType::SdRemoved ||
               event.type == rtos::AppEventType::SdReinserted ||
               event.type == rtos::AppEventType::SdError ||
               event.type == rtos::AppEventType::StorageReadCompleted ||
               event.type == rtos::AppEventType::StorageWriteCompleted ||
               event.type == rtos::AppEventType::StorageRequestError) {
      rtos::UiCommand status{};
      status.type = rtos::UiCommandType::ShowStatus;
      status.requestId = event.requestId;
      std::snprintf(status.title, sizeof(status.title), "Storage");
      std::snprintf(status.text, sizeof(status.text), "%s", event.text);
      sendUiCommand(ctx, status,
                    "AppTask: storage status UI queue timeout/overflow");
    }
  }
}
}  // namespace filament_station::tasks
