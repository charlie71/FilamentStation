/**
 * @file
 * @brief Implements tasks::powerTask(): the Active/Dimmed/Sleep energy-saving
 *        state machine, peripheral power-down coordination, and the
 *        touch-wake light-sleep cycle.
 */
#include "tasks/Tasks.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cstdio>

#include "config/AppConfig.h"
#include "config/BoardConfig.h"
#include "config/PowerConfig.h"
#include "rtos/Messages.h"
#include "rtos/RtosContext.h"
#include "services/Logger.h"

namespace filament_station::tasks {
namespace {

/**
 * @brief Energy-saving state machine driven by measured input inactivity
 *        (TASKS.md Phase 11).
 *
 * @dot
 * digraph PowerState {
 *   rankdir=LR;
 *   Active -> Dimmed [label="inactive >= kPowerDimTimeoutMs"];
 *   Dimmed -> Sleep  [label="inactive >= kPowerSleepTimeoutMs"];
 *   Dimmed -> Active [label="inactive < kPowerDimTimeoutMs"];
 *   Sleep  -> Active [label="touch wake (GPIO)"];
 * }
 * @enddot
 */
enum class PowerState : std::uint8_t { Active, Dimmed, Sleep };

/// @brief Text name for a PowerState, used in log lines.
/// @param state State to describe.
/// @return Static, NUL-terminated lowercase name.
const char* powerStateName(PowerState state) {
  switch (state) {
    case PowerState::Active: return "active";
    case PowerState::Dimmed: return "dimmed";
    case PowerState::Sleep: return "sleep";
  }
  return "unknown";
}

/// @brief Maps a measured inactivity duration to its target PowerState.
/// @param inactiveMs Milliseconds since the last user input.
/// @return The state that inactivity duration corresponds to.
PowerState stateForInactivity(std::uint32_t inactiveMs) {
  if (inactiveMs >= config::kPowerSleepTimeoutMs) return PowerState::Sleep;
  if (inactiveMs >= config::kPowerDimTimeoutMs) return PowerState::Dimmed;
  return PowerState::Active;
}

/// @brief Target display brightness for a PowerState.
/// @param state State to look up.
/// @return Brightness value (0-255).
std::uint8_t brightnessForState(PowerState state) {
  switch (state) {
    case PowerState::Active: return config::kDisplayDefaultBrightness;
    case PowerState::Dimmed: return config::kPowerDimmedBrightness;
    case PowerState::Sleep: return 0;
  }
  return config::kDisplayDefaultBrightness;
}

/// @brief Sends a SetBrightness command to UiTask.
/// @param ctx Owning RTOS context.
/// @param brightness Target brightness (0-255).
void sendBrightness(rtos::RtosContext& ctx, std::uint8_t brightness) {
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::SetBrightness;
  command.value = brightness;
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(100)) != pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=ui_command op=set_brightness");
  }
}

/// @brief Sends a "waking up" toast notification to UiTask after resuming from sleep.
/// @param ctx Owning RTOS context.
void sendWakeToast(rtos::RtosContext& ctx) {
  rtos::UiCommand command{};
  command.type = rtos::UiCommandType::ShowToast;
  std::snprintf(command.text, sizeof(command.text), "%s",
                "Aufgewacht, verbinde...");
  if (xQueueSend(ctx.uiCommandQueue, &command, pdMS_TO_TICKS(100)) != pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=ui_command op=wake_toast");
  }
}

/// @brief Sends a power up/down command to ScaleTask.
/// @param ctx Owning RTOS context.
/// @param type PowerUp or PowerDown.
void sendScalePower(rtos::RtosContext& ctx, rtos::ScaleCommandType type) {
  rtos::ScaleCommand command{};
  command.type = type;
  if (xQueueSend(ctx.scaleCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=scale_command op=power");
  }
}

/// @brief Sends a power up/down command to NfcTask.
/// @param ctx Owning RTOS context.
/// @param type PowerUp or PowerDown.
void sendNfcPower(rtos::RtosContext& ctx, rtos::NfcCommandType type) {
  rtos::NfcCommand command{};
  command.type = type;
  if (xQueueSend(ctx.nfcCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=nfc_command op=power");
  }
}

/// @brief Sends a power up/down command to NetworkTask.
/// @param ctx Owning RTOS context.
/// @param type PowerUp or PowerDown.
void sendNetworkPower(rtos::RtosContext& ctx, rtos::NetworkCommandType type) {
  rtos::NetworkCommand command{};
  command.type = type;
  if (xQueueSend(ctx.networkCommandQueue, &command, pdMS_TO_TICKS(100)) !=
      pdPASS) {
    FS_LOGW(services::LogComponent::Power,
            "Event enqueue failed queue=network_command op=power");
  }
}

/// @brief Blocks until Scale-/Nfc-/NetworkTask have all acknowledged their
///        PowerDown, or until a timeout elapses.
/// @param ctx Owning RTOS context.
// Wartet, bis Scale-/Nfc-/NetworkTask ihren PowerDown tatsaechlich
// abgeschlossen haben (PowerDownAcknowledged je Task), bevor der echte
// Light-Sleep beginnt -- ein waehrend einer laufenden PN532-UART-Transaktion
// oder eines HX711-SCK-Toggles angehaltener Prozessortakt koennte sonst eine
// unvollstaendige Transaktion hinterlassen. Bricht nach
// kPowerSleepAckTimeoutMs ohnehin ab (ein haengender/verlorener Ack darf den
// Sleep nicht fuer immer blockieren); ReportInactivity-Meldungen, die genau
// in dieser kurzen Phase eintreffen, werden verworfen -- UiTask sendet
// ohnehin jede Sekunde erneut.
void waitForSleepQuiescence(rtos::RtosContext& ctx) {
  constexpr std::uint8_t kScaleBit =
      1U << static_cast<std::uint8_t>(rtos::PowerPeripheral::Scale);
  constexpr std::uint8_t kNfcBit =
      1U << static_cast<std::uint8_t>(rtos::PowerPeripheral::Nfc);
  constexpr std::uint8_t kNetworkBit =
      1U << static_cast<std::uint8_t>(rtos::PowerPeripheral::Network);
  constexpr std::uint8_t kAllBits = kScaleBit | kNfcBit | kNetworkBit;

  std::uint8_t received = 0;
  const TickType_t deadline =
      xTaskGetTickCount() + pdMS_TO_TICKS(config::kPowerSleepAckTimeoutMs);
  rtos::PowerCommand ack{};
  while (received != kAllBits) {
    const std::int32_t remaining =
        static_cast<std::int32_t>(deadline - xTaskGetTickCount());
    if (remaining <= 0) {
      FS_LOGW(services::LogComponent::Power,
              "Sleep quiescence timeout mask=0x%02X, sleeping anyway",
              static_cast<unsigned>(received));
      return;
    }
    if (xQueueReceive(ctx.powerCommandQueue, &ack,
                       static_cast<TickType_t>(remaining)) != pdTRUE) {
      continue;
    }
    if (ack.type == rtos::PowerCommandType::PowerDownAcknowledged) {
      received = static_cast<std::uint8_t>(
          received | (1U << static_cast<std::uint8_t>(ack.source)));
    }
  }
  FS_LOGD(services::LogComponent::Power,
          "Sleep quiescence complete mask=0x%02X",
          static_cast<unsigned>(received));
}

/// @brief Enters light sleep repeatedly until a real touch (GPIO) wake occurs.
/// @param ctx Owning RTOS context (unused directly, kept for symmetry with the other helpers).
// Blockiert bis zu einem echten Touch-Wake (GPIO-Ursache). Ein periodischer
// Timer-Wake dient als Sicherheitsnetz: das FT6336-INT-Verhalten (Pegel vs.
// Puls, Polaritaet) ist am realen Board noch nicht verifiziert -- ohne dieses
// Netz koennte ein falsch angenommener Wake-Pegel das Geraet dauerhaft im
// Sleep belassen. GPIO_INTR_LOW_LEVEL passt zur bestehenden LovyanGFX-
// Konfiguration des Pins als input_pullup (idle HIGH, siehe
// Touch_FT5x06::wakeup()) -- ein aktives Signal zieht ihn LOW.
void sleepUntilTouchWake(rtos::RtosContext& ctx) {
  const gpio_num_t touchPin =
      static_cast<gpio_num_t>(config::kTouchInterruptPin);
  gpio_wakeup_enable(touchPin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  for (;;) {
    esp_sleep_enable_timer_wakeup(
        static_cast<std::uint64_t>(config::kPowerSleepSafetyNetTimerMs) *
        1000ULL);
    esp_light_sleep_start();
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
      return;
    }
    FS_LOGD(services::LogComponent::Power,
            "Sleep safety-net wake, no touch detected, resuming sleep cause=%d",
            static_cast<int>(cause));
  }
}

}  // namespace

void powerTask(void* parameter) {
  auto& ctx = *static_cast<rtos::RtosContext*>(parameter);

  PowerState state = PowerState::Active;
  std::uint32_t inactiveMs = 0;
  rtos::PowerCommand command{};
  // Set right after a touch wake so the very next ReportInactivity's value
  // gets one diagnostic log line (see below) -- confirms whether
  // lv_display_trigger_activity() (UiBridge.cpp's SetBrightness handler)
  // actually produced a fresh, low inactivity value instead of the stale
  // pre-sleep one that previously sent the device straight back to sleep
  // (Nutzerbericht 2026-08-28).
  bool logNextInactivityReport = false;

  for (;;) {
    if (xQueueReceive(ctx.powerCommandQueue, &command,
                       pdMS_TO_TICKS(config::kPowerActivityReportIntervalMs)) ==
        pdTRUE) {
      switch (command.type) {
        case rtos::PowerCommandType::ReportInactivity:
          inactiveMs = command.inactiveMs;
          if (logNextInactivityReport) {
            logNextInactivityReport = false;
            FS_LOGI(services::LogComponent::Power,
                    "First post-wake inactivity report inactive_ms=%lu",
                    static_cast<unsigned long>(inactiveMs));
          }
          break;
        case rtos::PowerCommandType::PowerDownAcknowledged:
          FS_LOGD(services::LogComponent::Power,
                  "Power down acknowledged source=%u",
                  static_cast<unsigned>(command.source));
          break;
      }
    }

    const PowerState nextState = stateForInactivity(inactiveMs);
    if (nextState == state) continue;

    FS_LOGI(services::LogComponent::Power,
            "Power state changed from=%s to=%s inactive_ms=%lu",
            powerStateName(state), powerStateName(nextState),
            static_cast<unsigned long>(inactiveMs));
    state = nextState;
    sendBrightness(ctx, brightnessForState(state));
    if (state != PowerState::Sleep) continue;

    // LIGHT-SLEEP: Peripherie abschalten, auf Bestaetigung warten, dann
    // tatsaechlich schlafen -- blockiert diesen (und wegen des gemeinsamen
    // Prozessortakts effektiv jeden) Task, bis ein echter Touch-Wake
    // eintrifft. Der Ruecksprung aus sleepUntilTouchWake() ist deshalb
    // gleichbedeutend mit "Nutzer hat das Display beruehrt".
    sendScalePower(ctx, rtos::ScaleCommandType::PowerDown);
    sendNfcPower(ctx, rtos::NfcCommandType::PowerDown);
    sendNetworkPower(ctx, rtos::NetworkCommandType::PowerDown);
    waitForSleepQuiescence(ctx);

    const std::uint32_t sleepEnteredMs = millis();
    sleepUntilTouchWake(ctx);
    FS_LOGD(services::LogComponent::Power,
            "Touch wake detected, resuming after %lu ms asleep",
            static_cast<unsigned long>(millis() - sleepEnteredMs));

    // Nutzerbericht 2026-08-27: nach einem Aufwachen aus dem Light-Sleep
    // funktioniert das serielle Logging nicht mehr. Bereits in TASKS.md
    // Phase 11.6 als Risiko vermerkt und mit einem passenden Datenpunkt
    // beobachtet (esptool meldete nach einem Light-Sleep-Zyklus wiederholt
    // "Could not open COM5", bis das Kabel physisch neu gesteckt wurde) --
    // die native USB-CDC-Verbindung dieses Chips uebersteht einen
    // Light-Sleep-Zyklus offenbar nicht zuverlaessig und muss vom Host neu
    // erkannt werden. Ein erneuter Serial.begin() nach dem Aufwachen ist
    // der naheliegende, risikoarme Firmware-seitige Versuch (kein
    // Serial.end() davor -- LoggingTask koennte zum exakt selben Zeitpunkt
    // ebenfalls aus dem Sleep aufwachen und noch vor dem Sleep geloggte,
    // nicht mehr geleerte Zeilen nachtraeglich schreiben wollen; ein
    // zerstoerendes end() waere damit riskanter als ein reines
    // Neu-Initialisieren). Kein bekannter Weg, dies ohne echte Hardware zu
    // verifizieren -- falls das Problem bestehen bleibt, ist es
    // wahrscheinlich eine tiefere USB-PHY-/Host-Erkennungsgrenze, die eine
    // Firmware-seitige Behebung allein nicht loesen kann.
    Serial.begin(config::kSerialBaudRate);

    // UiTask keeps reporting inactivity every kPowerActivityReportIntervalMs
    // right up until the CPU actually halts in esp_light_sleep_start(), so at
    // least the most recent ReportInactivity (with a stale, large inactiveMs
    // from just before sleep) is typically still sitting unconsumed in the
    // queue -- it would otherwise be the very next message read below and
    // immediately re-trigger Sleep, undoing the wake before the screen was
    // even visible for a full second (observed on hardware: a touch
    // occasionally only flashes the display briefly before it goes dark
    // again, with the next touch then waking it properly). Drain everything
    // queued during the sleep-entry window so only fresh, post-wake reports
    // decide the next state.
    rtos::PowerCommand stale{};
    std::uint32_t drainedCount = 0;
    while (xQueueReceive(ctx.powerCommandQueue, &stale, 0) == pdTRUE) {
      ++drainedCount;
    }

    FS_LOGI(services::LogComponent::Power,
            "Woken by touch, resuming drained=%lu",
            static_cast<unsigned long>(drainedCount));
    inactiveMs = 0;
    state = PowerState::Active;
    sendBrightness(ctx, brightnessForState(state));
    sendScalePower(ctx, rtos::ScaleCommandType::PowerUp);
    sendNfcPower(ctx, rtos::NfcCommandType::PowerUp);
    sendNetworkPower(ctx, rtos::NetworkCommandType::PowerUp);
    sendWakeToast(ctx);
    // Nutzerbericht 2026-08-28: bestaetigt, dass die naechste
    // ReportInactivity-Meldung nach einem Wake tatsaechlich einen frischen,
    // niedrigen Wert traegt (siehe UiBridge.cpp's SetBrightness-Handler,
    // lv_display_trigger_activity()) statt eines uralten Werts von vor dem
    // Sleep, der PowerTask sofort wieder in Sleep schicken wuerde -- eine
    // einzelne Logzeile statt dauerhaftem Mitloggen jeder ReportInactivity,
    // um das Log im Normalbetrieb nicht zu fluten.
    logNextInactivityReport = true;
  }
}

}  // namespace filament_station::tasks
