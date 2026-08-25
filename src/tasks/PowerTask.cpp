#include "tasks/Tasks.h"

#include "config/BoardConfig.h"
#include "config/PowerConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"

namespace filament_station::tasks {
namespace {

// Statemachine aus dem Energiesparkonzept (TASKS.md Phase 11): AKTIV ->
// GEDIMMT -> LIGHT-SLEEP, jeweils nach laenger werdender Inaktivitaet. Die
// Peripherie-Abschaltung und der echte Light-Sleep folgen in den Phasen
// 11.3-11.6; hier steht der Zustandsuebergang plus die Backlight-Stufe.
enum class PowerState : std::uint8_t { Active, Dimmed, Sleep };

const char* powerStateName(PowerState state) {
  switch (state) {
    case PowerState::Active: return "active";
    case PowerState::Dimmed: return "dimmed";
    case PowerState::Sleep: return "sleep";
  }
  return "unknown";
}

PowerState stateForInactivity(std::uint32_t inactiveMs) {
  if (inactiveMs >= config::kPowerSleepTimeoutMs) return PowerState::Sleep;
  if (inactiveMs >= config::kPowerDimTimeoutMs) return PowerState::Dimmed;
  return PowerState::Active;
}

std::uint8_t brightnessForState(PowerState state) {
  switch (state) {
    case PowerState::Active: return config::kDisplayDefaultBrightness;
    case PowerState::Dimmed: return config::kPowerDimmedBrightness;
    case PowerState::Sleep: return 0;
  }
  return config::kDisplayDefaultBrightness;
}

void sendBrightness(rtos::RtosContext& ctx, std::uint8_t brightness) {
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::SetBrightness;
  command.value = brightness;
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(100)) != pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=ui_command op=set_brightness");
  }
}

void sendScalePower(rtos::RtosContext& ctx, rtos::ScaleCommandType type) {
  rtos::ScaleCommand command{};
  command.type = type;
  if (xQueueSend(ctx.scaleCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=scale_command op=power");
  }
}

void sendNfcPower(rtos::RtosContext& ctx, rtos::NfcCommandType type) {
  rtos::NfcCommand command{};
  command.type = type;
  if (xQueueSend(ctx.nfcCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=nfc_command op=power");
  }
}

void sendNetworkPower(rtos::RtosContext& ctx, rtos::NetworkCommandType type) {
  rtos::NetworkCommand command{};
  command.type = type;
  if (xQueueSend(ctx.networkCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=network_command op=power");
  }
}

}  // namespace

void powerTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);

  PowerState state = PowerState::Active;
  std::uint32_t inactiveMs = 0;
  rtos::PowerCommand command{};

  for (;;) {
    if (xQueueReceive(ctx.powerCommandQueue, &command,
                       pdMS_TO_TICKS(config::kPowerActivityReportIntervalMs)) ==
        pdTRUE) {
      switch (command.type) {
        case rtos::PowerCommandType::ReportInactivity:
          inactiveMs = command.inactiveMs;
          break;
        case rtos::PowerCommandType::PowerDownAcknowledged:
          // Das eigentliche Abwarten aller Bestaetigungen vor dem Light-
          // Sleep folgt erst mit Phase 11.6; hier nur sichtbar im Log.
          FS_LOGD(services::LogComponent::Power, "Power down acknowledged");
          break;
      }
    }

    const PowerState nextState = stateForInactivity(inactiveMs);
    if (nextState != state) {
      FS_LOGI(services::LogComponent::Power,
              "Power state changed from=%s to=%s inactive_ms=%lu",
              powerStateName(state), powerStateName(nextState),
              static_cast<unsigned long>(inactiveMs));
      const bool enteringSleep = nextState == PowerState::Sleep;
      const bool leavingSleep =
          state == PowerState::Sleep && nextState != PowerState::Sleep;
      state = nextState;
      sendBrightness(ctx, brightnessForState(state));
      if (enteringSleep) {
        sendScalePower(ctx, rtos::ScaleCommandType::PowerDown);
        sendNfcPower(ctx, rtos::NfcCommandType::PowerDown);
        sendNetworkPower(ctx, rtos::NetworkCommandType::PowerDown);
      } else if (leavingSleep) {
        sendScalePower(ctx, rtos::ScaleCommandType::PowerUp);
        sendNfcPower(ctx, rtos::NfcCommandType::PowerUp);
        sendNetworkPower(ctx, rtos::NetworkCommandType::PowerUp);
      }
    }
  }
}

}  // namespace filament_station::tasks
