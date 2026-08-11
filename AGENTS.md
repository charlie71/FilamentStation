# AGENTS.md – FilamentStation

## 1. Projektziel

Entwickle eine eigenständige Filamentverwaltungsstation für 3D-Druck-Filamentrollen.

Das Gerät kombiniert:

* eine Filamentwaage,
* eine Touch-Bedienoberfläche,
* einen NFC/RFID-Leser,
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
* NFC/RFID-zu-Spoolman-Zuordnungen,
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
* PN532 NFC/RFID-Leser
* NFC/RFID-Tags entsprechend Abschnitt 2.3
* optional Buzzer
* optional Status-LED
* optional Hardwaretaste
* optional SD-Card-Detect-Eingang

## 2.3 Unterstützte NFC/RFID-Tags

Die NFC/RFID-Unterstützung wird nicht auf ein einzelnes Tagformat fest verdrahtet.

Hardware-Technologie und logisches Datenformat sind getrennt zu behandeln.

### 2.3.1 Native FilamentStation-Tags

Unterstützte Chips:

* NTAG213
* NTAG215 – bevorzugter Standard
* NTAG216

Verwendung:

* NDEF
* lesen
* schreiben
* löschen
* verifizieren

Standard-Payload:

```text
spoolman:<spool_id>
```

Beispiel:

```text
spoolman:42
```

Der Tag enthält bewusst nur einen Verweis auf die Spoolman-Spule.

Nicht auf dem Tag duplizieren:

* Hersteller,
* Material,
* Farbe,
* Leergewicht,
* Restgewicht,
* Temperaturen.

Spoolman bleibt die führende Datenquelle.

NTAG215 ist der bevorzugte Standard für neu gekaufte FilamentStation-Tags.

NTAG213 und NTAG216 müssen ebenfalls unterstützt werden.

### 2.3.2 Originale Bambu-Lab-RFID-Tags

Originale Bambu-Lab-Tags müssen unterstützt werden.

Ziele:

* Tagtyp erkennen,
* UID lesen,
* vorhandene Filamentinformationen auslesen, sofern technisch und rechtlich möglich,
* Definition-Daten in ein neutrales `TagDefinition`-Modell umwandeln,
* Daten mit Spoolman abgleichen,
* vorhandene Spule zuordnen,
* neue Spoolman-Datensätze über den Importworkflow erzeugen.

Wichtige Regel:

> Originale Bambu-Lab-Tags werden niemals beschrieben oder verändert.

Die originale Bambu-RFID-Struktur darf nicht durch FilamentStation überschrieben werden.

Die Verbindung zwischen einem originalen Bambu-Tag und einer Spoolman-Spule wird lokal gespeichert:

```text
/mappings/bambu-tags.json
```

Beispiel:

```json
{
  "schemaVersion": 1,
  "mappings": [
    {
      "uid": "A1:B2:C3:D4",
      "spoolId": 42
    }
  ]
}
```

Wenn bereits eine bekannte UID-Zuordnung existiert, darf diese direkt zur Spoolman-Spule führen.

### 2.3.3 OpenPrintTag

OpenPrintTag soll lesend unterstützt werden.

Ziele:

* Format sicher erkennen,
* vorhandene standardisierte Felder auslesen,
* Daten in `TagDefinition` umwandeln,
* passende Spoolman-Daten suchen,
* Import nach Spoolman anbieten.

Schreibunterstützung gehört nicht zur ersten Version.

Codex darf Feldnamen, Datenlayout oder Tagstruktur nicht erfinden.

Vor Implementierung muss die tatsächlich verwendete öffentliche Spezifikation beziehungsweise eine belastbare Primärquelle geprüft und in der Projektdokumentation referenziert werden.

### 2.3.4 OpenTag3D

OpenTag3D soll lesend unterstützt werden.

Ziele:

* Format sicher erkennen,
* Daten parsen,
* in `TagDefinition` umwandeln,
* passenden Spoolman-Datensatz finden,
* Import nach Spoolman ermöglichen.

Schreibunterstützung gehört nicht zur ersten Version.

Auch hier gilt:

Keine Formatdetails erfinden.

Die Implementierung muss sich auf die tatsächlich geprüfte Spezifikation stützen.

### 2.3.5 Legacy-Tags

Unterstützte ältere, bekannte Formate können über eigene Parser migriert werden.

Mögliche Aktionen:

* Daten anzeigen,
* mit vorhandener Spoolman-Spule verbinden,
* Daten nach Spoolman importieren,
* auf einen neuen FilamentStation-NTAG migrieren,
* beschreibbaren Legacy-Tag in FilamentStation-NDEF-Format umschreiben.

Es wird kein Security-Key-System aus SpoolEase übernommen.

Verschlüsselte Legacy-Formate, die einen solchen Security Key voraussetzen, sind nicht automatisch Teil des Projektumfangs.

Sie dürfen nur implementiert werden, wenn dies ausdrücklich später beschlossen wird.

### 2.3.6 Unbekannte Tags

Ein unbekannter Tag darf niemals automatisch verändert werden.

FilamentStation soll mindestens anzeigen:

* erkannte Tagtechnologie,
* UID,
* NDEF vorhanden/nicht vorhanden,
* lesbar/nicht lesbar,
* beschreibbar/nicht beschreibbar, falls sicher ermittelbar.

Optional kann die UID manuell mit einer vorhandenen Spoolman-Spule verbunden werden.

Diese Zuordnung wird lokal gespeichert.

### 2.3.7 Generische MIFARE-Classic-Tags

Bei unbekannten MIFARE-Classic-Tags gilt zunächst:

* UID lesen,
* Technologie anzeigen,
* keine unbekannten Speicherblöcke verändern,
* optional UID-basierte Spoolman-Zuordnung.

Nur ein explizit implementierter und verifizierter Parser darf zusätzliche Daten interpretieren.

---

# 3. GPIO-Regel

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

# 4. Software-Stack

## 4.1 Entwicklungsumgebung

* Visual Studio Code
* PlatformIO
* Arduino Framework für ESP32
* FreeRTOS aus dem Arduino-ESP32-/ESP-IDF-Unterbau
* C++17
* Git
* Codex in Visual Studio Code

## 4.2 Benutzeroberfläche

* LVGL 9.x
* LovyanGFX
* EEZ Studio
* EEZ Studio ohne EEZ Flow
* Anwendungslogik in C++

## 4.3 Weitere Bibliotheken

Voraussichtlich erforderlich:

* ArduinoJson 7
* WiFiManager
* HX711-Bibliothek oder eigener kleiner HX711-Treiber
* PN532-Bibliothek
* HTTPClient oder vergleichbarer ESP32-HTTP-Client
* MQTT-Client mit TLS-Unterstützung
* SD oder SD_MMC
* Preferences nur für systembedingte Daten, die nicht sinnvoll auf SD liegen

Alle Bibliotheken müssen in `platformio.ini` mit festen, geprüften Versionen eingetragen werden.

Keine unnötigen Bibliotheken hinzufügen.

---

# 5. Bedeutung von „Prozess“

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

# 6. Vorgeschriebene Task-Architektur

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

## 6.1 UiTask

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

## 6.2 AppTask

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

## 6.3 ScaleTask

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

## 6.4 NfcTask

Verantwortlich für:

* PN532 initialisieren,
* Tags erkennen,
* Tagtechnologie bestimmen,
* UID lesen,
* NDEF lesen,
* native FilamentStation-Tags schreiben,
* beschreibbare Tags löschen,
* geschriebene Daten verifizieren,
* Tagformat klassifizieren,
* Parser auswählen,
* Bambu-Tags erkennen,
* OpenPrintTag erkennen,
* OpenTag3D erkennen,
* Legacy-Tags erkennen,
* unbekannte Tags melden,
* normalisierte Tagdaten an den AppTask senden.

Der PN532-IRQ-Pin soll verwendet werden, sofern Hardware und Schnittstelle dies unterstützen.

Die ISR führt keine I²C-, SPI- oder NFC-Kommunikation aus.

## 6.5 StorageTask

Verantwortlich für:

* SD-Karte initialisieren,
* Verzeichnisstruktur anlegen,
* JSON-Dateien lesen,
* JSON-Dateien validieren,
* JSON-Dateien atomar speichern,
* Backups verwalten,
* Cache verwalten,
* ausstehende Messungen speichern,
* Tag-Mappings speichern,
* Diagnoseinformationen speichern.

Nur der StorageTask darf direkt auf die SD-Karte zugreifen.

## 6.6 NetworkTask

Verantwortlich für:

* WiFiManager,
* WLAN-Erstkonfiguration,
* Captive Portal,
* WLAN-Wiederverbindung,
* WiFi-Events,
* Netzwerkstatus,
* erneute WLAN-Konfiguration.

Der NetworkTask führt keine Spoolman- oder Bambu-Fachlogik aus.

## 6.7 SpoolmanTask

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
* normalisierte `TagDefinition`-Daten auf Spoolman-Datenmodelle abbilden,
* Antworten parsen,
* Timeouts und HTTP-Fehler behandeln.

HTTP-Aufrufe dürfen UiTask und AppTask nicht blockieren.

## 6.8 BambuTask

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

# 7. NFC/RFID-Abstraktionsschicht

## 7.1 Trennung von Technologie und Format

Die physische Tagtechnologie und das Datenformat müssen getrennt modelliert werden.

Beispiel:

```cpp
enum class TagTechnology {
    Unknown,
    Ntag213,
    Ntag215,
    Ntag216,
    MifareClassic1K,
    MifareClassic4K,
    OtherIso14443A
};
```

```cpp
enum class TagFormat {
    Unknown,
    EmptyNdef,
    FilamentStation,
    BambuLab,
    OpenPrintTag,
    OpenTag3D,
    Legacy
};
```

## 7.2 Normalisiertes Ergebnis

Alle Parser liefern ein gemeinsames Modell.

Beispiel:

```cpp
struct TagDefinition {
    TagFormat format;

    bool hasSpoolId;
    uint32_t spoolId;

    char vendor[48];
    char material[32];
    char colorName[48];
    char colorCode[12];

    float nominalFilamentWeightG;
    float emptySpoolWeightG;

    int16_t nozzleTempMinC;
    int16_t nozzleTempMaxC;

    char sourceDescription[64];
};
```

Nur tatsächlich bekannte Werte werden gesetzt.

Nicht vorhandene Informationen dürfen nicht erfunden werden.

## 7.3 TagReadResult

Beispiel:

```cpp
struct TagReadResult {
    TagTechnology technology;
    TagFormat format;

    uint8_t uid[10];
    uint8_t uidLength;

    bool ndefPresent;
    bool writable;
    bool knownFormat;

    TagDefinition definition;
};
```

## 7.4 Parser-Interface

Tagformate werden über austauschbare Parser umgesetzt.

Beispiel:

```cpp
class ITagParser {
public:
    virtual ~ITagParser() = default;

    virtual TagFormat format() const = 0;
    virtual bool canParse(const RawTagData& tag) const = 0;
    virtual TagParseResult parse(
        const RawTagData& tag,
        TagDefinition& result
    ) const = 0;
};
```

Vorgesehene Parser:

```text
FilamentStationTagParser
BambuLabTagParser
OpenPrintTagParser
OpenTag3DParser
LegacyTagParser
```

Der unbekannte Tagfall ist kein Parser, der Fantasiedaten erzeugt.

## 7.5 Parser-Reihenfolge

Die Erkennung muss deterministisch sein.

Eine mögliche Reihenfolge:

1. bekannte native FilamentStation-NDEF-Payload,
2. eindeutig erkannter Bambu-Lab-Tag,
3. OpenPrintTag,
4. OpenTag3D,
5. bekannte Legacy-Formate,
6. leerer NDEF-Tag,
7. unbekannter Tag.

Die tatsächliche Erkennungslogik muss pro Format dokumentiert werden.

Keine heuristische Erkennung verwenden, die fremde Tags fälschlich als bekanntes Format behandeln könnte.

## 7.6 Schreibrechte

Schreiboperationen werden separat vom Parser behandelt.

Native FilamentStation-Tags:

* lesen: ja,
* schreiben: ja,
* löschen: ja.

Originale Bambu-Tags:

* lesen: ja,
* schreiben: nein,
* löschen: nein.

OpenPrintTag:

* lesen: ja,
* schreiben: vorerst nein.

OpenTag3D:

* lesen: ja,
* schreiben: vorerst nein.

Legacy:

* abhängig vom konkreten Format,
* nur schreiben, wenn sicher dokumentiert und ausdrücklich unterstützt.

Unbekannt:

* keine automatische Schreiboperation.

---

# 8. FreeRTOS-Kommunikation

## 8.1 Zentrale App-Event-Queue

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
    NfcTagClassified,
    NfcTagWritten,
    NfcTagErased,
    NfcTagVerified,
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

## 8.2 Command-Queues

```cpp
QueueHandle_t uiCommandQueue;
QueueHandle_t scaleCommandQueue;
QueueHandle_t nfcCommandQueue;
QueueHandle_t storageCommandQueue;
QueueHandle_t networkCommandQueue;
QueueHandle_t spoolmanCommandQueue;
QueueHandle_t bambuCommandQueue;
```

## 8.3 Event Group

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

Event Groups übertragen nur Zustände.

## 8.4 Task Notifications

Vorgesehene Anwendungen:

* HX711-DOUT-ISR → ScaleTask
* PN532-IRQ-ISR → NfcTask
* Touch-IRQ → UiTask, falls sinnvoll
* SD-Card-Detect-ISR → StorageTask
* Hardwaretaste → zuständiger Task

## 8.5 Mutexes

Mögliche gemeinsame Ressourcen:

* gemeinsamer I²C-Bus,
* gemeinsamer SPI-Bus,
* zentrale Logging-Ausgabe.

Da nur StorageTask auf SD zugreift, ist dort normalerweise kein zusätzlicher Dateisystem-Mutex notwendig.

## 8.6 Nachrichtenregeln

* Keine Zeiger auf lokale Stackvariablen über Queues.
* Kleine Strukturen als Wert übertragen.
* Keine großen Arduino-`String`-Objekte in Queues.
* Begrenzte Zeichenfelder verwenden.
* Jede asynchrone Anfrage erhält `requestId`.
* Antworten enthalten dieselbe `requestId`.
* Druckerbezogene Nachrichten enthalten `printerId`.
* Slotbezogene Nachrichten enthalten Drucker, AMS und Slot.
* Queue-Überläufe erkennen und protokollieren.
* ISR-Code ausschließlich mit `FromISR`-Funktionen.

---

# 9. Polling vermeiden

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

Zeitgesteuerte Abläufe:

* FreeRTOS-Software-Timer,
* `vTaskDelayUntil()`,
* LVGL-Timer,
* WiFi-Callbacks,
* MQTT-Callbacks,
* Hardwareinterrupts.

Zulässige periodische Funktionen:

* `lv_timer_handler()`,
* langsame Health-Checks,
* Watchdog,
* Diagnose,
* notwendige Hardware-Fallbacks.

Busy Waiting ist verboten.

---

# 10. Interrupt-Regeln

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
* SD verwenden,
* I²C/SPI-Transaktionen durchführen,
* HTTP/MQTT aufrufen,
* LVGL aufrufen,
* NFC-Daten parsen,
* dynamischen Speicher anfordern,
* lange rechnen,
* umfangreich loggen.

---

# 11. Task-Konfiguration

Tasknamen, Stackgrößen, Prioritäten und Core-Affinitäten ausschließlich in:

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

Core-Pinning nur mit dokumentierter Begründung.

Zu dokumentieren:

* Stack High Water Mark,
* Tasklaufzeiten,
* Queue-Auslastung,
* Heap,
* PSRAM,
* Prioritätsprobleme.

---

# 12. Startablauf

`setup()`:

1. serielle Schnittstelle starten,
2. Chipinformationen ausgeben,
3. RTOS-Objekte erzeugen,
4. Queues erzeugen,
5. Event Group erzeugen,
6. Mutexes erzeugen,
7. Tasks starten,
8. Startstatus prüfen.

Die Arduino-`loop()`-Funktion enthält keine Anwendungslogik.

StorageTask wird früh gestartet.

AppTask wartet auf notwendige Ready-Events.

---

# 13. WiFiManager

WLAN wird über WiFiManager eingerichtet.

## 13.1 Erstinbetriebnahme
Wenn keine gültige WLAN-Konfiguration vorhanden ist:

1. NetworkTask startet WiFiManager.
2. Access Point wird erstellt.
3. Benutzer verbindet sich.
4. Captive Portal öffnet sich.
5. WLAN und Passwort wählen.
6. Gerät verbindet sich.
7. AppTask erhält Status.

## 13.2 Erneute Konfiguration

GUI:

* Captive Portal starten,
* WLAN neu konfigurieren,
* WLAN-Zugangsdaten löschen,
* Hostname ändern,
* DHCP/statische IP.

## 13.3 Task-Verhalten

WiFiManager darf im NetworkTask blockierend laufen.

Andere Tasks müssen weiter funktionieren.

Kein dauerhaftes `WiFiManager::process()` in `loop()`.

## 13.4 WiFi-Callbacks

Callbacks dürfen nur:

* kurze Events erzeugen,
* Event Bits ändern,
* Nachrichten senden.

Keine direkten GUI-, SD-, Spoolman- oder Bambu-Aufrufe.

---

# 14. SD-Karte und JSON

## 14.1 Grundregel

Alle persistenten Anwendungsdateien sind gültige JSON-Dateien auf SD.

Ausnahme:

SSID und WLAN-Passwort werden durch WiFiManager/ESP32 intern verwaltet.

## 14.2 Verzeichnisstruktur

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
/mappings/open-tags.json
/mappings/printer-slots.json

/diagnostics/system.json
/diagnostics/last-error.json
/diagnostics/task-stats.json

/logs/events.json
```

## 14.3 JSON-Grundstruktur

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
* Migrationsstrategie.

## 14.4 Atomisches Speichern

1. `.tmp.json` schreiben,
2. flushen,
3. schließen,
4. erneut validieren,
5. bestehende Datei nach `.bak.json`,
6. temporäre Datei umbenennen,
7. Backup entfernen.

## 14.5 Verhalten ohne SD

* Fehler anzeigen,
* Speichern nicht vortäuschen,
* keine Messung als dauerhaft gespeichert melden,
* eingeschränkten Diagnosebetrieb ermöglichen,
* keinen stillen Datenbank-Fallback verwenden.

---

# 15. Spoolman-Anbindung

Spoolman ist führende Datenbank.
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
Benötigte Funktionen mindestens:

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

## 15.1 TagDefinition → Spoolman

Importierte Tagdaten werden zunächst in `TagDefinition` normalisiert.

Danach übernimmt ausschließlich die Spoolman-Schicht die Zuordnung zu:

* Vendor,
* Filament,
* Spool.

Der NFC-Parser darf keine HTTP-Anfragen durchführen.

## 15.2 Importstrategie

Bei importierten Definitionen:

1. Hersteller suchen.
2. Filament suchen.
3. vorhandene passende Daten anzeigen.
4. Benutzerentscheidung ermöglichen, wenn mehrere Treffer existieren.
5. fehlenden Hersteller nur bei Bedarf anlegen.
6. fehlendes Filament nur bei Bedarf anlegen.
7. Spule anlegen oder bestehende Spule auswählen.
8. Tag-Mapping speichern.

Keine unkontrollierten Dubletten erzeugen.

---

# 16. Mehrere Bambu-Drucker

Jeder Drucker besitzt:

```cpp
using PrinterId = uint16_t;
```

Konfiguration:

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
    }
  ]
}
```

Kein zusätzlicher Security Key.

Druckerwechsel benötigt keinen Neustart.

Alle Druckerkommandos und -events enthalten `printerId`.

---

# 17. GUI-Grundlage

Bedienoberfläche und Workflows orientieren sich funktional an SpoolEase.

Zu übernehmen:

* AMS-/External-Übersicht,
* Staging,
* Druckerauswahl,
* Slotdetails,
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
* Legacy-Workflow,
* Status- und Fortschrittsdialoge.

Zusätzlich:

* OpenPrintTag-Import,
* OpenTag3D-Import,
* unbekannter-Tag-Workflow.

Keine Slint-Dateien oder SpoolEase-Assets kopieren.

---

# 18. Keine separaten Spool-Screens

Separate Hauptscreens `SCR_SPOOL_SEARCH` und `SCR_SPOOL_DETAILS` sind nicht erforderlich.

Spulenauswahl wird als wiederverwendbare Workflow-Komponente umgesetzt:

```text
CMP_SPOOL_PICKER
CMP_SPOOL_INFO
```

`CMP_SPOOL_PICKER` kann verwendet werden in:

* Tag verknüpfen,
* unbekannten Tag zuordnen,
* Slot manuell konfigurieren,
* importierten Tag mit bestehender Spule verbinden.

Der Picker darf als:

* Overlay,
* Dialog,
* eingebettete Liste,
* Wizard-Schritt

umgesetzt werden.

Er ist kein eigener Hauptnavigationsscreen.

---

# 19. Integrierte Waage

Nicht übernehmen:

* externe Netzwerkwaage,
* Waagensuche,
* mehrere Netzwerkwaagen.

Stattdessen:

* lokale HX711-Waage,
* aktuelles Gewicht,
* Stabilität,
* Tarieren,
* Kalibrieren,
* Diagnose.

---

# 20. Permanente Druckeranzeige

Auf normalen Haupt- und Workflow-Screens:

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

Kopfzeile antippbar → Druckerauswahl.

---

# 21. EEZ-Studio-Regeln

EEZ-Projekt:

```text
ui-project/FilamentStation.eez-project
```

Generierter Code:

```text
src/ui/generated/
```

Generierte Dateien nicht manuell ändern.

Eigene Actions:

```text
src/ui/actions/
src/ui/UiBridge.cpp
src/ui/UiBridge.h
```

Kein EEZ Flow.

Kein zweites paralleles EEZ-Projekt.

---

# 22. UI-Kommunikation

## 22.1 UiCommand

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
    UpdateWeight,
    UpdateTagInfo,
    UpdateSettings,
    UpdateBootStatus,
    ShowToast
};
```

## 22.2 UiAction

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
    AssignUnknownTag,

    SelectSpool,
    SaveMeasurement,

    ImportTagDefinition,
    IgnoreTagDefinition,
    RewriteLegacyTag,

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

Druckerbezogene Aktionen enthalten `printerId`.

Slotaktionen enthalten:

* `printerId`,
* `amsId`,
* `trayId`.

---

# 23. Standardlayout

```text
480 × 320 Pixel
```

Standard:

```text
TopPrinterBar:    40 px
Content:         224 px
BottomActionBar:  56 px
```

Touchflächen:

* mindestens 48 px,
* bevorzugt 52–56 px.

Typografie:

* Titel 22–24 px,
* Text 18–20 px,
* Status 15–17 px,
* Gewicht 30–40 px.

---

# 24. Wiederverwendbare Komponenten

Mindestens:

```text
CMP_TOP_PRINTER_BAR
CMP_BOTTOM_ACTION_BAR
CMP_STATUS_BADGE
CMP_CONNECTION_INDICATOR
CMP_AMS_SELECTOR
CMP_TRAY_CARD
CMP_STAGING_CARD
CMP_SPOOL_SUMMARY
CMP_SPOOL_PICKER
CMP_SPOOL_INFO
CMP_WEIGHT_DISPLAY
CMP_TAG_SUMMARY
CMP_PROGRESS_OVERLAY
CMP_CONFIRM_DIALOG
CMP_RESULT_DIALOG
CMP_ERROR_DIALOG
CMP_NUMERIC_INPUT
CMP_TEXT_INPUT
CMP_SETTINGS_BUTTON
```

---

# 25. Screenliste

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

    QuickWeight,
    AdvancedWeight,
    WeightSummary,

    TagDetected,
    TagActionSelect,
    TagDefinitionImport,
    BambuSpoolType,
    TagReview,
    TagWrite,
    TagLegacy,
    TagUnknown,
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

# 26. Screen-Beschreibungen

## 26.1 SCR_BOOT

Systeminitialisierung.

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

## 26.2 SCR_HOME

Enthält:

* Drucker-Kopfzeile,
* AMS-Auswahl,
* vier AMS-Slots,
* External,
* Staging,
* Gewicht,
* NFC,
* Spoolman,
* WLAN.

## 26.3 SCR_PRINTER_SELECT

Liste:

* Name,
* Status,
* Standardmarkierung,
* aktuelle Auswahl,
* AMS-Anzahl.

## 26.4 SCR_STAGING_DETAILS

Zeigt:

* Spoolman-ID,
* Hersteller,
* Filament,
* Material,
* Farbe,
* Leergewicht,
* Bruttogewicht,
* Restgewicht,
* NFC/RFID-Tagtyp,
* UID.

Aktionen:

* Quick Weight,
* Mehr,
* Schließen.

## 26.5 SCR_STAGING_ACTIONS

Aktionen:

* Slot konfigurieren,
* Advanced Weight,
* Staging leeren,
* Tag schreiben,
* Tag verknüpfen,
* Tag trennen,
* Tag löschen,
* Spule auswählen.

## 26.6 SCR_TRAY_DETAILS

Tabs:

* Slotinformationen,
* Spuleninformationen.

Spuleninformationen werden über `CMP_SPOOL_INFO` dargestellt.

## 26.7 SCR_TRAY_ACTIONS

* aus Staging konfigurieren,
* Spule manuell auswählen,
* Zuordnung entfernen,
* Slot zurücksetzen,
* erneut anwenden,
* aktualisieren.

## 26.8 SCR_TRAY_SELECT

Home-ähnliche Zielslot-Auswahl.

Drucker und AMS dürfen gewechselt werden.

## 26.9 SCR_QUICK_WEIGHT

* aktuelle Spule,
* Gewicht,
* Stabilität,
* Restgewicht,
* letzte Messung.

## 26.10 SCR_ADVANCED_WEIGHT

* gebrauchte Spule,
* volle/neue Spule,
* Leergewicht korrigieren,
* Ausgangsgewicht korrigieren.

## 26.11 SCR_WEIGHT_SUMMARY

Zusammenfassung vor Schreiben nach Spoolman.

## 26.12 SCR_TAG_DETECTED

Zeigt:

* UID,
* Technologie,
* erkanntes Format.

Danach automatische Navigation.

## 26.13 SCR_TAG_ACTION_SELECT

Für leere oder neue native Tags.

Aktionen:

* mit vorhandener Spule verbinden,
* zuletzt verwendete Spule,
* Spule über Picker auswählen,
* Tag löschen,
* abbrechen.

## 26.14 SCR_TAG_DEFINITION_IMPORT

Wird verwendet für:

* Bambu Lab,
* OpenPrintTag,
* OpenTag3D,
* andere unterstützte Definition-Tags.

Zeigt:

* Format,
* Hersteller,
* Material,
* Farbe,
* Farbcode,
* Temperaturen,
* Gewichte,
* UID,
* erkannte Felder.

Aktionen:

* ignorieren,
* mit bestehender Spule verbinden,
* nach Spoolman importieren.

## 26.15 SCR_BAMBU_SPOOL_TYPE

Nur wenn für Bambu-Import notwendig.

Auswahl:

* Low Temperature,
* High Temperature,
* Other.

Gewichtsvorschläge sind editierbar.

## 26.16 SCR_TAG_REVIEW

Zeigt:

* Tagformat,
* UID,
* Spoolman-Zielspule,
* erkannte Definition,
* Mapping,
* vorgesehene Aktion.

## 26.17 SCR_TAG_WRITE

Nur für beschreibbare, unterstützte Tags.

Native FilamentStation-Tags:

```text
spoolman:<id>
```

Originale Bambu-Tags dürfen diesen Screen nicht zum Schreiben verwenden.

## 26.18 SCR_TAG_LEGACY

Zeigt erkanntes Legacy-Format.

Aktionen abhängig von Fähigkeiten:

* importieren,
* verbinden,
* auf natives Format migrieren,
* löschen,
* abbrechen.

Keine Security-Key-Funktion.

## 26.19 SCR_TAG_UNKNOWN

Zeigt:

```text
Unbekannter NFC/RFID-Tag

Technologie: MIFARE Classic 1K
UID: A1:B2:C3:D4
NDEF: nein
```

Aktionen:

* UID einer Spoolman-Spule zuordnen,
* erneut lesen,
* ignorieren,
* schließen.

Keine automatische Schreiboperation.

## 26.20 SCR_TAG_RESULT

Zeigt Erfolg oder Fehler einer Tagoperation.

## 26.21 Settings

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

Kein Security Key.

---

# 27. Verbindliche Tag-Workflows

## 27.1 Native FilamentStation-Spule

```text
NTAG
  ↓
spoolman:<id>
  ↓
Spoolman laden
  ↓
Staging
  ↓
Wiegen
  ↓
optional AMS-Zuordnung
```

## 27.2 Neuer leerer NTAG

```text
Tag erkannt
  ↓
leer/unzugeordnet
  ↓
Spoolman-Spule über CMP_SPOOL_PICKER auswählen
  ↓
Review
  ↓
spoolman:<id> schreiben
  ↓
erneut lesen
  ↓
verifizieren
  ↓
Staging
```

## 27.3 Originaler Bambu-Tag

```text
Bambu Tag
  ↓
UID + Definition lesen
  ↓
bambu-tags.json prüfen
  ├─ Mapping vorhanden → Spoolman laden
  └─ kein Mapping
       ↓
       Definition anzeigen
       ↓
       vorhandene Spule auswählen
       ODER
       nach Spoolman importieren
       ↓
       UID-Mapping speichern
  ↓
Staging
  ↓
Wiegen
  ↓
optional AMS-Zuordnung
```

Der Bambu-Tag wird niemals verändert.

## 27.4 OpenPrintTag

```text
OpenPrintTag
  ↓
Format parsen
  ↓
TagDefinition
  ↓
Spoolman-Match suchen
  ├─ passende Spule → verbinden
  └─ kein Treffer → Import anbieten
  ↓
lokales Mapping falls erforderlich
  ↓
Staging
```

## 27.5 OpenTag3D

Gleicher Grundablauf wie OpenPrintTag.

## 27.6 Legacy-Tag

```text
Legacy
  ↓
bekanntes Format parsen
  ↓
Daten anzeigen
  ↓
importieren / verbinden / migrieren / löschen
```

## 27.7 Unbekannter Tag

```text
Unknown
  ↓
UID + Technologie anzeigen
  ↓
optional UID mit Spoolman-Spule verbinden
```

Keine Datenblöcke verändern.

---

# 28. Projektstruktur

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
│   ├── nfc-tags.md
│   ├── spoolman-api.md
│   └── decisions/
├── test/
│   ├── test_weight_filter/
│   ├── test_tag_payload/
│   ├── test_tag_parsers/
│   ├── test_json_storage/
│   ├── test_spoolman_models/
│   ├── test_app_state/
│   └── test_ui_models/
├── ui-project/
│   └── FilamentStation.eez-project
└── src/
    ├── main.cpp
    ├── app/
    ├── config/
    ├── rtos/
    ├── tasks/
    ├── drivers/
    ├── services/
    ├── models/
    │   ├── Printer.h
    │   ├── Ams.h
    │   ├── AmsTray.h
    │   ├── Spool.h
    │   ├── Filament.h
    │   ├── Vendor.h
    │   ├── ScaleMeasurement.h
    │   ├── NfcTag.h
    │   ├── TagDefinition.h
    │   ├── TagReadResult.h
    │   └── UiModels.h
    ├── nfc/
    │   ├── ITagParser.h
    │   ├── TagParserRegistry.cpp
    │   ├── TagParserRegistry.h
    │   ├── FilamentStationTagParser.cpp
    │   ├── BambuLabTagParser.cpp
    │   ├── OpenPrintTagParser.cpp
    │   ├── OpenTag3DParser.cpp
    │   └── LegacyTagParser.cpp
    └── ui/
        ├── UiBridge.cpp
        ├── UiBridge.h
        ├── actions/
        └── generated/
```

---

# 29. Fehlerbehandlung

Jeder Fehler besitzt:

* Quelle,
* Fehlercode,
* Schweregrad,
* `requestId`,
* optional `printerId`,
* verständliche Meldung.

Normale Hardware- oder Kommunikationsfehler führen nicht automatisch zum Neustart.

---

# 30. Logging

Stufen:

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

Beispiele:

```text
[NfcTask] Technology: NTAG215
[NfcTask] Tag format: FilamentStation
[NfcTask] Tag format: BambuLab
[NfcTask] Unknown tag UID detected
[SpoolmanTask] Loading spool 42
```

Nicht loggen:

* WLAN-Passwort,
* vollständigen Bambu-Zugangscode,
* Tokens,
* Schlüssel.

---

# 31. Tests

Zusätzlich zu bestehenden Tests:

* FilamentStation-Payload gültig,
* ungültige native Payload,
* ungültige Spool-ID,
* Tagformat-Erkennung,
* Parser-Reihenfolge,
* unbekannter Tag,
* leerer NDEF-Tag,
* Bambu-Parser-Testdaten,
* OpenPrintTag-Testdaten,
* OpenTag3D-Testdaten,
* Legacy-Parser,
* Parser darf unbekannte Felder nicht erfinden,
* originaler Bambu-Tag darf nie einen Schreibbefehl auslösen,
* UID-Mapping,
* Mapping-Konflikt,
* doppeltes Mapping.

Testvektoren für externe Formate müssen aus dokumentierten Quellen oder realen, anonymisierten Testtags stammen.

Keine erfundenen Testdaten als Beweis für Formatkompatibilität verwenden.

---

# 32. Coding-Regeln

* C++17.
* Englische Klassen- und Methodennamen.
* Kleine Klassen.
* Komposition bevorzugen.
* Keine monolithischen Dateien.
* Keine langen `delay()`.
* Kein Busy Waiting.
* Keine Hardwarezugriffe aus AppTask oder UI.
* Keine LVGL-Aufrufe außerhalb UiTask.
* Keine SD-Zugriffe außerhalb StorageTask.
* Keine langen ISRs.
* Keine Formatdetails erfinden.
* Externe Tagformate über eigene Parser kapseln.
* Parser führen keine Netzwerk- oder Dateisystemzugriffe aus.
* Gewichte intern `float` in Gramm.
* Queue-Nachrichten klein halten.

---

# 33. Regeln für Codex

Vor jeder Änderung:

1. `AGENTS.md` vollständig lesen.
2. `TASKS.md` vollständig lesen.
3. Repository analysieren.
4. aktuellen Implementierungsstand berücksichtigen.
5. erledigte Aufgaben nicht unnötig neu implementieren.
6. kurzen Umsetzungsplan erstellen.

Aktueller Migrationsgrundsatz:

> Die bestehende Implementierung bis einschließlich Aufgabe 5.3 ist die Ausgangsbasis.

Neue Anforderungen ab 5.4 dürfen bestehende funktionierende Hardware-, RTOS-, Waagen-, GUI- und PN532-Funktionalität nicht unnötig ersetzen.

Bei NFC/RFID-Arbeiten:

* vorhandenen NfcTask weiterverwenden,
* Hardwaretreiber nicht neu schreiben, wenn nicht notwendig,
* Parser von Hardwarezugriff trennen,
* externe Formatspezifikationen vor Implementierung prüfen,
* keine Formatdetails erfinden,
* Bambu-Tags niemals beschreiben,
* OpenPrintTag/OpenTag3D in Version 1 nur lesen,
* unbekannte Tags niemals automatisch verändern,
* native FilamentStation-Tags als NTAG/NDEF mit `spoolman:<id>` verwenden.

Bei GUI-Arbeiten:

* vorhandenes EEZ-Projekt weiterverwenden,
* kein zweites UI-Projekt,
* generierten Code nicht manuell ändern,
* `CMP_SPOOL_PICKER` statt eigener Spool-Suchhauptscreens verwenden,
* Tagformat und Tagtechnologie im Workflow sichtbar machen,
* keine Security-Key-Funktion erzeugen.

Nach jedem Arbeitsschritt:

1. EEZ-Code neu generieren, falls nötig.
2. `pio run`.
3. Tests ausführen.
4. Compilerwarnungen prüfen.
5. geänderte Dateien auflisten.
6. nur tatsächlich erledigte Checkboxen abhaken.
7. offene Annahmen dokumentieren.
8. nächsten offenen Arbeitsschritt nennen.
