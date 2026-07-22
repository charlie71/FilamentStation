# AGENTS.md – FilamentStation

## 1. Projektziel

Entwickle eine eigenständige Filamentverwaltungsstation für 3D-Druck-Filamentrollen.

Das Gerät kombiniert:

* Filamentwaage
* Touch-Bedienoberfläche
* NFC-Leser
* Spoolman-Anbindung
* optionale direkte Bambu-Lab- und AMS-Anbindung

Spoolman ist die führende Filament- und Spulendatenbank.

Das Gerät verwendet keine eigene konkurrierende Filamentdatenbank. Lokal gespeichert werden ausschließlich Konfigurationen, Zwischenspeicher, Zuordnungen, noch nicht übertragene Messungen und Diagnosedaten.

Arbeitstitel:

`FilamentStation`

---

## 2. Technische Rahmenbedingungen

### Hardware

* WT32-SC01-Plus
* ESP32-S3
* Displayauflösung 480 × 320 Pixel
* kapazitiver Touchscreen
* vorhandenes PSRAM
* HX711 mit Wägezelle
* PN532 NFC-Leser
* SD-Karte
* optional Buzzer, Status-LED und Card-Detect-Eingang

### Entwicklungsumgebung

* Visual Studio Code
* PlatformIO
* Arduino Framework für ESP32
* FreeRTOS aus dem Arduino-ESP32-/ESP-IDF-Unterbau
* C++17
* Git
* Codex als Entwicklungsunterstützung

### Benutzeroberfläche

* LVGL 9.x
* LovyanGFX
* EEZ Studio
* kein EEZ Flow
* Anwendungslogik ausschließlich in C++

### Weitere Bibliotheken

Voraussichtlich erforderlich:

* ArduinoJson 7
* WiFiManager
* HX711
* PN532-Bibliothek
* HTTPClient
* MQTT-Client mit TLS-Unterstützung
* SD beziehungsweise SD_MMC, abhängig von der Hardware
* Preferences nur dort, wo der ESP32-Unterbau oder WiFiManager dies technisch benötigt

Alle Bibliotheken werden in `platformio.ini` mit einer geprüften Version festgelegt.

---

## 3. Bedeutung von „Prozess“

FreeRTOS verwendet Tasks und keine voneinander isolierten Betriebssystemprozesse.

In diesem Projekt bezeichnet „Prozess“ daher einen eigenständigen FreeRTOS-Task mit:

* klar abgegrenzter Verantwortlichkeit,
* eigener Task-Funktion,
* eigenem Stack,
* definierter Priorität,
* definierter Kommunikation,
* möglichst keinem gemeinsam veränderbaren Zustand.

Die Tasks kommunizieren über FreeRTOS-Mechanismen und rufen einander nicht direkt für lang laufende Tätigkeiten auf.

---

## 4. Vorgeschriebene Task-Architektur

Mindestens folgende Tasks sind vorzusehen:

```text
UiTask
AppTask
ScaleTask
NfcTask
StorageTask
NetworkTask
SpoolmanTask
BambuTask             optional ab späterer Projektphase
```

### 4.1 UiTask

Verantwortlich für:

* LVGL initialisieren
* EEZ-generierte Oberfläche betreiben
* Touch-Eingaben verarbeiten
* UI-Kommandos an den AppTask senden
* UI-Aktualisierungen vom AppTask empfangen
* `lv_timer_handler()` aufrufen

Nur der `UiTask` darf LVGL-Funktionen aufrufen.

Andere Tasks dürfen keine LVGL-Objekte direkt verändern.

### 4.2 AppTask

Verantwortlich für:

* zentralen Anwendungszustand
* Zustandsautomat
* Koordination aller Services
* Verarbeitung sämtlicher Ereignisse
* Senden von Kommandos an andere Tasks
* Entscheidung über Bildschirmwechsel
* Benutzerbestätigungen
* Fehlerbehandlung

Der `AppTask` ist die zentrale Steuerung und empfängt alle fachlichen Ereignisse über eine gemeinsame Event-Queue.

### 4.3 ScaleTask

Verantwortlich für:

* HX711 initialisieren
* Rohwerte lesen
* tarieren
* kalibrieren
* Messwerte filtern
* stabilen Messwert erkennen
* Kalibrierdaten über den `StorageTask` speichern
* Waagenereignisse an den `AppTask` senden

Der HX711-DOUT-Pin soll nach Möglichkeit einen Interrupt auslösen, sobald ein Messwert bereitsteht.

Die ISR liest nicht selbst den HX711 aus. Sie weckt lediglich den `ScaleTask` über eine Task Notification.

### 4.4 NfcTask

Verantwortlich für:

* PN532 initialisieren
* NFC-Tags erkennen
* UID lesen
* NDEF-Daten lesen
* NFC-Tags schreiben
* geschriebenen Inhalt verifizieren
* NFC-Ereignisse an den `AppTask` senden

Der PN532-IRQ-Pin soll verwendet werden, sofern das eingesetzte Modul und die gewählte Schnittstelle ihn unterstützen.

Die ISR führt keine I²C- oder NFC-Kommunikation aus. Sie weckt nur den `NfcTask`.

### 4.5 StorageTask

Verantwortlich für:

* SD-Karte initialisieren
* Verzeichnisstruktur anlegen
* JSON-Dateien lesen
* JSON-Dateien validieren
* JSON-Dateien atomar speichern
* Backups verwalten
* ausstehende Messungen speichern
* Cache-Dateien verwalten

Nur der `StorageTask` darf direkt auf die SD-Karte zugreifen.

Andere Tasks senden Speicheranfragen über eine Queue.

Damit wird ein gleichzeitiger Zugriff mehrerer Tasks auf das Dateisystem verhindert.

### 4.6 NetworkTask

Verantwortlich für:

* WiFiManager
* Captive Portal
* WLAN-Erstkonfiguration
* WLAN-Wiederverbindung
* Verarbeitung der Arduino-WiFi-Events
* Meldung des Netzwerkstatus
* Starten einer erneuten WLAN-Konfiguration auf Benutzeranforderung

Der `NetworkTask` führt keine Spoolman- oder Bambu-Fachlogik aus.

### 4.7 SpoolmanTask

Verantwortlich für:

* HTTP-Kommunikation mit Spoolman
* Spule laden
* Spulen suchen
* Gewicht übertragen
* Antworten parsen
* Timeouts behandeln
* Spoolman-Verfügbarkeit prüfen

HTTP-Aufrufe dürfen den `UiTask` oder `AppTask` nicht blockieren.

### 4.8 BambuTask

Erst nach Fertigstellung des Spoolman-Wiegeablaufs implementieren.

Verantwortlich für:

* MQTT-Verbindung zum Bambu-Drucker
* Druckerstatus
* AMS-Status
* AMS-Slot-Zuordnung
* Wiederverbindung
* Protokollfehler

Die Bambu-Funktion ist optional. Ein Ausfall darf die Waage, NFC oder Spoolman nicht blockieren.

---

## 5. Kommunikationsarchitektur

### 5.1 Zentrale Event-Queue

Alle fachlichen Ereignisse werden an den `AppTask` gesendet:

```cpp
QueueHandle_t appEventQueue;
```

Beispiele:

```cpp
enum class AppEventType {
    UiCommand,
    ScaleReady,
    ScaleStable,
    ScaleError,
    NfcTagDetected,
    NfcTagRead,
    NfcTagWritten,
    NfcError,
    SdMounted,
    SdError,
    WifiConnected,
    WifiDisconnected,
    WifiConfigPortalStarted,
    SpoolmanConnected,
    SpoolmanResponse,
    SpoolmanError,
    BambuConnected,
    BambuUpdate,
    BambuError
};
```

### 5.2 Command-Queues

Für jeden Service wird eine eigene Command-Queue vorgesehen:

```cpp
QueueHandle_t uiCommandQueue;
QueueHandle_t scaleCommandQueue;
QueueHandle_t nfcCommandQueue;
QueueHandle_t storageCommandQueue;
QueueHandle_t networkCommandQueue;
QueueHandle_t spoolmanCommandQueue;
QueueHandle_t bambuCommandQueue;
```

Beispiele:

```cpp
enum class ScaleCommandType {
    Tare,
    StartCalibration,
    ResetCalibration,
    RequestMeasurement
};

enum class NfcCommandType {
    StartReading,
    StopReading,
    WriteSpoolTag
};

enum class StorageCommandType {
    LoadJson,
    SaveJson,
    DeleteJson,
    CreateBackup
};
```

### 5.3 Event Groups

Globale Systemzustände werden über eine Event Group abgebildet:

```cpp
EventGroupHandle_t systemEventGroup;
```

Vorgesehene Bits:

```cpp
constexpr EventBits_t EVENT_UI_READY        = BIT0;
constexpr EventBits_t EVENT_SD_READY        = BIT1;
constexpr EventBits_t EVENT_SCALE_READY     = BIT2;
constexpr EventBits_t EVENT_NFC_READY       = BIT3;
constexpr EventBits_t EVENT_WIFI_CONNECTED  = BIT4;
constexpr EventBits_t EVENT_SPOOLMAN_READY  = BIT5;
constexpr EventBits_t EVENT_BAMBU_READY     = BIT6;
constexpr EventBits_t EVENT_FATAL_ERROR     = BIT7;
```

Event Groups übertragen nur Zustände, keine komplexen Daten.

### 5.4 Task Notifications

Task Notifications werden bevorzugt für schnelle Ereignisse verwendet, bei denen genau ein Task geweckt werden soll.

Vorgesehene Anwendungen:

* HX711 Data Ready ISR → `ScaleTask`
* PN532 IRQ ISR → `NfcTask`
* optional Touch IRQ → `UiTask`
* optional SD Card Detect ISR → `StorageTask`

Beispielprinzip:

```cpp
void IRAM_ATTR hx711DataReadyIsr()
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(
        scaleTaskHandle,
        &higherPriorityTaskWoken
    );

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
```

### 5.5 Mutexes

Mutexes werden nur für gemeinsam verwendete Ressourcen eingesetzt.

Mögliche gemeinsame Ressourcen:

* I²C-Bus
* SPI-Bus, falls mehrere Geräte denselben Bus verwenden
* Debug-Ausgabe, falls Lognachrichten sonst vermischt werden

Beispiel:

```cpp
SemaphoreHandle_t i2cMutex;
```

Ein Mutex darf nicht aus einer ISR verwendet werden.

Da ausschließlich der `StorageTask` auf die SD-Karte zugreift, ist für die SD-Karte im Normalfall kein zusätzlicher Mutex notwendig.

### 5.6 Semaphoren

Binäre oder zählende Semaphoren dürfen verwendet werden, wenn eine Task-Synchronisation benötigt wird.

Für einfache ISR-zu-Task-Signale sind Task Notifications zu bevorzugen.

### 5.7 Nachrichtenregeln

* Keine Zeiger auf lokale Stackvariablen über Queues senden.
* Kleine Nachrichten als Wert übertragen.
* Keine großen dynamischen `String`-Objekte in Queues verwenden.
* Bevorzugt feste Strukturen und begrenzte Zeichenfelder verwenden.
* Jede Anfrage erhält bei Bedarf eine `requestId`.
* Antworten enthalten dieselbe `requestId`.
* Queue-Längen und erwartete Last dokumentieren.
* Queue-Überläufe müssen erkannt und protokolliert werden.
* ISR-Code verwendet ausschließlich dafür vorgesehene `FromISR`-Funktionen.

---

## 6. Polling vermeiden

Nicht erlaubt sind schnelle Schleifen wie:

```cpp
while (true) {
    if (flag) {
        // ...
    }
    delay(10);
}
```

Tasks sollen stattdessen blockieren:

```cpp
xQueueReceive(queue, &message, portMAX_DELAY);
```

oder:

```cpp
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

oder:

```cpp
xEventGroupWaitBits(...);
```

Zeitgesteuerte Funktionen werden umgesetzt mit:

* FreeRTOS Software Timern
* `vTaskDelayUntil()`
* LVGL-internen Timern
* Netzwerk-Callbacks
* Hardwareinterrupts

Zulässige periodische Vorgänge sind:

* notwendiger Aufruf von `lv_timer_handler()`
* definierte Verbindungs-Health-Checks
* Watchdog-Überwachung
* Sensoren ohne verfügbare Interruptleitung

Ein periodischer Fallback muss dokumentiert und so langsam wie technisch vertretbar ausgeführt werden.

Busy Waiting ist nicht erlaubt.

---

## 7. Interrupt-Regeln

Interrupts werden verwendet, wenn die Hardware dies sinnvoll unterstützt.

Bevorzugte Interruptquellen:

* HX711 DOUT
* PN532 IRQ
* Touch IRQ
* SD Card Detect
* optionale Hardwaretaste

Eine ISR darf nur:

* Interruptquelle quittieren,
* kleinen atomaren Zustand setzen,
* Task Notification senden,
* Queue-Nachricht mit `FromISR` senden,
* einen Taskwechsel anfordern.

Eine ISR darf nicht:

* JSON verarbeiten,
* SD-Karte verwenden,
* I²C- oder SPI-Transaktionen durchführen,
* HTTP oder MQTT aufrufen,
* LVGL-Funktionen aufrufen,
* lange rechnen,
* dynamischen Speicher anfordern,
* umfangreiche serielle Ausgaben erzeugen.

Alle ISR-Funktionen erhalten `IRAM_ATTR`, sofern dies für die Plattform erforderlich ist.

---

## 8. LVGL und Multitasking

LVGL gehört ausschließlich dem `UiTask`.

Nur der `UiTask` darf:

* LVGL-Objekte erzeugen,
* Texte ändern,
* Screens wechseln,
* Styles ändern,
* Dialoge öffnen,
* `lv_timer_handler()` aufrufen.

Andere Tasks senden `UiCommand`-Nachrichten an den `UiTask`.

Beispiel:

```cpp
struct UiCommand {
    UiCommandType type;
    uint32_t requestId;
    float weightGrams;
    char title[48];
    char text[160];
};
```

Der `UiTask` wartet zwischen den Aufrufen von `lv_timer_handler()` auf:

* UI-Kommandos,
* Touchereignisse,
* den von LVGL zurückgegebenen nächsten Ausführungszeitpunkt.

Direkte LVGL-Aufrufe aus Interrupts sind verboten.

Der EEZ-generierte Code befindet sich ausschließlich in:

```text
src/ui/generated/
```

Generierte Dateien werden nicht manuell verändert.

Eigene Eventhandler befinden sich in:

```text
src/ui/actions/
src/ui/UiBridge.cpp
```

---

## 9. WiFiManager

Die WLAN-Verbindung wird mit WiFiManager eingerichtet.

### Erstinbetriebnahme

Wenn keine gültige WLAN-Konfiguration vorhanden ist oder keine Verbindung hergestellt werden kann:

1. `NetworkTask` meldet `WifiConfigPortalStarted`.
2. WiFiManager startet einen Access Point.
3. Benutzer verbindet sich mit dem Access Point.
4. Captive Portal wird geöffnet.
5. Benutzer wählt WLAN und gibt das Passwort ein.
6. Gerät versucht die Verbindung.
7. Ergebnis wird an den `AppTask` gesendet.

### Erneute Konfiguration

Die Benutzeroberfläche bietet:

* „WLAN neu konfigurieren“
* „Gespeicherte WLAN-Verbindung löschen“
* „Captive Portal starten“

Der entsprechende Befehl wird über die `networkCommandQueue` gesendet.

### Blockierender Betrieb

WiFiManager darf im eigenen `NetworkTask` blockierend betrieben werden.

Damit bleibt die übrige Anwendung weiterhin aktiv:

* UI funktioniert weiter.
* Waage funktioniert weiter.
* NFC funktioniert weiter.
* StorageTask funktioniert weiter.

Ein wiederholter Aufruf von `WiFiManager::process()` in der Arduino-`loop()`-Funktion soll vermieden werden.

### WiFi-Events

Arduino-WiFi-Callbacks dürfen ausschließlich:

* kurze Ereignisstrukturen erzeugen,
* Event Bits setzen oder löschen,
* Nachrichten an den `AppTask` senden.

Sie dürfen nicht direkt:

* die Benutzeroberfläche verändern,
* JSON-Dateien schreiben,
* Spoolman aufrufen,
* gemeinsam verwendete komplexe Daten verändern.

---

## 10. SD-Karte und JSON-Speicherung

### 10.1 Geltungsbereich

Alle vom Gerät erzeugten oder verwalteten persistenten Anwendungsdateien werden als gültige JSON-Dateien auf der SD-Karte gespeichert.

Dies betrifft:

* Gerätekonfiguration
* Spoolman-Konfiguration
* Bambu-Konfiguration
* UI-Einstellungen
* Waagenkalibrierung
* NFC-Zuordnungen
* Cache
* ausstehende Messungen
* Diagnoseinformationen
* strukturierte Fehlerprotokolle

Quellcode, Firmware, kompilierte Ressourcen und Bibliotheken sind davon nicht betroffen.

### 10.2 Ausnahme für WLAN-Zugangsdaten

WiFiManager beziehungsweise der ESP32-WiFi-Unterbau verwaltet SSID und WLAN-Passwort standardmäßig im internen Systembereich.

Diese systemverwalteten WLAN-Zugangsdaten sind die einzige zulässige Ausnahme von der SD-/JSON-Regel.

Zusätzliche Netzwerkparameter werden auf SD gespeichert:

```text
/config/network.json
```

Darin können beispielsweise stehen:

* Hostname
* DHCP oder statische IP
* DNS
* Access-Point-Name
* Portal-Timeout
* Verbindungs-Timeout

Das WLAN-Passwort darf nicht zusätzlich unverschlüsselt auf der SD-Karte gespeichert werden.

### 10.3 Verzeichnisstruktur

```text
/config/device.json
/config/network.json
/config/spoolman.json
/config/bambu.json
/config/ui.json
/config/scale.json
/config/nfc.json

/cache/spools.json
/cache/filaments.json
/cache/printers.json

/queue/pending-measurements.json
/queue/pending-actions.json

/mappings/nfc-spools.json
/mappings/bambu-tags.json

/diagnostics/system.json
/diagnostics/last-error.json
/diagnostics/task-stats.json

/logs/events.json
```

### 10.4 JSON-Schema

Jede Datei enthält mindestens:

```json
{
  "schemaVersion": 1,
  "updatedAt": "2026-07-22T18:00:00Z"
}
```

Jede Datei besitzt:

* definierte maximale Größe,
* dokumentiertes Schema,
* Versionsnummer,
* Pflichtfelder,
* Standardwerte,
* Validierung beim Laden.

### 10.5 Atomisches Speichern

Dateien werden nicht direkt überschrieben.

Vorgesehener Ablauf:

1. neue Daten serialisieren,
2. in eine temporäre JSON-Datei schreiben,
3. Datei schließen und flushen,
4. temporäre Datei erneut öffnen,
5. JSON validieren,
6. vorhandene Datei als Backup umbenennen,
7. temporäre Datei zur Zieldatei umbenennen,
8. altes Backup bei Erfolg entfernen.

Beispiel:

```text
/config/scale.tmp.json
/config/scale.bak.json
/config/scale.json
```

Auch temporäre Dateien und Backups enthalten gültiges JSON.

### 10.6 Schreibzugriffe

Nur der `StorageTask` schreibt oder liest Dateien.

Andere Tasks verwenden:

```cpp
struct StorageCommand {
    StorageCommandType type;
    uint32_t requestId;
    char path[96];
    StorageDocumentType documentType;
};
```

Große Dokumente dürfen nicht vollständig als Queue-Kopie übertragen werden.

Stattdessen sind klar definierte Datenmodelle oder kontrollierte Speicherpuffer zu verwenden.

### 10.7 Verhalten ohne SD-Karte

Ohne funktionsfähige SD-Karte:

* erscheint eine deutliche Fehlermeldung,
* werden keine Einstellungen dauerhaft geändert,
* werden keine Messungen als gespeichert angezeigt,
* darf das Gerät in einen Diagnose- oder eingeschränkten Betriebsmodus wechseln,
* darf es keinen stillen Fallback auf eine zweite lokale Datenbank geben.

---

## 11. Projektstruktur

```text
FilamentStation/
├── AGENTS.md
├── TASKS.md
├── README.md
├── platformio.ini
├── docs/
│   ├── architecture.md
│   ├── hardware.md
│   ├── rtos.md
│   ├── storage.md
│   ├── json-schemas.md
│   ├── spoolman-api.md
│   └── decisions/
├── test/
│   ├── test_weight_filter/
│   ├── test_tag_payload/
│   ├── test_json_storage/
│   ├── test_spoolman_models/
│   └── test_app_state/
├── ui-project/
│   └── FilamentStation.eez-project
└── src/
    ├── main.cpp
    ├── app/
    │   ├── AppTask.cpp
    │   ├── AppTask.h
    │   ├── ApplicationState.h
    │   └── ApplicationController.cpp
    ├── config/
    │   ├── BoardConfig.h
    │   ├── AppConfig.h
    │   ├── TaskConfig.h
    │   └── Secrets.example.h
    ├── rtos/
    │   ├── RtosContext.cpp
    │   ├── RtosContext.h
    │   ├── Events.h
    │   ├── Commands.h
    │   └── Messages.h
    ├── tasks/
    │   ├── UiTask.cpp
    │   ├── ScaleTask.cpp
    │   ├── NfcTask.cpp
    │   ├── StorageTask.cpp
    │   ├── NetworkTask.cpp
    │   ├── SpoolmanTask.cpp
    │   └── BambuTask.cpp
    ├── drivers/
    │   ├── DisplayDriver.cpp
    │   ├── TouchDriver.cpp
    │   ├── ScaleDriver.cpp
    │   ├── NfcDriver.cpp
    │   └── SdCardDriver.cpp
    ├── services/
    │   ├── WeightFilter.cpp
    │   ├── JsonStorage.cpp
    │   ├── SpoolmanClient.cpp
    │   ├── NetworkManager.cpp
    │   └── BambuClient.cpp
    ├── models/
    │   ├── Spool.h
    │   ├── Filament.h
    │   ├── ScaleMeasurement.h
    │   ├── NfcTag.h
    │   └── AmsTray.h
    └── ui/
        ├── UiBridge.cpp
        ├── actions/
        └── generated/
```

---

## 12. Task-Konfiguration

Tasknamen, Prioritäten, Stackgrößen und Core-Affinitäten werden ausschließlich in folgender Datei definiert:

```text
src/config/TaskConfig.h
```

Beispielstruktur:

```cpp
struct TaskSettings {
    const char* name;
    uint32_t stackSize;
    UBaseType_t priority;
    BaseType_t core;
};
```

Keine Stackgrößen oder Prioritäten direkt in `main.cpp` verteilen.

Core-Pinning wird nur verwendet, wenn es einen dokumentierten Grund gibt.

Nach Hardwaretests sind zu dokumentieren:

* `uxTaskGetStackHighWaterMark()`
* Task-Laufzeiten
* Queue-Auslastung
* Heap
* PSRAM
* mögliche Prioritätsprobleme

---

## 13. Startablauf

`setup()` führt nur die Systeminitialisierung aus:

1. serielle Diagnose starten,
2. grundlegende Hardware prüfen,
3. FreeRTOS-Objekte erzeugen,
4. Queues erzeugen,
5. Event Group erzeugen,
6. Mutexes erzeugen,
7. Tasks starten,
8. Arduino-Haupttask blockieren oder beenden.

Die Arduino-`loop()`-Funktion enthält keine Anwendungslogik.

Alle weitere Arbeit erfolgt in den FreeRTOS-Tasks.

Der `StorageTask` wird früh gestartet, da Konfigurationen von der SD-Karte benötigt werden.

Der `AppTask` wartet auf die notwendigen Ready-Events.

---

## 14. Fehlerbehandlung

Jeder Task muss unterscheiden zwischen:

* temporärem Fehler,
* wiederholbarem Fehler,
* eingeschränktem Betrieb,
* schwerwiegendem Systemfehler.

Fehler werden als strukturierte Ereignisse an den `AppTask` gemeldet.

Beispiel:

```cpp
struct ErrorEvent {
    ErrorSource source;
    ErrorCode code;
    ErrorSeverity severity;
    uint32_t requestId;
    char message[128];
};
```

Tasks dürfen bei normalen Kommunikations- oder Hardwarefehlern nicht eigenständig den ESP32 neu starten.

Ein kontrollierter Neustart wird ausschließlich vom `AppTask` angefordert.

---

## 15. Tests

Hardwareunabhängig zu testen sind mindestens:

* Gewichtsmittelwert
* Tiefpassfilter
* Stabilitätserkennung
* Kalibrierberechnung
* NFC-Payload-Parser
* JSON-Schema-Validierung
* Laden fehlender JSON-Felder
* Migration älterer Schema-Versionen
* atomisches Speichern
* Wiederherstellen einer Backup-Datei
* Spoolman-JSON-Parser
* Zustandsautomat
* Queue-Nachrichtentypen
* Behandlung doppelter oder verspäteter Antworten

Zusätzlich sind Integrationstests zu dokumentieren für:

* Interrupt → Task Notification
* Task → Queue → AppTask
* AppTask → Command-Queue → Service-Task
* StorageTask mit mehreren gleichzeitigen Anfragen
* WLAN-Abbruch während HTTP-Anfrage
* Entfernen der SD-Karte während eines Schreibvorgangs

---

## 16. Regeln für Codex

Vor jeder Änderung:

1. `AGENTS.md` vollständig lesen.
2. `TASKS.md` vollständig lesen.
3. aktuelle Phase bestimmen.
4. bestehende Architektur prüfen.
5. kurze Umsetzungsschritte formulieren.

Während der Implementierung:

* nur die angeforderte Phase bearbeiten,
* keine monolithische Superklasse erstellen,
* keine Hardwarezugriffe im `AppTask`,
* keine UI-Aufrufe außerhalb des `UiTask`,
* keine SD-Zugriffe außerhalb des `StorageTask`,
* keine lang laufende Arbeit in ISRs,
* keine Busy-Wait-Schleifen,
* keine Statusabfragen in schnellen Polling-Schleifen,
* Queue- und Event-Wartezeiten sinnvoll festlegen,
* alle Task- und Queue-Erzeugungen auf Fehler prüfen,
* keine GPIO-Belegungen erfinden,
* keine Zugangsdaten eintragen,
* keine erfolgreichen Hardwaretests behaupten, die nicht durchgeführt wurden.

Nach jedem Arbeitsschritt:

1. `pio run` ausführen,
2. verfügbare Tests ausführen,
3. Compilerwarnungen prüfen,
4. geänderte Dateien zusammenfassen,
5. Task- und Queue-Annahmen dokumentieren,
6. nur tatsächlich erledigte Punkte in `TASKS.md` abhaken.

Priorität:

1. RTOS-Grundarchitektur
2. SD- und JSON-Speicherung
3. Display, Touch und LVGL
4. Waage mit Interrupt
5. NFC mit Interrupt
6. WiFiManager
7. Spoolman
8. vollständiger Wiegeablauf
9. Bambu und AMS
10. Robustheit und Dokumentation
