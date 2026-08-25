#pragma once

#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace filament_station::config {

struct TaskSettings {
  const char* name;
  std::uint32_t stackSize;
  UBaseType_t priority;
  BaseType_t core;
};

constexpr BaseType_t kNoCoreAffinity = tskNO_AFFINITY;

constexpr TaskSettings kStorageTask{"StorageTask", 8192, 2, kNoCoreAffinity};
constexpr TaskSettings kLoggingTask{"LoggingTask", 2048, 1, kNoCoreAffinity};
// AppEvent contains NFC parsing results and persisted UID mappings by value;
// UI workflows additionally create fixed-size UiCommand objects. Bumped from
// 8192 after a real stack-overflow crash triggered by repeatedly pressing
// the diagnostics "Aktualisieren" button (Phase 10.1): handleUiAction is a
// very large, deeply branching function, and the added
// logTaskDiagnostics() call frame pushed a marginal peak over the edge.
constexpr TaskSettings kAppTask{"AppTask", 12288, 3, kNoCoreAffinity};
// A 20-row spool picker makes LVGL's rounded-rectangle mask renderer recurse
// more deeply while the scrollable result list is laid out and drawn. The
// former 8 KiB stack reached its canary during that render pass.
constexpr TaskSettings kUiTask{"UiTask", 16384, 2, kNoCoreAffinity};
// ScaleTask erzeugt die wertbasierte zentrale AppEvent-Nachricht auf dem
// Stack. AppEvent ist seit den Bambu-Phasen (8.4 PrinterState, 8.6
// BambuConfigCollection) nochmals deutlich gewachsen; 4096 Byte fuehrten zu
// einem Stack-Overflow-Absturz direkt beim Start. Groessenordnung jetzt
// analog zu den anderen AppEvent-erzeugenden Tasks (Storage/Spoolman/Bambu).
constexpr TaskSettings kScaleTask{"ScaleTask", 8192, 2, kNoCoreAffinity};
// Die Bambu-Erkennung kombiniert MIFARE-Authentifizierung, HKDF-SHA256,
// Parserdaten und wertbasierte AppEvent-Nachrichten. AppEvent ist seit den
// Bambu-Phasen (8.4 PrinterState, 8.6 BambuConfigCollection, plus Material/
// ColorHex je Slot) wiederholt gewachsen; 12288 Byte fuehrten beim Auflegen
// eines Tags zu einem Stack-Overflow-Absturz in reportTag(). Deutliche
// Reserve statt einer erneuten knappen Anpassung, da AppEvent absehbar
// weiterwaechst.
constexpr TaskSettings kNfcTask{"NfcTask", 16384, 2, kNoCoreAffinity};
// WiFiManager betreibt waehrend des Captive Portals DNS- und Webserver im
// NetworkTask. Dafuer wird mehr Stack als fuer das fruehere Queue-Geruest
// benoetigt.
constexpr TaskSettings kNetworkTask{"NetworkTask", 8192, 1, kNoCoreAffinity};
// HTTPClient, TLS and ArduinoJson are used together for nested Spoolman spool
// responses. Keep enough reserve for parsing without moving large objects
// through the central AppEvent queue. Bumped from 8192 after a real
// stack-overflow reboot loop in healthCheck()/sendResult(): the Spoolman
// auto-connect fix made ApplyConfiguration call healthCheck() on every
// boot (previously only a rare manual "Verbindung testen"), and several
// stack-local rtos::AppEvent locals in this file (now made static, see
// the matching AppTask/ScaleTask/NfcTask fixes) pushed the already-tight
// HTTP+TLS+JSON peak over the edge.
constexpr TaskSettings kSpoolmanTask{"SpoolmanTask", 10240, 1, kNoCoreAffinity};
// TLS-Handshakes (mbedTLS) fuer bis zu vier gleichzeitige MQTT-Verbindungen
// sowie ArduinoJson-Parsing der Statusberichte benoetigen mehr Reserve als
// das fruehere Taskgeruest; Groessenordnung analog zu kSpoolmanTask.
constexpr TaskSettings kBambuTask{"BambuTask", 8192, 1, kNoCoreAffinity};
// PowerTask haelt nur eine kleine Statemachine (Aktiv/Gedimmt/Sleep) und
// sendet/empfaengt ausschliesslich kleine PowerCommand-Werte -- kein
// AppEvent-grosser Stack-Local wie bei den anderen Tasks noetig.
constexpr TaskSettings kPowerTask{"PowerTask", 4096, 1, kNoCoreAffinity};

// Queue-Laengen basieren auf geringer Last der Task-Gerueste und werden nach
// Messung der maximalen Auslastung in spaeteren Phasen angepasst.
constexpr UBaseType_t kAppEventQueueLength = 16;
constexpr UBaseType_t kUiCommandQueueLength = 8;
constexpr UBaseType_t kServiceCommandQueueLength = 8;
constexpr UBaseType_t kStorageCommandQueueLength = 8;
constexpr UBaseType_t kLogQueueLength = 32;
// Bumped from 256: BambuTask logs the full ams_filament_setting MQTT
// payload (up to kBambuRequestPayloadCapacity=256 bytes JSON) for
// diagnosing why a printer accepts-then-reverts a slot reassignment; 256
// left no room for the "I [BAMBU] ..." prefix/label plus the JSON without
// truncating it, which is exactly the data that needed to be visible.
constexpr std::size_t kLogMessageCapacity = 320;

}  // namespace filament_station::config
