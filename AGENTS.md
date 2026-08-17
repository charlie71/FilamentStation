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

---

# 2.3 Unterstützte NFC/RFID-Tags

Die NFC/RFID-Unterstützung wird nicht auf ein einzelnes Tagformat fest verdrahtet.

Hardware-Technologie und logisches Datenformat sind getrennt zu behandeln.

## 2.3.1 Native FilamentStation-Tags

Unterstützte Chips:

* NTAG213
* NTAG215 – bevorzugter Standard
* NTAG216

Verwendung:

* NDEF lesen
* NDEF schreiben
* FilamentStation-Payload löschen
* Verifikation nach Schreiboperation

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

## 2.3.2 Originale Bambu-Lab-RFID-Tags

Originale Bambu-Lab-Tags müssen unterstützt werden.

Ziele:

* Tagtyp erkennen,
* UID lesen,
* vorhandene Filamentinformationen auslesen,
* Definition-Daten in ein neutrales `TagDefinition`-Modell umwandeln,
* Daten mit Spoolman abgleichen,
* vorhandene Spule zuordnen,
* neue Spoolman-Datensätze über den Importworkflow erzeugen.

Wichtige Regel:

> Originale Bambu-Lab-Tags werden niemals beschrieben oder verändert.

Die Verbindung zwischen Bambu-Tag und Spoolman-Spule wird lokal gespeichert:

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

## 2.3.3 OpenPrintTag

OpenPrintTag wird lesend unterstützt.

Ziele:

* Format erkennen,
* standardisierte Felder auslesen,
* Daten in `TagDefinition` umwandeln,
* passende Spoolman-Daten suchen,
* Zuordnung beziehungsweise Import nach Spoolman ermöglichen.

In Version 1 wird OpenPrintTag nicht verändert.

## 2.3.4 OpenTag3D

OpenTag3D wird lesend unterstützt.

Ziele:

* Format erkennen,
* Daten parsen,
* `TagDefinition` erzeugen,
* passenden Spoolman-Datensatz suchen,
* Zuordnung beziehungsweise Import ermöglichen.

In Version 1 wird OpenTag3D nicht verändert.

## 2.3.5 Legacy-Tags

Bekannte Legacy-Formate können über eigene Parser unterstützt werden.

Mögliche Funktionen:

* Daten anzeigen,
* Spoolman-Spule zuordnen,
* nach Spoolman importieren,
* einen sicheren beschreibbaren Tag auf FilamentStation-Format migrieren.

Es gibt kein Security-Key-System.

## 2.3.6 Unbekannte Tags

Unbekannte Tags dürfen niemals automatisch verändert werden.

Angezeigt werden mindestens:

* Tagtechnologie,
* UID,
* NDEF vorhanden/nicht vorhanden,
* lesbar/nicht lesbar,
* beschreibbar/nicht beschreibbar, falls sicher bestimmbar.

Eine UID kann einer Spoolman-Spule zugeordnet werden.

## 2.3.7 Generische MIFARE-Classic-Tags

Bei unbekannten MIFARE-Classic-Tags:

* UID lesen,
* Technologie anzeigen,
* unbekannte Speicherblöcke nicht verändern,
* UID-basierte Spoolman-Zuordnung erlauben.

---

# 3. Benutzerkonzept für NFC/RFID-Zuordnungen

## 3.1 Grundprinzip

Die Benutzeroberfläche unterscheidet **nicht** zwischen:

* Tag schreiben,
* Tag verknüpfen,
* Tag löschen,
* Tag trennen.

Diese Begriffe beschreiben technische Implementierungsdetails und sind für den Benutzer unnötig kompliziert.

Es gibt nur zwei Benutzerfunktionen:

```text
Tag zuordnen
Tag-Zuordnung entfernen
```

---

# 3.2 Tag zuordnen

Die Funktion **„Tag zuordnen“** verbindet den aktuell erkannten NFC/RFID-Tag mit einer Spoolman-Spule.

Dabei gilt immer:

1. UID des Tags wird ermittelt.
2. Spoolman-Spule wird ausgewählt oder aus dem Tag ermittelt.
3. lokales UID-zu-Spoolman-Mapping wird angelegt.
4. falls der Tag sicher mit dem FilamentStation-Payload beschrieben werden darf:

   * `spoolman:<id>` schreiben,
   * erneut lesen,
   * Payload verifizieren.
5. Ergebnis anzeigen.

Der Benutzer entscheidet **nicht**, ob nur ein Mapping oder zusätzlich ein Schreibvorgang durchgeführt wird.

Das System entscheidet dies anhand der Tag-Fähigkeiten.

---

# 3.3 Tag-Zuordnung entfernen

Die Funktion **„Tag-Zuordnung entfernen“** entfernt die Verbindung zwischen Tag und Spoolman-Spule.

Dabei gilt:

1. lokales UID-Mapping entfernen.
2. prüfen, ob ein von FilamentStation verwalteter Payload sicher entfernt werden darf.
3. wenn ja:

   * eigenen FilamentStation-Payload löschen,
   * Tag erneut lesen,
   * Löschung verifizieren.
4. fremde oder originale Taginformationen niemals verändern.
5. Ergebnis anzeigen.

Der Benutzer entscheidet **nicht** zwischen:

* „trennen“
* „löschen“.

---

# 3.4 Capability-gesteuertes Verhalten

Technisch besitzt jeder erkannte Tag Fähigkeiten.

Beispiel:

```cpp
struct TagCapabilities {
    bool canAssociateByUid;
    bool canWriteFilamentStationPayload;
    bool canClearFilamentStationPayload;
    bool preserveOriginalContent;
};
```

Alternativ darf eine enum-basierte Policy verwendet werden.

Wichtig ist die semantische Trennung.

## Native FilamentStation NTAG213/215/216

Zuordnen:

```text
UID-Mapping
+
spoolman:<id> schreiben
+
verifizieren
```

Zuordnung entfernen:

```text
UID-Mapping entfernen
+
eigenen FilamentStation-Payload löschen
+
verifizieren
```

## Originaler Bambu-Tag

Zuordnen:

```text
UID-Mapping
```

Originalinhalt bleibt unverändert.

Zuordnung entfernen:

```text
UID-Mapping entfernen
```

Originalinhalt bleibt unverändert.

## OpenPrintTag

Zuordnen:

```text
UID-Mapping
```

Originalinhalt bleibt unverändert.

Zuordnung entfernen:

```text
UID-Mapping entfernen
```

Originalinhalt bleibt unverändert.

## OpenTag3D

Gleich wie OpenPrintTag.

## Unknown

Zuordnen:

```text
UID-Mapping
```

Keine automatische Änderung am Tag.

Zuordnung entfernen:

```text
UID-Mapping entfernen
```

## Legacy

Ein Legacy-Parser darf nur dann zusätzlich schreiben oder löschen, wenn er ausdrücklich bestätigt:

```text
safeToRewriteAsFilamentStation = true
```

Andernfalls nur UID-Mapping.

---

# 3.5 Ergebnisanzeige

Der Benutzer soll sehen, **was tatsächlich passiert ist**, aber nicht vorher technische Details auswählen müssen.

Beispiele:

```text
Tag erfolgreich zugeordnet und beschrieben.
```

oder:

```text
Tag erfolgreich zugeordnet.
Originaler Taginhalt wurde nicht verändert.
```

Beim Entfernen:

```text
Tag-Zuordnung entfernt.
Taginhalt wurde ebenfalls entfernt.
```

oder:

```text
Tag-Zuordnung entfernt.
Originaler Taginhalt blieb unverändert.
```

---

# 4. GPIO-Regel

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
* Interruptfähigkeit.

---

# 5. Software-Stack

## 5.1 Entwicklungsumgebung

* Visual Studio Code
* PlatformIO
* Arduino Framework für ESP32
* FreeRTOS aus Arduino-ESP32/ESP-IDF
* C++17
* Git
* Codex

## 5.2 Benutzeroberfläche

* LVGL 9.x
* LovyanGFX
* EEZ Studio
* EEZ Studio ohne EEZ Flow
* Anwendungslogik in C++

## 5.3 Weitere Bibliotheken

Voraussichtlich:

* ArduinoJson 7
* WiFiManager
* HX711
* PN532
* HTTPClient
* MQTT-Client mit TLS
* SD oder SD_MMC

Alle Bibliotheksversionen werden festgelegt.

---

# 6. FreeRTOS-Task-Architektur

Mindestens:

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

* LVGL,
* EEZ-GUI,
* Touch,
* UI-Kommandos,
* UI-Aktionen,
* Screens,
* Dialoge.

Nur UiTask darf LVGL verwenden.

## 6.2 AppTask

Verantwortlich für:

* Zustandsautomat,
* Workflows,
* Task-Koordination,
* `requestId`,
* `printerId`,
* Tag-Zuordnungsworkflow,
* Fehlerbehandlung.

Keine direkten Hardwarezugriffe.

## 6.3 ScaleTask

Verantwortlich für:

* HX711,
* Messwerte,
* Tarierung,
* Kalibrierung,
* Filter,
* Stabilität.

HX711-DOUT soll wenn möglich per Interrupt den Task wecken.

## 6.4 NfcTask

Verantwortlich für:

* PN532,
* UID,
* NDEF,
* Tagtechnologie,
* Parser,
* Tagklassifikation,
* technische Schreib-/Löschoperationen,
* Verifikation.

Wichtig:

Der NfcTask kennt technische Kommandos wie:

```text
ReadTag
WriteFilamentStationPayload
ClearFilamentStationPayload
VerifyTag
```

Diese technischen Operationen sind **keine Benutzeraktionen**.

## 6.5 StorageTask

Nur StorageTask greift auf SD zu.

Verantwortlich für:

* Konfiguration,
* JSON,
* Cache,
* Tag-Mappings,
* Backups,
* Pending Actions,
* Diagnose.

## 6.6 NetworkTask

Verantwortlich für WiFiManager und WLAN.

## 6.7 SpoolmanTask

Verantwortlich für Spoolman-REST-Kommunikation.

## 6.8 BambuTask

Verantwortlich für mehrere Bambu-Drucker und AMS.

---

# 7. NFC/RFID-Abstraktion

## 7.1 TagTechnology

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

## 7.2 TagFormat

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

## 7.3 TagCapabilities

Jeder TagReadResult muss eine Capability-Information bereitstellen.

Beispiel:

```cpp
struct TagCapabilities {
    bool canAssociateByUid;
    bool canWriteFilamentStationPayload;
    bool canClearFilamentStationPayload;
    bool preserveOriginalContent;
};
```

`writable` alleine reicht nicht aus.

Ein physisch beschreibbarer Tag darf nicht automatisch überschrieben werden.

Entscheidend ist:

```text
Darf FilamentStation diesen Tag sicher verändern?
```

## 7.4 TagDefinition

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

## 7.5 TagReadResult

```cpp
struct TagReadResult {
    TagTechnology technology;
    TagFormat format;

    uint8_t uid[10];
    uint8_t uidLength;

    bool ndefPresent;
    bool knownFormat;

    TagCapabilities capabilities;
    TagDefinition definition;
};
```

---

# 8. Parser-Architektur

```text
FilamentStationTagParser
BambuLabTagParser
OpenPrintTagParser
OpenTag3DParser
LegacyTagParser
```

Parser:

* kennen keine GUI,
* kennen kein Spoolman,
* greifen nicht auf SD zu,
* führen keine Schreiboperationen aus,
* interpretieren nur Rohdaten.

Parser-Reihenfolge deterministisch.

Keine unbekannten Daten erfinden.

---

# 9. FreeRTOS-Kommunikation

Zentrale App-Queue:

```cpp
QueueHandle_t appEventQueue;
```

Command-Queues:

```cpp
QueueHandle_t uiCommandQueue;
QueueHandle_t scaleCommandQueue;
QueueHandle_t nfcCommandQueue;
QueueHandle_t storageCommandQueue;
QueueHandle_t networkCommandQueue;
QueueHandle_t spoolmanCommandQueue;
QueueHandle_t bambuCommandQueue;
```

Event Group für globale Zustände.

Task Notifications bevorzugt für:

* HX711 IRQ,
* PN532 IRQ,
* Touch IRQ,
* SD Detect.

---

# 10. UI-Aktionen

Die Benutzeraktionen müssen semantisch und nicht technisch benannt werden.

## 10.1 Erlaubte Tag-Aktionen

```cpp
AssignTag
RemoveTagAssignment
```

## 10.2 Nicht mehr als Benutzeraktion verwenden

Folgende alten `UiActionType`-Werte sollen entfernt oder intern migriert werden:

```cpp
LinkTag
WriteTag
EraseTag
UnlinkTag
AssignUnknownTag
RewriteLegacyTag
```

Technische NfcCommands dürfen weiterhin existieren.

## 10.3 UiActionType

Zielstruktur:

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

    AssignTag,
    RemoveTagAssignment,

    SelectSpool,
    SaveMeasurement,

    ImportTagDefinition,
    IgnoreTagDefinition,

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

---

# 11. Interner TagAssignment-Workflow

Der AppTask orchestriert die Zuordnung.

Pseudologik:

```text
AssignTag(spoolId)
    ↓
TagReadResult prüfen
    ↓
UID-Mapping über StorageTask speichern
    ↓
capabilities.canWriteFilamentStationPayload ?
    ├─ nein → Erfolg: MappingOnly
    └─ ja
         ↓
         NfcTask: WriteFilamentStationPayload
         ↓
         NfcTask: VerifyTag
         ↓
         Erfolg: MappingAndTagWritten
```

Fehler beim Schreiben:

* Mapping darf nicht unbemerkt verloren gehen.
* Benutzer erhält klare Meldung.

Beispiel:

```text
Spule wurde zugeordnet.
Tag konnte jedoch nicht beschrieben werden.
```

Optional darf der AppTask bei fehlgeschlagenem Schreiben das Mapping zurückrollen, wenn dies als definierte Policy festgelegt wird.

Die gewählte Policy muss dokumentiert werden.

Empfehlung:

> Mapping bleibt bestehen, wenn das Schreiben fehlschlägt.

---

# 12. Interner RemoveTagAssignment-Workflow

```text
RemoveTagAssignment
    ↓
TagReadResult prüfen
    ↓
UID-Mapping entfernen
    ↓
capabilities.canClearFilamentStationPayload ?
    ├─ nein → Erfolg: MappingRemoved
    └─ ja
         ↓
         NfcTask: ClearFilamentStationPayload
         ↓
         VerifyTag
         ↓
         Erfolg: MappingAndPayloadRemoved
```

Originale Inhalte fremder Tags dürfen nie gelöscht werden.

---

# 13. Polling und Interrupts

Keine Busy-Wait-Schleifen.

Tasks warten auf:

* Queue,
* Notification,
* Event Group,
* Timer.

ISRs dürfen keine:

* I²C-Kommunikation,
* SPI-Kommunikation,
* JSON-Verarbeitung,
* LVGL-Aufrufe,
* HTTP-/MQTT-Aufrufe

durchführen.

---

# 14. WiFiManager

WLAN über WiFiManager.

Captive Portal im NetworkTask.

Andere Tasks bleiben aktiv.

SSID und WLAN-Passwort werden nicht zusätzlich auf SD gespeichert.

---

# 15. SD und JSON

Persistente Anwendungsdaten als JSON auf SD.

Verzeichnisse:

```text
/config/
/cache/
/queue/
/mappings/
/diagnostics/
/logs/
```

Tag-Mappings:

```text
/mappings/nfc-spools.json
/mappings/bambu-tags.json
/mappings/open-tags.json
/mappings/printer-slots.json
```

Alle Mapping-Dateien werden ausschließlich durch StorageTask gelesen und geschrieben.

---

# 16. Spoolman

Spoolman bleibt führende Datenbank.

Importierte Tagdaten werden über `TagDefinition` normalisiert.

Parser führen keine HTTP-Kommunikation aus.

SpoolmanTask übernimmt:

* Vendor-Matching,
* Filament-Matching,
* Spool-Matching,
* Import,
* Gewichtsupdate.

---

# 17. Mehrere Bambu-Drucker

Jeder Druckerbefehl enthält `printerId`.

Druckerwechsel ohne Neustart.

Aktiver Drucker permanent in GUI sichtbar.

Kein Security Key.

Originale Bambu-RFID-Tags bleiben read-only.

---

# 18. GUI-Grundlage

GUI orientiert sich funktional an SpoolEase.

Übernommen werden:

* AMS-/External-Übersicht,
* Staging,
* Druckerauswahl,
* Slotdetails,
* Quick Weight,
* Advanced Weight,
* Definition-Tag-Import,
* Bambu-Spulentyp,
* Legacy-Workflow,
* Fortschrittsdialoge.

Nicht übernehmen:

* getrennte Benutzerfunktionen für Tag schreiben/verknüpfen,
* getrennte Benutzerfunktionen für Tag löschen/trennen,
* Security Key,
* externe Waagenverwaltung.

---

# 19. Keine separaten Spool-Screens

Spulenauswahl über:

```text
CMP_SPOOL_PICKER
CMP_SPOOL_INFO
```

Kein eigener Hauptscreen erforderlich.

---

# 20. Permanente Druckeranzeige

Beispiel:

```text
● P1S Werkstatt          AMS 1      ⚙
```

---

# 21. EEZ-Studio-Regeln

Bestehendes Projekt weiterverwenden:

```text
ui-project/FilamentStation.eez-project
```

Generierter Code:

```text
src/ui/generated/
```

Nicht manuell ändern.

---

# 22. Wiederverwendbare Komponenten

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

# 23. Screenliste

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
    TagAssign,
    TagDefinitionImport,
    BambuSpoolType,
    TagReview,
    TagOperation,
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

Alte Screens wie `TagWrite` dürfen beim Migrieren intern vorübergehend weiterverwendet werden, sollen jedoch nicht mehr als „Tag schreiben“ in der Benutzeroberfläche erscheinen.

---

# 24. Screen-Beschreibungen

## SCR_HOME

Enthält:

* Drucker,
* AMS,
* Slots,
* External,
* Staging,
* Gewicht,
* NFC,
* Spoolman,
* WLAN.

## SCR_STAGING_DETAILS

Zeigt:

* Spoolman-ID,
* Hersteller,
* Material,
* Farbe,
* Gewicht,
* NFC/RFID-Typ,
* UID,
* Zuordnungsstatus.

Aktionen:

```text
Quick Weight
Mehr …
Schließen
```

## SCR_STAGING_ACTIONS

Benutzeraktionen:

```text
Slot konfigurieren
Advanced Weight
Staging leeren
Tag zuordnen
Tag-Zuordnung entfernen
Spule auswählen
```

Nicht anzeigen:

```text
Tag schreiben
Tag verknüpfen
Tag trennen
Tag löschen
```

## SCR_TAG_ASSIGN

Zweck:

Tag einer Spoolman-Spule zuordnen.

Zeigt:

* Tagtyp,
* UID,
* vorhandene Zuordnung,
* gewählte Spule,
* Tag-Fähigkeiten in verständlicher Form.

Nicht technische Optionen anbieten.

Beispiel:

```text
NFC-Tag zuordnen

Tag: NTAG215
UID: 04:A2:...

Spule:
#42 Bambu PLA Basic – Jade White

[Abbrechen] [Zuordnen]
```

Bei Bambu:

```text
Der originale Bambu-Tag bleibt unverändert.
```

Bei NTAG:

```text
Die Zuordnung wird zusätzlich auf dem Tag gespeichert.
```

Der Benutzer muss dies nicht auswählen.

## SCR_TAG_REVIEW

Zeigt vor der Zuordnung:

* Tag,
* UID,
* Spule,
* Spoolman-ID,
* erkannte Definition,
* erwartetes Verhalten.

Beispiel NTAG:

```text
Zuordnung wird lokal gespeichert.
Der Tag wird zusätzlich beschrieben.
```

Beispiel Bambu:

```text
Zuordnung wird lokal gespeichert.
Der originale Bambu-Tag bleibt unverändert.
```

## SCR_TAG_OPERATION

Generischer Progress-Screen.

Nicht:

```text
Tag wird geschrieben
```

als generischer Workflowname.

Stattdessen:

```text
Tag wird zugeordnet …
```

Interne Schritte dürfen angezeigt werden:

```text
✓ Tag erkannt
✓ Zuordnung gespeichert
◐ Tag wird aktualisiert
○ Verifikation
```

Bei Mapping-only:

```text
✓ Tag erkannt
✓ Zuordnung gespeichert
✓ Originalinhalt unverändert
```

Beim Entfernen:

```text
Tag-Zuordnung wird entfernt …
```

## SCR_TAG_RESULT

Beispiele:

```text
Tag erfolgreich zugeordnet und beschrieben.
```

```text
Tag erfolgreich zugeordnet.
Originalinhalt wurde nicht verändert.
```

```text
Tag-Zuordnung erfolgreich entfernt.
```

```text
Tag-Zuordnung entfernt.
FilamentStation-Daten wurden ebenfalls vom Tag entfernt.
```

## SCR_TAG_UNKNOWN

Aktionen:

```text
Tag zuordnen
Erneut lesen
Schließen
```

Nicht:

```text
Tag schreiben
```

## SCR_TAG_LEGACY

Aktionen:

* Definition importieren,
* Tag zuordnen,
* Tag-Zuordnung entfernen, falls vorhanden,
* Abbrechen.

Eine Migration auf FilamentStation-Payload kann intern Teil von „Tag zuordnen“ sein, wenn der Parser dies sicher erlaubt.

---

# 25. Zuordnungsstatus

Die GUI soll einen klaren Zustand anzeigen.

Beispiele:

```text
Nicht zugeordnet
```

```text
Zugeordnet zu Spule #42
```

Optional:

```text
Zugeordnet + auf Tag gespeichert
```

oder:

```text
Zugeordnet · Originaltag unverändert
```

---

# 26. Verbindliche NFC/RFID-Workflows

## 26.1 Native FilamentStation-Spule lesen

```text
NTAG
 ↓
spoolman:<id>
 ↓
Spoolman laden
 ↓
Staging
```

## 26.2 Leeren NTAG zuordnen

```text
Tag erkannt
 ↓
Tag zuordnen
 ↓
Spoolman-Spule auswählen
 ↓
UID-Mapping speichern
 ↓
spoolman:<id> schreiben
 ↓
verifizieren
 ↓
Erfolg
```

Der Benutzer klickt nur:

```text
Tag zuordnen
```

## 26.3 Bambu-Tag zuordnen

```text
Bambu-Tag
 ↓
UID + Definition
 ↓
bestehendes Mapping?
 ├─ ja → Spule laden
 └─ nein
      ↓
      Definition importieren oder bestehende Spule auswählen
      ↓
      Tag zuordnen
      ↓
      UID-Mapping speichern
      ↓
      Originaltag unverändert
```

## 26.4 OpenPrintTag/OpenTag3D

Zuordnung erfolgt per UID-Mapping.

Originalinhalt bleibt unverändert.

## 26.5 Unknown

```text
UID
 ↓
Tag zuordnen
 ↓
Spule auswählen
 ↓
UID-Mapping speichern
```

Keine Änderung am Tag.

## 26.6 Tag-Zuordnung entfernen

```text
Tag erkannt
 ↓
Tag-Zuordnung entfernen
 ↓
Bestätigen
 ↓
Mapping löschen
 ↓
darf eigener Payload sicher gelöscht werden?
 ├─ nein → fertig
 └─ ja → Payload entfernen + verifizieren
```

---

# 27. Bestätigungsdialog beim Entfernen

Beispiel nativer NTAG:

```text
Tag-Zuordnung entfernen?

Die Verbindung zu Spule #42 wird entfernt.
Die FilamentStation-Daten werden auch vom Tag gelöscht.

[Abbrechen] [Entfernen]
```

Bambu:

```text
Tag-Zuordnung entfernen?

Die Verbindung zu Spule #42 wird entfernt.
Der originale Bambu-Tag wird nicht verändert.

[Abbrechen] [Entfernen]
```

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
    ├── nfc/
    └── ui/
```

---

# 29. Logging

Interne Logs dürfen technische Details enthalten:

```text
[AppTask] AssignTag started
[StorageTask] UID mapping stored
[NfcTask] Writing FilamentStation payload
[NfcTask] Payload verified
```

Die GUI verwendet dagegen benutzerfreundliche Begriffe.

Nicht loggen:

* WLAN-Passwort,
* Bambu-Zugangscode,
* Token,
* geheime Schlüssel.

---

# 30. Tests für Zuordnungslogik

Mindestens testen:

## AssignTag

Native NTAG:

* Mapping wird angelegt.
* Payload wird geschrieben.
* Payload wird verifiziert.

Bambu:

* Mapping wird angelegt.
* kein Write-Command.

OpenPrintTag:

* Mapping wird angelegt.
* kein Write-Command.

OpenTag3D:

* Mapping wird angelegt.
* kein Write-Command.

Unknown:

* Mapping wird angelegt.
* kein Write-Command.

## RemoveTagAssignment

Native FilamentStation-Tag:

* Mapping wird entfernt.
* eigener Payload wird entfernt.
* Löschung wird verifiziert.

Bambu:

* Mapping wird entfernt.
* kein Erase-Command.

OpenPrintTag/OpenTag3D:

* Mapping wird entfernt.
* kein Erase-Command.

Unknown:

* Mapping wird entfernt.
* kein Erase-Command.

## Fehlerfälle

* Schreiben schlägt fehl.
* Verifikation schlägt fehl.
* Mapping-Speicherung schlägt fehl.
* Tag wird während Operation entfernt.
* Tag wird zwischen Lesen und Schreiben ausgetauscht.
* UID bei Verifikation stimmt nicht mehr.

---

# 31. Coding-Regeln

* C++17.
* keine Busy-Wait-Schleifen.
* keine Hardwarezugriffe aus AppTask.
* keine LVGL-Aufrufe außerhalb UiTask.
* keine SD-Zugriffe außerhalb StorageTask.
* Parser ohne Netzwerk/Storage.
* technische Tagoperationen in NfcTask.
* Workflowentscheidung in AppTask.
* UI kennt nur semantische Benutzeraktionen.
* keine Formatdetails erfinden.
* Queue-Nachrichten klein halten.

---

# 32. Regeln für Codex

Vor Änderungen:

1. `AGENTS.md` vollständig lesen.
2. `TASKS.md` vollständig lesen.
3. bestehenden Code analysieren.
4. bereits implementierte Funktionen weiterverwenden.
5. keine unnötigen Refactorings.

Aktueller Stand:

> Aufgaben bis einschließlich 5.11 wurden bereits mit dem bisherigen Tag-Menü umgesetzt.

Das neue UX-Konzept erfordert eine Migration, keinen Neubau des NFC-Subsystems.

Codex muss insbesondere vorhandene Implementierungen für:

```text
LinkTag
WriteTag
UnlinkTag
EraseTag
```

identifizieren und unter den neuen Workflows:

```text
AssignTag
RemoveTagAssignment
```

orchestrieren.

Technische NfcCommands dürfen erhalten bleiben.

Die öffentliche UI-Schnittstelle wird jedoch vereinheitlicht.

Nach jedem Teilpaket:

1. EEZ neu generieren, falls nötig.
2. `pio run`.
3. Tests.
4. Compilerwarnungen.
5. geänderte Dateien auflisten.
6. nur erledigte Checkboxen abhaken.
7. nächsten Schritt nennen.
