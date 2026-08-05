#include "tasks/Tasks.h"

#include <cstdio>
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"

namespace filament_station::tasks {
namespace {

rtos::UiScreenId currentScreen = rtos::UiScreenId::Home;
rtos::UiScreenId previousScreen = rtos::UiScreenId::Home;

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
      } else {
        command.screenId = previousScreen;
        currentScreen = previousScreen;
      }
      sendUiCommand(ctx, command, "AppTask: back command queue overflow");
      return;

    case rtos::UiActionType::OpenWifiSettings:
    case rtos::UiActionType::OpenSpoolmanSettings:
    case rtos::UiActionType::OpenScaleSettings:
    case rtos::UiActionType::OpenPrinterSettings:
    case rtos::UiActionType::OpenDeviceSettings:
    case rtos::UiActionType::OpenDiagnostics:
    case rtos::UiActionType::OpenFirmwareSettings: {
      const char* section = "Einstellungen";
      switch (action.type) {
        case rtos::UiActionType::OpenWifiSettings:
          section = "WLAN";
          break;
        case rtos::UiActionType::OpenSpoolmanSettings:
          section = "Spoolman";
          break;
        case rtos::UiActionType::OpenScaleSettings:
          section = "Waage";
          break;
        case rtos::UiActionType::OpenPrinterSettings:
          section = "Bambu-Drucker";
          break;
        case rtos::UiActionType::OpenDeviceSettings:
          section = "Geraet";
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
      std::snprintf(command.text, sizeof(command.text), "%s ausgewaehlt",
                    section);
      sendUiCommand(ctx, command,
                    "AppTask: settings navigation queue overflow");
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
