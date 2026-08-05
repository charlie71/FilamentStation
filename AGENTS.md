# AGENTS.md – FilamentStation

## 1. Projektziel

Entwickle eine eigenständige Filamentverwaltungsstation für 3D-Druck-Filamentrollen.

Das Gerät kombiniert:

* eine Filamentwaage,
* eine Touch-Bedienoberfläche,
* einen NFC-Leser,
* eine direkte Spoolman-Anbindung,
* eine direkte Bambu-Lab-Anbindung,
* die Verwaltung mehrerer Bambu-Lab-Drucker,
* die Darstellung und Konfiguration von AMS-Slots.

Arbeitstitel:

```text
FilamentStation
```

Das Zielgerät ist der WT32-SC01-Plus mit integriertem 480 × 320-Pixel-Touchdisplay.

Spoolman ist die führende Datenbank für:

* Hersteller,
* Filamente,
* Spulen,
* Leergewichte,
* Ausgangsgewichte,
* verbleibende Filamentmengen,
* Spulenstatus.

Das Gerät besitzt keine zweite konkurrierende Filamentdatenbank.

Lokal gespeichert werden ausschließlich:

* Gerätekonfiguration,
* Netzwerkkonfiguration,
* Spoolman-Konfiguration,
* Bambu-Druckerkonfiguration,
* Waagenkalibrierung,
* UI-Einstellungen,
* NFC-zu-Spoolman-Zuordnungen,
* Drucker- und Slot-Zuordnungen,
* Cache-Daten,
* noch nicht übertragene Messungen,
* Diagnose- und Fehlerdaten.

---

# 2. Zielhardware

## 2.1 Hauptgerät

* WT32-SC01-Plus
* ESP32-S3
* Displayauflösung 480 × 320 Pixel
* kapazitiver Touchscreen
* vorhandenes PSRAM
* integrierter SD-Kartenanschluss beziehungsweise vorhandene SD-Schnittstelle

## 2.2 Externe Hardware

* HX711
* geeignete Wägezelle
* PN532 NFC-Leser
* NFC-Tags, vorzugsweise NTAG213, NTAG215 oder NTAG216
* optional Buzzer
* optional Status-LED
* optional Hardwaretaste
* optional SD-Card-Detect-Eingang

## 2.3 GPIO-Regel

Keine GPIO-Belegung erfinden.

Alle Pins müssen zentral definiert werden in:

```text
src/config/BoardConfig.h
```

Vor der Festlegung von Pins müssen geprüft werden:

* offizielle Board-Dokumentation,
* Displaypins,
* Touchpins,
* SD-Kartenpins,
* PSRAM-Nutzung,
* USB-Pins,
* Boot-Strapping-Pins,
* verfügbare externe GPIOs,
* Interruptfähigkeit der vorgesehenen Eingänge.

Display-, Touch-, SD-, USB- und Boot-Pins dürfen nicht versehentlich für HX711 oder PN532 verwendet werden.

---

# 3. Software-Stack

## 3.1 Entwicklungsumgebung

* Visual Studio Code
* PlatformIO
* Arduino Framework für ESP32
* FreeRTOS aus dem Arduino-ESP32-/ESP-IDF-Unterbau
* C++17
* Git
* Codex in Visual Studio Code

## 3.2 Benutzeroberfläche

* LVGL 9.x
* LovyanGFX
* EEZ Studio
* EEZ Studio ohne EEZ Flow
* Anwendungslogik in C++

## 3.3 Weitere Bibliotheken

Voraussichtlich erforderlich:

* ArduinoJson 7
* WiFiManager
* HX711-Bibliothek oder eigener kleiner HX711-Treiber
* PN532-Bibliothek
* HTTPClient oder ein vergleichbarer ESP32-HTTP-Client
* MQTT-Client mit TLS-Unterstützung
* SD oder SD_MMC
* Preferences nur für systembedingte Daten, die nicht sinnvoll auf SD liegen

Alle Bibliotheken müssen in `platformio.ini` mit festen, geprüften Versionen eingetragen werden.

Keine unnötigen Bibliotheken hinzufügen.

---

# 4. Bedeutung von „Prozess“

FreeRTOS verwendet Tasks und keine voneinander isolierten Betriebssystemprozesse.

In diesem Projekt bezeichnet „Prozess“ daher einen eigenständigen FreeRTOS-Task mit:

* eigener Task-Funktion,
* eigenem Stack,
* definierter Priorität,
* klarer Verantwortung,
* definierter Kommunikation,
* möglichst keinem gemeinsam veränderbaren Zustand.

Tasks kommunizieren über:

* Queues,
* Task Notifications,
* Event Groups,
* Mutexes,
* Semaphoren.

Lang laufende Tätigkeiten dürfen nicht durch direkte synchrone Aufrufe zwischen Tasks gekoppelt werden.

---

# 5. Vorgeschriebene Task-Architektur

Mindestens folgende Tasks sind vorzusehen:

```text
UiTask
AppTask
ScaleTask
NfcTask
StorageTask
NetworkTask
SpoolmanTask
BambuTask
```

## 5.1 UiTask

Verantwortlich für:

* LVGL initialisieren,
* EEZ-generierte Oberfläche betreiben,
* Touch-Eingaben verarbeiten,
* UI-Kommandos empfangen,
* UI-Aktionen an den AppTask senden,
* `lv_timer_handler()` aufrufen,
* Screens wechseln,
* Dialoge öffnen,
* UI-Modelle aktualisieren.

Nur der UiTask darf LVGL-Funktionen aufrufen.

Andere Tasks dürfen keine LVGL-Objekte direkt verändern.

## 5.2 AppTask

Verantwortlich für:

* zentralen Anwendungszustand,
* fachlichen Zustandsautomaten,
* Koordination aller Tasks,
* Verarbeitung fachlicher Ereignisse,
* Erzeugen von Kommandos für andere Tasks,
* Prüfung von `requestId`,
* Prüfung von `printerId`,
* Fehlerbehandlung,
* Entscheidung über Navigation und Workflows.

Der AppTask führt keine direkten Hardwarezugriffe aus.

## 5.3 ScaleTask

Verantwortlich für:

* HX711 initialisieren,
* Messwerte lesen,
* tarieren,
* kalibrieren,
* Messwerte filtern,
* Ausreißer erkennen,
* stabile Messwerte erkennen,
* Waagenstatus melden,
* Kalibrierdaten über den StorageTask laden und speichern.

Der HX711-DOUT-Pin soll nach Möglichkeit als Interruptquelle verwendet werden.

Die ISR liest den HX711 nicht aus.

Die ISR weckt ausschließlich den ScaleTask über eine Task Notification.

## 5.4 NfcTask

Verantwortlich für:

* PN532 initialisieren,
* Tags erkennen,
* UID lesen,
* NDEF lesen,
* Tags schreiben,
* Tags löschen,
* geschriebene Daten verifizieren,
* Bambu-Tags erkennen,
* Legacy-Tags erkennen,
* NFC-Ereignisse melden.

Der PN532-IRQ-Pin soll verwendet werden, sofern Hardware und Schnittstelle dies unterstützen.

Die ISR führt keine I²C-, SPI- oder NFC-Kommunikation aus.

## 5.5 StorageTask

Verantwortlich für:

* SD-Karte initialisieren,
* Verzeichnisstruktur anlegen,
* JSON-Dateien lesen,
* JSON-Dateien validieren,
* JSON-Dateien atomar speichern,
* Backups verwalten,
* Cache verwalten,
* ausstehende Messungen speichern,
* Diagnoseinformationen speichern.

Nur der StorageTask darf direkt auf die SD-Karte zugreifen.

## 5.6 NetworkTask

Verantwortlich für:

* WiFiManager,
* WLAN-Erstkonfiguration,
* Captive Portal,
* WLAN-Wiederverbindung,
* WiFi-Events,
* Netzwerkstatus,
* erneute WLAN-Konfiguration.

Der NetworkTask führt keine Spoolman- oder Bambu-Fachlogik aus.

## 5.7 SpoolmanTask

Verantwortlich für:

* Spoolman-Health-Check,
* Spulen laden,
* Spulen suchen,
* Hersteller suchen,
* Filamente suchen,
* Hersteller anlegen,
* Filamente anlegen,
* Spulen anlegen,
* Gewicht aktualisieren,
* Antworten parsen,
* Timeouts und HTTP-Fehler behandeln.

HTTP-Aufrufe dürfen den UiTask und AppTask nicht blockieren.

## 5.8 BambuTask

Verantwortlich für:

* mehrere Bambu-Drucker verwalten,
* Druckerverbindungen aufbauen,
* MQTT-Verbindungen verwalten,
* Druckerstatus lesen,
* AMS-Einheiten lesen,
* Slots lesen,
* externen Spulenhalter lesen,
* Slotdaten schreiben,
* Slotdaten zurücksetzen,
* Wiederverbindung,
* Druckerwechsel.

Die gesamte Architektur muss mehrere Drucker unterstützen.

---

# 6. FreeRTOS-Kommunikation

## 6.1 Zentrale App-Event-Queue

Alle fachlichen Ereignisse werden an den AppTask gesendet:

```cpp
QueueHandle_t appEventQueue;
```

Beispiel:

```cpp
enum class AppEventType {
    UiAction,

    ScaleInitialized,
    ScaleMeasurement,
    ScaleStable,
    ScaleUnstable,
    ScaleTared,
    ScaleCalibrated,
    ScaleError,

    NfcInitialized,
    NfcTagDetected,
    NfcTagRemoved,
    NfcTagRead,
    NfcTagWritten,
    NfcTagErased,
    NfcError,

    StorageReady,
    StorageResponse,
    StorageError,

    WifiConnecting,
    WifiConnected,
    WifiDisconnected,
    WifiGotIp,
    WifiConfigPortalStarted,
    WifiConfigPortalStopped,
    WifiError,

    SpoolmanConnected,
    SpoolmanResponse,
    SpoolmanError,

    BambuPrinterConnecting,
    BambuPrinterConnected,
    BambuPrinterDisconnected,
    BambuPrinterUpdated,
    BambuAmsUpdated,
    BambuTrayUpdated,
    BambuError,

    TimerExpired,
    FatalError
};
```

## 6.2 Command-Queues

Für jeden Task wird eine eigene Command-Queue vorgesehen:

```cpp
QueueHandle_t uiCommandQueue;
QueueHandle_t scaleCommandQueue;
QueueHandle_t nfcCommandQueue;
QueueHandle_t storageCommandQueue;
QueueHandle_t networkCommandQueue;
QueueHandle_t spoolmanCommandQueue;
QueueHandle_t bambuCommandQueue;
```

## 6.3 Event Group

Globale Systemzustände werden über eine Event Group abgebildet:

```cpp
EventGroupHandle_t systemEventGroup;
```

Vorgesehene Bits:

```cpp
constexpr EventBits_t EVENT_UI_READY         = BIT0;
constexpr EventBits_t EVENT_SD_READY         = BIT1;
constexpr EventBits_t EVENT_SCALE_READY      = BIT2;
constexpr EventBits_t EVENT_NFC_READY        = BIT3;
constexpr EventBits_t EVENT_WIFI_CONNECTED   = BIT4;
constexpr EventBits_t EVENT_SPOOLMAN_READY   = BIT5;
constexpr EventBits_t EVENT_BAMBU_READY      = BIT6;
constexpr EventBits_t EVENT_FATAL_ERROR      = BIT7;
```

Event Groups übertragen nur Zustände und keine komplexen Daten.

## 6.4 Task Notifications

Task Notifications werden bevorzugt für schnelle Signale verwendet, bei denen genau ein Task geweckt wird.

Vorgesehene Anwendungen:

* HX711-DOUT-ISR → ScaleTask
* PN532-IRQ-ISR → NfcTask
* Touch-IRQ → UiTask, falls sinnvoll
* SD-Card-Detect-ISR → StorageTask
* Hardwaretaste → zuständiger Task

## 6.5 Mutexes

Mutexes werden nur für gemeinsam verwendete Ressourcen eingesetzt.

Mögliche Ressourcen:

* gemeinsamer I²C-Bus,
* gemeinsamer SPI-Bus,
* zentrale Logging-Ausgabe.

Da nur der StorageTask auf die SD-Karte zugreift, ist für das Dateisystem normalerweise kein zusätzlicher Mutex notwendig.

## 6.6 Nachrichtenregeln

* Keine Zeiger auf lokale Stackvariablen über Queues senden.
* Kleine Strukturen als Wert übertragen.
* Keine großen Arduino-`String`-Objekte in Queues senden.
* Begrenzte Zeichenfelder oder feste Datenstrukturen verwenden.
* Jede asynchrone Anfrage erhält eine `requestId`.
* Antworten enthalten dieselbe `requestId`.
* Druckerbezogene Nachrichten enthalten immer eine `printerId`.
* Slotbezogene Nachrichten enthalten Drucker, AMS und Slot.
* Queue-Überläufe erkennen und protokollieren.
* ISR-Code verwendet ausschließlich `FromISR`-Funktionen.

---

# 7. Polling vermeiden

Nicht erlaubt:

```cpp
while (true) {
    if (flag) {
        // ...
    }

    delay(10);
}
```

Tasks sollen blockieren:

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

Zeitgesteuerte Abläufe werden umgesetzt mit:

* FreeRTOS-Software-Timern,
* `vTaskDelayUntil()`,
* LVGL-Timern,
* WiFi-Callbacks,
* MQTT-Callbacks,
* Hardwareinterrupts.

Zulässige periodische Funktionen:

* `lv_timer_handler()`,
* langsame Verbindungs-Health-Checks,
* Watchdog-Überwachung,
* Diagnosemessungen,
* notwendige Fallbacks für Hardware ohne Interrupt.

Busy Waiting ist verboten.

---

# 8. Interrupt-Regeln

Interrupts werden verwendet, wenn die Hardware dies sinnvoll unterstützt.

Bevorzugte Interruptquellen:

* HX711 DOUT,
* PN532 IRQ,
* Touch IRQ,
* SD Card Detect,
* Hardwaretaste.

Eine ISR darf nur:

* Interruptquelle quittieren,
* kleinen atomaren Zustand setzen,
* Task Notification senden,
* Queue-Nachricht über `FromISR` senden,
* Taskwechsel anfordern.

Eine ISR darf nicht:

* JSON verarbeiten,
* SD-Karte verwenden,
* I²C- oder SPI-Transaktionen durchführen,
* HTTP oder MQTT aufrufen,
* LVGL aufrufen,
* dynamischen Speicher anfordern,
* lange Berechnungen durchführen,
* umfangreich loggen.

---

# 9. Task-Konfiguration

Tasknamen, Stackgrößen, Prioritäten und Core-Affinitäten werden ausschließlich definiert in:

```text
src/config/TaskConfig.h
```

Beispiel:

```cpp
struct TaskSettings {
    const char* name;
    uint32_t stackSize;
    UBaseType_t priority;
    BaseType_t core;
};
```

Keine Taskparameter direkt in `main.cpp` verteilen.

Core-Pinning nur mit dokumentierter Begründung verwenden.

Zu dokumentieren:

* Stack High Water Mark,
* Tasklaufzeiten,
* Queue-Auslastung,
* Heap,
* PSRAM,
* Prioritätsprobleme.

---

# 10. Startablauf

`setup()` führt nur die grundlegende Initialisierung aus:

1. serielle Schnittstelle starten,
2. Chipinformationen ausgeben,
3. RTOS-Objekte erzeugen,
4. Queues erzeugen,
5. Event Group erzeugen,
6. Mutexes erzeugen,
7. Tasks starten,
8. Startstatus prüfen.

Die Arduino-`loop()`-Funktion enthält keine Anwendungslogik.

Sie darf dauerhaft blockieren oder den Arduino-Task kontrolliert freigeben.

Der StorageTask wird früh gestartet.

Der AppTask wartet auf die erforderlichen Ready-Events.

---

# 11. WiFiManager

Die WLAN-Verbindung wird über WiFiManager eingerichtet.

## 11.1 Erstinbetriebnahme

Wenn keine gültige WLAN-Konfiguration vorhanden ist:

1. NetworkTask startet WiFiManager.
2. Access Point wird erstellt.
3. Benutzer verbindet sich mit dem Access Point.
4. Captive Portal öffnet sich.
5. Benutzer wählt WLAN und Passwort.
6. Gerät verbindet sich.
7. AppTask erhält den Status.

## 11.2 Erneute Konfiguration

Die GUI bietet:

* Captive Portal starten,
* WLAN neu konfigurieren,
* WLAN-Zugangsdaten löschen,
* Hostname ändern,
* DHCP oder statische IP konfigurieren.

## 11.3 Task-Verhalten

WiFiManager darf im NetworkTask blockierend betrieben werden.

Andere Tasks müssen weiterhin funktionieren.

Es soll kein dauerhaftes `WiFiManager::process()` in der Arduino-`loop()`-Funktion verwendet werden.

## 11.4 WiFi-Callbacks

WiFi-Callbacks dürfen ausschließlich:

* kurze Events erzeugen,
* Event Bits setzen oder löschen,
* Nachrichten senden.

Keine GUI-, SD-, Spoolman- oder Bambu-Aufrufe direkt aus dem Callback.

---

# 12. SD-Karte und JSON-Speicherung

## 12.1 Grundregel

Alle vom Gerät erzeugten oder verwalteten persistenten Anwendungsdateien werden als gültige JSON-Dateien auf der SD-Karte gespeichert.

Dies betrifft:

* Gerätekonfiguration,
* Netzwerkparameter,
* Spoolman-Konfiguration,
* Bambu-Konfiguration,
* Waagenkalibrierung,
* UI-Einstellungen,
* NFC-Zuordnungen,
* Drucker-Slot-Zuordnungen,
* Cache,
* Warteschlangen,
* Diagnosedaten,
* Fehlerdaten,
* strukturierte Logs.

## 12.2 WLAN-Ausnahme

SSID und WLAN-Passwort werden von WiFiManager beziehungsweise vom ESP32-WiFi-Unterbau intern verwaltet.

Sie werden nicht zusätzlich unverschlüsselt als JSON auf SD gespeichert.

Zusätzliche Netzwerkparameter liegen in:

```text
/config/network.json
```

## 12.3 Verzeichnisstruktur

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
/cache/vendors.json
/cache/printers.json
/cache/ams-slots.json

/queue/pending-measurements.json
/queue/pending-actions.json

/mappings/nfc-spools.json
/mappings/bambu-tags.json
/mappings/printer-slots.json

/diagnostics/system.json
/diagnostics/last-error.json
/diagnostics/task-stats.json

/logs/events.json
```

## 12.4 JSON-Grundstruktur

Jede Datei enthält mindestens:

```json
{
  "schemaVersion": 1,
  "updatedAt": "2026-08-05T10:00:00Z"
}
```

Jede Datei besitzt:

* Schema-Version,
* Pflichtfelder,
* Standardwerte,
* maximale Dateigröße,
* Validierung,
* definierte Migrationsstrategie.

## 12.5 Atomisches Speichern

Ablauf:

1. neue Daten serialisieren,
2. in `.tmp.json` schreiben,
3. flushen,
4. schließen,
5. temporäre Datei erneut lesen,
6. JSON validieren,
7. bestehende Datei nach `.bak.json` umbenennen,
8. temporäre Datei zur Zieldatei umbenennen,
9. Backup nach Erfolg entfernen.

Beispiel:

```text
/config/scale.json
/config/scale.tmp.json
/config/scale.bak.json
```

## 12.6 Verhalten ohne SD-Karte

Ohne SD-Karte:

* deutliche Fehlermeldung anzeigen,
* keine Speicherung vortäuschen,
* keine Konfigurationsänderung als erfolgreich melden,
* keine Messung als dauerhaft gespeichert melden,
* eingeschränkten Diagnosebetrieb ermöglichen,
* keinen stillen Fallback auf eine zweite Datenbank verwenden.

---

# 13. Spoolman-Anbindung

## 13.1 Grundsatz

Spoolman ist die führende Datenbank.

Die Oberfläche verwendet Spoolman-Begriffe wie:

* Spulen-ID,
* Hersteller,
* Filament,
* Material,
* Farbe,
* Leergewicht,
* Anfangsgewicht,
* verbleibendes Gewicht,
* Standort,
* Kommentar.

## 13.2 Benötigte Funktionen

Mindestens:

```text
GET  /health
GET  /spool
GET  /spool/{id}
POST /spool
PUT  /spool/{id}
PUT  /spool/{id}/measure

GET  /filament
POST /filament

GET  /vendor
POST /vendor
```

Die tatsächliche API-Version muss geprüft werden.

Keine Endpunkte erfinden.

## 13.3 Messung

Das an Spoolman übertragene Gewicht ist das aktuelle Bruttogewicht aus:

```text
Spule + verbleibendes Filament
```

## 13.4 Cache

Cache-Dateien verbessern nur die Bedienbarkeit.

Sie sind keine führende Datenbank.

Veraltete Daten müssen als veraltet gekennzeichnet werden.

---

# 14. Mehrere Bambu-Drucker

## 14.1 Grundsatz

Die Architektur darf niemals von genau einem Drucker ausgehen.

Jeder Drucker besitzt eine stabile lokale ID:

```cpp
using PrinterId = uint16_t;
```

## 14.2 Konfiguration

`/config/bambu.json` enthält eine Liste:

```json
{
  "schemaVersion": 1,
  "selectedPrinterId": 1,
  "defaultPrinterId": 1,
  "printers": [
    {
      "id": 1,
      "name": "P1S Werkstatt",
      "host": "192.168.1.50",
      "serialNumber": "01P...",
      "accessCode": "12345678",
      "enabled": true
    },
    {
      "id": 2,
      "name": "X1C Labor",
      "host": "192.168.1.51",
      "serialNumber": "00M...",
      "accessCode": "87654321",
      "enabled": true
    }
  ]
}
```

## 14.3 Kein Security Key

Es gibt keinen zusätzlichen Security Key.

Nicht implementieren:

* Security-Key-Erzeugung,
* Security-Key-Eingabe,
* Security-Key-Reset,
* Verschlüsselung lokaler JSON-Dateien,
* verschlüsselte NFC-Payloads.

Der Bambu-LAN-Zugangscode darf lokal gespeichert werden.

Er darf nicht vollständig in Logs erscheinen.

## 14.4 Aktiver Drucker

Der aktive Drucker bestimmt:

* angezeigte AMS-Einheiten,
* Slots,
* External Slot,
* Druckerstatus,
* Slotoperationen,
* Slotzuweisungen.

Druckerwechsel benötigt keinen Neustart.

## 14.5 Datenmodell

```cpp
struct PrinterContext {
    PrinterId id;
    PrinterConnectionState connectionState;
    uint8_t activeAmsId;
    uint32_t lastUpdateMs;
};
```

```cpp
struct BambuCommand {
    BambuCommandType type;
    uint32_t requestId;
    PrinterId printerId;
    uint8_t amsId;
    uint8_t trayId;
    SpoolId spoolId;
};
```

```cpp
struct BambuEvent {
    BambuEventType type;
    uint32_t requestId;
    PrinterId printerId;
    BambuErrorCode error;
};
```

Die Implementierung darf zunächst nur eine aktive MQTT-Verbindung gleichzeitig verwenden, wenn RAM oder TLS dies erfordern.

Datenmodell und GUI müssen trotzdem von Anfang an mehrere Drucker unterstützen.

---

# 15. GUI-Grundlage

## 15.1 Referenz

Bedienoberfläche und Workflows orientieren sich funktional an SpoolEase.

Zu übernehmen sind:

* AMS-/External-Übersicht,
* Staging-Bereich,
* Druckerauswahl,
* Slotdetails,
* Spulendetails,
* Staging-Operationen,
* Slotoperationen,
* Quick Weight,
* Advanced Weight,
* Tag verknüpfen,
* Tag schreiben,
* Tag löschen,
* Tag trennen,
* Definition-Tag-Import,
* Bambu-Spulentyp-Auswahl,
* Legacy-Tag-Workflow,
* Status- und Fortschrittsdialoge.

Die GUI wird eigenständig mit EEZ Studio und LVGL umgesetzt.

Keine Slint-Dateien, Quellcodedateien oder Assets aus SpoolEase kopieren.

## 15.2 Integrierte Waage

Nicht übernehmen:

* externe Netzwerkwaage,
* Waagensuche im Netzwerk,
* Auswahl mehrerer Waagen.

Stattdessen:

* lokale HX711-Waage,
* Waagenstatus,
* aktuelles Gewicht,
* Stabilitätsstatus,
* Tarieren,
* Kalibrieren,
* Diagnose.

## 15.3 Permanente Druckeranzeige

Der aktuell gewählte Drucker ist auf allen normalen Screens sichtbar.

Beispiel:

```text
● P1S Werkstatt          AMS 1      ⚙
```

Offline:

```text
○ P1S Werkstatt          offline    ⚙
```

Kein Drucker:

```text
○ Kein Drucker ausgewählt           ⚙
```

Die Kopfzeile ist antippbar und öffnet die Druckerauswahl.

---

# 16. EEZ-Studio-Regeln

Das EEZ-Projekt liegt in:

```text
ui-project/FilamentStation.eez-project
```

Generierter Code liegt ausschließlich in:

```text
src/ui/generated/
```

Generierte Dateien dürfen nicht manuell verändert werden.

Eigene Actions und Bridges liegen außerhalb:

```text
src/ui/actions/
src/ui/UiBridge.cpp
src/ui/UiBridge.h
```

EEZ Flow wird nicht verwendet.

Die bereits bestehende GUI wird migriert.

Es darf kein zweites paralleles EEZ-Projekt erzeugt werden.

---

# 17. UI-Kommunikation

## 17.1 UiCommand

```cpp
enum class UiCommandType {
    ShowScreen,
    ShowDialog,
    ShowProgress,
    HideProgress,
    UpdateHeader,
    UpdatePrinterList,
    UpdateAmsOverview,
    UpdateStaging,
    UpdateTrayDetails,
    UpdateSpoolDetails,
    UpdateWeight,
    UpdateSettings,
    UpdateBootStatus,
    ShowToast
};
```

## 17.2 UiAction

```cpp
enum class UiActionType {
    SelectPrinter,
    SelectAms,
    SelectTray,
    SelectStaging,
    OpenSettings,

    Back,
    Cancel,
    Confirm,
    Close,

    QuickWeight,
    AdvancedWeight,

    ConfigureSlot,
    ConfigureSlotFromStaging,
    ResetSlot,
    UntagSlot,
    ReapplySlot,
    RefreshSlot,

    LinkTag,
    WriteTag,
    EraseTag,
    UnlinkTag,

    SearchSpool,
    SelectSpool,
    SaveMeasurement,

    OpenWifiSettings,
    OpenSpoolmanSettings,
    OpenScaleSettings,
    OpenPrinterSettings,
    OpenDeviceSettings,
    OpenDiagnostics,
    OpenFirmwareSettings,

    AddPrinter,
    EditPrinter,
    DeletePrinter,
    TestPrinterConnection,
    SetDefaultPrinter,

    TestSpoolmanConnection,
    SaveSpoolmanSettings,

    StartWifiPortal,
    ResetWifiCredentials,

    TareScale,
    StartScaleCalibration,
    ResetScaleCalibration
};
```

```cpp
struct UiAction {
    UiActionType type;
    uint32_t requestId;
    PrinterId printerId;
    SpoolId spoolId;
    uint8_t amsId;
    uint8_t trayId;
    int32_t value;
};
```

Die GUI führt keine Hardware-, Netzwerk- oder Dateisystemoperation direkt aus.

---

# 18. Standardlayout

Auflösung:

```text
480 × 320 Pixel
```

Standardaufteilung:

```text
TopPrinterBar:    40 px
Content:         224 px
BottomActionBar:  56 px
```

## 18.1 Touchflächen

* mindestens 48 Pixel hoch,
* bevorzugt 52 bis 56 Pixel,
* Abstand mindestens 4 Pixel,
* destruktive Aktionen räumlich trennen.

## 18.2 Typografie

* Titel: 22 bis 24 Pixel,
* normale Texte: 18 bis 20 Pixel,
* Status: 15 bis 17 Pixel,
* Gewicht: 30 bis 40 Pixel.

## 18.3 Farben

* verbunden: Grün,
* Verbindungsaufbau: Gelb oder Orange,
* Fehler: Rot,
* deaktiviert: Grau,
* primäre Navigation: Blau,
* Filamentkarten: Filamentfarbe,
* Textfarbe kontrastierend wählen.

---

# 19. Wiederverwendbare UI-Komponenten

Codex soll mindestens folgende Komponenten erzeugen:

```text
CMP_TOP_PRINTER_BAR
CMP_BOTTOM_ACTION_BAR
CMP_STATUS_BADGE
CMP_CONNECTION_INDICATOR
CMP_AMS_SELECTOR
CMP_TRAY_CARD
CMP_STAGING_CARD
CMP_SPOOL_SUMMARY
CMP_WEIGHT_DISPLAY
CMP_PROGRESS_OVERLAY
CMP_CONFIRM_DIALOG
CMP_RESULT_DIALOG
CMP_ERROR_DIALOG
CMP_NUMERIC_INPUT
CMP_TEXT_INPUT
CMP_SETTINGS_BUTTON
```

## 19.1 CMP_TOP_PRINTER_BAR

Enthält:

* Verbindungsindikator,
* Druckername,
* aktives AMS,
* Druckerauswahl,
* Settings-Icon.

Aktionen:

* Druckerbereich → Druckerauswahl,
* Settings-Icon → Settings.

## 19.2 CMP_TRAY_CARD

Zeigt:

* Slotnummer,
* Filamentfarbe,
* Material,
* Spoolman-ID,
* Gewicht,
* Zustand,
* Loaded-/In-Use-Symbol.

Zustände:

```cpp
enum class UiTrayState {
    Unknown,
    Empty,
    Reading,
    Ready,
    Loading,
    Loaded,
    Unloading,
    Error
};
```

## 19.3 CMP_STAGING_CARD

Zustände:

```cpp
enum class StagingState {
    Empty,
    TagDetected,
    SpoolLoaded,
    WeightReady,
    Assigned,
    Error
};
```

Zeigt:

* Spoolman-ID,
* Hersteller,
* Material,
* Farbe,
* Bruttogewicht,
* Restgewicht,
* NFC-Status.

## 19.4 CMP_WEIGHT_DISPLAY

Zeigt:

* Bruttogewicht,
* Nettogewicht,
* stabil/instabil,
* Waagenfehler,
* Kalibrierstatus.

---

# 20. Screenliste

```cpp
enum class UiScreenId {
    Boot,
    Home,
    PrinterSelect,

    StagingDetails,
    StagingActions,

    TrayDetails,
    TrayActions,
    TraySelect,

    SpoolSearch,
    SpoolDetails,

    QuickWeight,
    AdvancedWeight,
    WeightSummary,

    TagDetected,
    TagDefinitionImport,
    BambuSpoolType,
    TagActionSelect,
    TagSpoolSelect,
    TagReview,
    TagWrite,
    TagLegacy,
    TagResult,

    SettingsHome,
    SettingsWifi,
    SettingsSpoolman,
    SettingsScale,
    SettingsPrinters,
    SettingsPrinterEdit,
    SettingsDevice,
    SettingsDiagnostics,
    SettingsFirmware,

    WifiPortalInfo,
    Diagnostics,
    FirmwareUpdate
};
```

---

# 21. Screen-Beschreibungen

## 21.1 SCR_BOOT

Zweck:

* Systemstart,
* Initialisierung,
* Fehleranzeige.

Ohne Drucker-Kopfzeile.

Anzeige:

```text
FilamentStation
Version 0.x.x

✓ SD-Karte
✓ Display
✓ Waage
✓ NFC
◐ WLAN
○ Spoolman
○ Bambu
```

Bei Fehler:

* Fehlertext,
* erneut versuchen,
* Diagnose,
* Neustart.

## 21.2 SCR_HOME

Zentrale Hauptansicht.

Enthält:

* permanente Drucker-Kopfzeile,
* AMS-Auswahl,
* vier Slots des aktiven AMS,
* External Slot,
* Staging,
* Gewicht,
* NFC-Status,
* Spoolman-Status,
* WLAN-Status.

Anordnung:

```text
┌────────────────────────────────────────────┐
│ ● P1S Werkstatt       AMS 1           ⚙   │
├────────────────────────────────────────────┤
│ AMS 1  ●●●○       AMS 2  ●●○○              │
├────────────────────────────────────────────┤
│ Slot 1 │ Slot 2 │ Slot 3 │ Slot 4 │ Ext.  │
├────────────────────────────────────────────┤
│ Staging       │ Gewicht       │ Status     │
└────────────────────────────────────────────┘
```

Aktionen:

* Kopfzeile → Druckerauswahl,
* AMS → AMS wechseln,
* Slot → Slotdetails,
* Staging → Stagingdetails,
* Settings → Settings.

## 21.3 SCR_PRINTER_SELECT

Liste aller Drucker.

Pro Drucker:

* Name,
* online/offline,
* Standardmarkierung,
* aktuelle Markierung,
* Anzahl AMS.

Beispiel:

```text
✓ P1S Werkstatt
  verbunden · 1 AMS · Standard

  X1C Labor
  offline · 2 AMS
```

Aktionen:

* Drucker auswählen,
* Drucker verwalten,
* zurück.

## 21.4 SCR_STAGING_DETAILS

Zeigt:

* Spoolman-ID,
* Hersteller,
* Filament,
* Material,
* Farbe,
* Farbcode,
* Leergewicht,
* Bruttogewicht,
* Restgewicht,
* NFC-UID,
* Tagstatus.

Aktionen:

```text
Quick Weight
Mehr …
Schließen
```

## 21.5 SCR_STAGING_ACTIONS

Grid mit:

* Slot konfigurieren,
* Advanced Weight,
* Staging leeren,
* Tag schreiben,
* Tag verknüpfen,
* Tag trennen,
* Tag löschen,
* Spule suchen,
* Spulendetails.

Destruktive Aktionen benötigen Bestätigung.

## 21.6 SCR_TRAY_DETAILS

Titel:

```text
AMS 1 · Slot 3
```

Tabs:

```text
Slotinformationen
Spuleninformationen
```

Slotinformationen:

* Status,
* Material,
* Farbcode,
* Temperaturen,
* K-/PA-Wert,
* Loaded-Zustand,
* Verbrauch seit Laden,
* lokale Zuordnung.

Spuleninformationen:

* Spoolman-ID,
* Hersteller,
* Filament,
* Farbe,
* Leergewicht,
* Bruttogewicht,
* Restgewicht,
* letzter Messzeitpunkt.

Aktionen:

* Mehr,
* Aktualisieren,
* Schließen.

## 21.7 SCR_TRAY_ACTIONS

Aktionen:

* aus Staging konfigurieren,
* Spule manuell auswählen,
* Zuordnung entfernen,
* Slot zurücksetzen,
* Zuordnung erneut anwenden,
* Slot aktualisieren.

## 21.8 SCR_TRAY_SELECT

Home-ähnliche Ansicht im Auswahlmodus.

Anzeige:

```text
Zielslot für Spule 42 auswählen
```

Drucker und AMS dürfen gewechselt werden.

Auswählbare Slots werden hervorgehoben.

Nach Auswahl Zusammenfassung anzeigen.

## 21.9 SCR_SPOOL_SEARCH

Filter:

* ID,
* Freitext,
* Hersteller,
* Material,
* Farbe,
* nur nicht archivierte Spulen.

Ergebnis:

```text
ID 42 · Bambu Lab PLA Basic
Jade White · 642 g
```

Aktionen:

* auswählen,
* Filter löschen,
* aktualisieren,
* abbrechen.

## 21.10 SCR_SPOOL_DETAILS

Zeigt:

* Spulen-ID,
* Hersteller,
* Filament,
* Material,
* Farbe,
* Kommentar,
* Standort,
* Leergewicht,
* Anfangsgewicht,
* Bruttogewicht,
* Restgewicht,
* letzter Einsatz,
* NFC-Zuordnung,
* Slotzuordnung.

Aktionen abhängig vom Workflow:

* auswählen,
* wiegen,
* Tag schreiben,
* schließen.

## 21.11 SCR_QUICK_WEIGHT

Zeigt:

* Spule,
* großes aktuelles Gewicht,
* Stabilitätsstatus,
* berechnetes Restgewicht,
* letzte Messung.

Speichern erst bei stabilem Wert.

## 21.12 SCR_ADVANCED_WEIGHT

Auswahlmöglichkeiten:

* gebrauchte Spule – aktuelles Gewicht aktualisieren,
* volle/neue Spule – Ausgangsgewicht setzen,
* Leergewicht korrigieren,
* Ausgangsgewicht korrigieren oder löschen.

Danach Navigation zur Zusammenfassung.

## 21.13 SCR_WEIGHT_SUMMARY

Zeigt alle Änderungen:

```text
Spule 42

Bruttogewicht:          1247 g
Leergewicht:             250 g
Verbleibendes Filament:  997 g
```

Aktionen:

* zurück,
* abbrechen,
* speichern.

## 21.14 SCR_TAG_DETECTED

Kurze Anzeige:

```text
NFC-Tag erkannt
UID: 04:A2:...
```

Automatische Navigation abhängig vom Tagtyp.

## 21.15 SCR_TAG_DEFINITION_IMPORT

Für Bambu- oder andere Definition-Tags.

Zeigt:

* Hersteller,
* Material,
* Farbe,
* Farbcode,
* Temperaturen,
* UID,
* Hinweis, dass keine Spoolman-Zuordnung existiert.

Aktionen:

* ignorieren,
* mit bestehender Spule verbinden,
* importieren.

## 21.16 SCR_BAMBU_SPOOL_TYPE

Auswahl:

* Low Temperature, etwa 250 g,
* High Temperature, etwa 260 g,
* andere Spule, manuelle Eingabe.

Gewichte sind Vorschläge und müssen bearbeitbar sein.

## 21.17 SCR_TAG_ACTION_SELECT

Aktionen:

* mit vorhandener Spoolman-Spule verbinden,
* mit zuletzt verwendeter Spule verbinden,
* neue Spule importieren,
* Tag löschen,
* abbrechen.

## 21.18 SCR_TAG_SPOOL_SELECT

Verwendet Spool-Suche im Auswahlmodus.

## 21.19 SCR_TAG_REVIEW

Zeigt:

* Tagtyp,
* UID,
* Zielspule,
* Hersteller,
* Material,
* Farbe,
* Leergewicht,
* Gewicht,
* vorgesehene Aktion.

Aktionen:

* zurück,
* abbrechen,
* bestätigen.

## 21.20 SCR_TAG_WRITE

Progress-Anzeige:

```text
✓ Tag erkannt
✓ Speicher geprüft
◐ Daten werden geschrieben
○ Verifikation
```

Kein Security Key.

Keine Verschlüsselungsanzeige.

## 21.21 SCR_TAG_LEGACY

Für ältere Tagformate.

Aktionen:

* nach Spoolman importieren,
* mit bestehender Spule verbinden,
* in neues Format umschreiben,
* Tag löschen,
* abbrechen.

## 21.22 SCR_TAG_RESULT

Erfolg oder Fehler.

Beispiel:

```text
Tag erfolgreich mit Spule 42 verbunden
```

oder:

```text
Tag konnte nicht verifiziert werden
```

Aktionen:

* erneut versuchen,
* Details,
* schließen.

## 21.23 SCR_SETTINGS_HOME

Kategorien:

```text
WLAN
Spoolman
Waage
Bambu-Drucker
Gerät
Diagnose
Firmware
```

Keine Security-Key-Kategorie.

## 21.24 SCR_SETTINGS_WIFI

Zeigt:

* WLAN-Status,
* SSID,
* IP-Adresse,
* RSSI,
* Hostname,
* DHCP/statisch.

Aktionen:

* Captive Portal starten,
* WLAN neu konfigurieren,
* Zugangsdaten löschen,
* speichern,
* zurück.

## 21.25 SCR_SETTINGS_SPOOLMAN

Formular:

```text
Name
Protokoll HTTP/HTTPS
Host oder IP
Port
API-Basispfad
Timeout
```

Status:

* verbunden,
* nicht verbunden,
* Serverversion.

Aktionen:

* verwerfen,
* Verbindung testen,
* speichern.

Testen speichert nicht automatisch.

Kein Security-Key-Feld.

## 21.26 SCR_SETTINGS_SCALE

Zeigt:

* HX711-Status,
* Rohwert,
* Gewicht,
* Stabilität,
* Kalibrierstatus,
* Kalibrierfaktor.

Aktionen:

* tarieren,
* kalibrieren,
* Kalibrierung zurücksetzen,
* Diagnose,
* zurück.

## 21.27 SCR_SETTINGS_PRINTERS

Liste aller Drucker.

Pro Drucker:

* Name,
* IP/Host,
* online/offline,
* Standard,
* aktiv.

Aktionen:

* hinzufügen,
* auswählen,
* bearbeiten,
* zurück.

## 21.28 SCR_SETTINGS_PRINTER_EDIT

Felder:

* Anzeigename,
* Host oder IP,
* Seriennummer,
* LAN-Zugangscode,
* aktiviert,
* Standarddrucker.

Zugangscode maskiert anzeigen.

Aktionen:

* Verbindung testen,
* speichern,
* löschen,
* abbrechen.

Kein zusätzlicher Security Key.

## 21.29 SCR_SETTINGS_DEVICE

Felder:

* Gerätename,
* Sprache,
* Helligkeit,
* Display-Timeout.

Aktionen:

* Neustart,
* Werkseinstellungen,
* speichern,
* zurück.

## 21.30 SCR_SETTINGS_DIAGNOSTICS

Scrollbare Liste:

* SD-Karte,
* WLAN,
* Spoolman,
* Waage,
* NFC,
* aktiver Drucker,
* Bambu MQTT,
* Heap,
* PSRAM,
* Taskstatus,
* Queue-Auslastung,
* letzter Fehler.

## 21.31 SCR_SETTINGS_FIRMWARE

Zeigt:

* installierte Version,
* verfügbare Version,
* Updatekanal,
* Fortschritt,
* Ergebnis.

Aktionen:

* suchen,
* installieren,
* neu starten,
* zurück.

---

# 22. Verbindliche Workflows

## 22.1 Drucker auswählen

1. Drucker-Kopfzeile antippen.
2. Druckerliste anzeigen.
3. Drucker auswählen.
4. `selectedPrinterId` ändern.
5. BambuTask verbinden.
6. AMS-Daten laden.
7. Home aktualisieren.

## 22.2 Staging öffnen

1. Staging antippen.
2. Spulendetails anzeigen.
3. Quick Weight, Advanced Weight und weitere Aktionen anbieten.

## 22.3 Quick Weight

1. Spule im Staging.
2. stabiler Messwert vorhanden.
3. Quick Weight auswählen.
4. Zusammenfassung anzeigen.
5. bestätigen.
6. Spoolman aktualisieren.
7. Spule neu laden.
8. UI aktualisieren.

## 22.4 Advanced Weight

1. Spule auswählen.
2. Messwert stabilisieren.
3. Art der Aktualisierung auswählen.
4. optionale Werte korrigieren.
5. Zusammenfassung anzeigen.
6. Spoolman aktualisieren.
7. Ergebnis anzeigen.

## 22.5 Slot öffnen

1. Slot antippen.
2. Slotdetails anzeigen.
3. zwischen Slot- und Spuleninformationen wechseln.
4. „Mehr“ öffnet Slotoperationen.

## 22.6 Slot aus Staging konfigurieren

1. Spule im Staging.
2. Slot konfigurieren wählen.
3. Drucker auswählen.
4. AMS auswählen.
5. Slot auswählen.
6. Zusammenfassung anzeigen.
7. Bambu-Slot aktualisieren.
8. Slot neu laden.
9. Ergebnis anzeigen.

## 22.7 Neuer einfacher NFC-Tag

1. Tag erkennen.
2. keine Zuordnung gefunden.
3. Aktion auswählen.
4. Spoolman-Spule suchen oder auswählen.
5. Daten prüfen.
6. optional wiegen.
7. `spoolman:<id>` schreiben.
8. erneut lesen.
9. verifizieren.
10. Ergebnis anzeigen.

## 22.8 Bambu-Definition-Tag

1. Tag erkennen.
2. UID und Definition lesen.
3. Zuordnung prüfen.
4. Daten anzeigen.
5. Import oder Verknüpfung auswählen.
6. Bambu-Spulentyp auswählen.
7. Leergewicht prüfen.
8. passenden Hersteller und Filamentdatensatz suchen.
9. gegebenenfalls Datensätze anlegen.
10. Spule anlegen oder auswählen.
11. UID zu Spoolman-Spule zuordnen.
12. optional Gewicht messen.
13. Ergebnis anzeigen.

## 22.9 Legacy-Tag

1. altes Format erkennen.
2. Daten anzeigen.
3. importieren, verbinden, umschreiben oder löschen.
4. Ergebnis verifizieren.

## 22.10 Tag schreiben

1. Spule im Staging.
2. Tag schreiben auswählen.
3. beschreibbaren Tag auflegen.
4. `spoolman:<id>` schreiben.
5. erneut lesen.
6. verifizieren.
7. Ergebnis anzeigen.

## 22.11 Tag trennen

Entfernt nur lokale Tagzuordnungen.

Die Spule wird nicht aus Spoolman gelöscht.

## 22.12 Tag löschen

1. Bestätigung anzeigen.
2. NDEF-Daten löschen.
3. Tag erneut lesen.
4. Ergebnis anzeigen.

---

# 23. Projektstruktur

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
│   ├── ui.md
│   ├── workflows.md
│   ├── spoolman-api.md
│   └── decisions/
├── test/
│   ├── test_weight_filter/
│   ├── test_tag_payload/
│   ├── test_json_storage/
│   ├── test_spoolman_models/
│   ├── test_app_state/
│   └── test_ui_models/
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
    │   ├── Printer.h
    │   ├── Ams.h
    │   ├── AmsTray.h
    │   ├── Spool.h
    │   ├── Filament.h
    │   ├── Vendor.h
    │   ├── ScaleMeasurement.h
    │   ├── NfcTag.h
    │   └── UiModels.h
    └── ui/
        ├── UiBridge.cpp
        ├── UiBridge.h
        ├── actions/
        └── generated/
```

---

# 24. Fehlerbehandlung

Jeder Fehler besitzt:

* Quelle,
* Fehlercode,
* Schweregrad,
* `requestId`,
* optional `printerId`,
* verständliche Meldung.

```cpp
struct ErrorEvent {
    ErrorSource source;
    ErrorCode code;
    ErrorSeverity severity;
    uint32_t requestId;
    PrinterId printerId;
    char message[128];
};
```

Tasks dürfen bei normalen Fehlern nicht selbst neu starten.

Ein kontrollierter Neustart wird nur durch den AppTask ausgelöst.

---

# 25. Logging

Logging-Stufen:

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

Beispiele:

```text
[ScaleTask] Stable weight: 1247.2 g
[NfcTask] Tag detected
[SpoolmanTask] Loading spool 42
[BambuTask] Printer 2 connected
[AppTask] Ready -> ReadingTag
```

Nicht protokollieren:

* WLAN-Passwort,
* vollständiger Bambu-Zugangscode,
* geheime Token,
* private Schlüssel.

---

# 26. Tests

Hardwareunabhängig zu testen:

* Gewichtsmittelwert,
* Tiefpassfilter,
* Stabilitätserkennung,
* Kalibrierberechnung,
* NFC-Payload-Parser,
* Legacy-Payload-Parser,
* JSON-Schema-Validierung,
* Schema-Migration,
* atomisches Speichern,
* Backup-Wiederherstellung,
* Spoolman-JSON-Parser,
* Mehrdrucker-Datenmodell,
* Druckerwechsel,
* Zustandsautomat,
* verspätete Antworten,
* falsche `requestId`,
* falsche `printerId`,
* doppelte Aktionen,
* Queue-Überlauf.

Integrationstests:

* Interrupt → Task Notification,
* Task → App-Queue,
* AppTask → Command-Queue,
* mehrere Storage-Anfragen,
* WLAN-Ausfall während HTTP,
* Druckerwechsel während MQTT,
* SD-Entfernung während Schreiben,
* NFC-Tag-Entfernung während Workflow.

---

# 27. Coding-Regeln

* C++17 verwenden.
* Englische Klassen- und Methodennamen.
* Kleine Klassen.
* Komposition bevorzugen.
* Keine monolithischen Dateien.
* Keine unnötige Vererbung.
* Keine langen `delay()`-Aufrufe.
* Keine Busy-Wait-Schleifen.
* Keine Hardwarezugriffe aus AppTask oder UI.
* Keine LVGL-Aufrufe außerhalb des UiTask.
* Keine SD-Zugriffe außerhalb des StorageTask.
* Keine langen Arbeiten in ISRs.
* Fehler nicht ignorieren.
* Gewichte intern als `float` in Gramm.
* Konstanten mit Einheit benennen.
* Keine Warnungen neu einführen.
* Dynamische Speicherverwendung begrenzen.
* Queue-Nachrichten klein halten.

---

# 28. Regeln für Codex

Vor jeder Änderung:

1. `AGENTS.md` vollständig lesen.
2. `TASKS.md` vollständig lesen.
3. Repository analysieren.
4. aktuelle Phase bestimmen.
5. bereits vorhandene Implementierung berücksichtigen.
6. kurzen Umsetzungsplan erstellen.

Während der Umsetzung:

* nur angeforderte Aufgaben bearbeiten,
* vorhandenes EEZ-Projekt migrieren,
* kein zweites UI-Projekt erstellen,
* keine SpoolEase-Dateien kopieren,
* keine Security-Key-Funktion erzeugen,
* generierten Code nicht manuell verändern,
* GUI-Aktionen nur über UiAction senden,
* mehrere Drucker berücksichtigen,
* keine GPIOs erfinden,
* keine Zugangsdaten eintragen,
* keine erfolgreichen Hardwaretests behaupten,
* Änderungen klein und überprüfbar halten.

Nach jedem Arbeitsschritt:

1. EEZ-Code neu generieren, falls GUI geändert wurde.
2. `pio run` ausführen.
3. verfügbare Tests ausführen.
4. Compilerwarnungen prüfen.
5. geänderte Dateien auflisten.
6. nur erledigte Checkboxen abhaken.
7. offene Entscheidungen dokumentieren.
8. nächsten sinnvollen Arbeitsschritt nennen.

Ein Screen gilt erst als fertig, wenn:

1. er im vorhandenen EEZ-Projekt existiert,
2. generierter Code kompiliert,
3. Navigation funktioniert,
4. Actions an den AppTask gesendet werden,
5. keine Hardware direkt aufgerufen wird,
6. Mehrdrucker-IDs korrekt verarbeitet werden.
