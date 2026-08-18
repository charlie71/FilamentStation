# TASKS.md – FilamentStation

## Allgemeine Regeln

* Phasen in Reihenfolge bearbeiten.
* Keine schnelle Polling-Schleife.
* Busy Waiting verboten.
* nur StorageTask → SD.
* nur UiTask → LVGL.
* AppTask koordiniert Workflows.
* persistente Daten als JSON auf SD.
* druckerbezogene Nachrichten enthalten `printerId`.
* nur tatsächlich erledigte Aufgaben abhaken.

---

# Phase 0 – PlatformIO und Projektbasis

## 0.1 PlatformIO-Projekt

* [x] PlatformIO-Projekt für ESP32-S3 anlegen
* [x] Arduino Framework konfigurieren
* [x] C++17 aktivieren
* [x] seriellen Monitor mit 115200 Baud konfigurieren
* [x] Flashgröße vorbereiten
* [x] PSRAM vorbereiten
* [x] `.gitignore` anlegen
* [x] Build-Anleitung in `README.md` erstellen

## 0.2 Grundstruktur

* [x] Verzeichnisstruktur aus `AGENTS.md` anlegen
* [x] `BoardConfig.h`
* [x] `AppConfig.h`
* [x] `TaskConfig.h`
* [x] `Secrets.example.h`
* [x] minimale Modelle
* [x] Message-Typen
* [x] keine unnötigen Bibliotheken

## 0.3 Minimaler Build

* [x] Startmeldung
* [x] Chipmodell
* [x] Heap
* [x] PSRAM
* [x] `pio run`

---

# Phase 1 – FreeRTOS-Infrastruktur

## 1.1 RtosContext

* [x] RtosContext
* [x] Task-Handles
* [x] Queue-Handles
* [x] Event Group
* [x] Mutexes
* [x] Fehlerbehandlung

## 1.2 Nachrichtentypen

* [x] AppEvent
* [x] UiCommand
* [x] UiAction
* [x] ScaleCommand
* [x] NfcCommand
* [x] StorageCommand
* [x] NetworkCommand
* [x] SpoolmanCommand
* [x] BambuCommand
* [x] requestId
* [x] printerId

## 1.3 Tasks

* [x] UiTask
* [x] AppTask
* [x] ScaleTask
* [x] NfcTask
* [x] StorageTask
* [x] NetworkTask
* [x] SpoolmanTask
* [x] BambuTask

## 1.4 Task-Konfiguration

* [x] Namen
* [x] Stackgrößen
* [x] Prioritäten
* [x] Core-Affinitäten
* [x] Dokumentation

## 1.5 Kommunikationstest

* [x] UiTask → AppTask
* [x] AppTask → UiTask
* [x] Queue-Timeout
* [x] Queue-Überlauf
* [x] Logging

---

# Phase 2 – SD und JSON

## 2.1 SD-Hardware

* [x] Schnittstelle
* [x] Pinbelegung
* [x] StorageTask-only
* [x] Card Detect
* [x] Interrupt falls vorhanden
* [x] Entfernen/Einsetzen

## 2.2 Verzeichnisse

* [x] `/config`
* [x] `/cache`
* [x] `/queue`
* [x] `/mappings`
* [x] `/diagnostics`
* [x] `/logs`

## 2.3 JsonStorage

* [x] laden
* [x] validieren
* [x] speichern
* [x] Dateigröße
* [x] Fehlercodes
* [x] schemaVersion
* [x] Defaultwerte
* [x] Migration

## 2.4 Atomisches Speichern

* [x] tmp schreiben
* [x] flush
* [x] schließen
* [x] validieren
* [x] Backup
* [x] umbenennen
* [x] Backup entfernen
* [ ] Wiederherstellung testen

## 2.5 Storage-Queue

* [x] Lesen
* [x] Schreiben
* [x] Antworten
* [x] mehrere Anfragen
* [x] keine anderen direkten SD-Zugriffe

## 2.6 Konfigurationen

* [x] device.json
* [x] network.json
* [x] spoolman.json
* [x] bambu.json
* [x] ui.json
* [x] scale.json
* [x] nfc.json

---

# Phase 3 – Display, LVGL und GUI

## 3.1–3.9

* [x] Hardwareprüfung
* [x] LovyanGFX
* [x] Touch
* [x] LVGL
* [x] UiTask
* [x] bestehende EEZ-GUI migriert
* [x] Designsystem
* [x] UI-Datenmodelle
* [x] Home
* [x] Druckerauswahl

## 3.10 Staging-Screens – UX-Migration erforderlich

Die Screens wurden bereits implementiert, müssen aber auf das neue Tag-Konzept migriert werden.

Bereits vorhanden:

* [x] `SCR_STAGING_DETAILS`
* [x] Spooldaten
* [x] Gewichte
* [x] NFC-Status
* [x] Quick Weight
* [x] Advanced Weight
* [x] Slot konfigurieren
* [x] Staging leeren
* [x] Spule auswählen

Neu zu migrieren:

* [ ] bisherige Buttons „Tag schreiben“ und „Tag verknüpfen“ entfernen
* [ ] stattdessen genau einen Button „Tag zuordnen“ anzeigen
* [ ] bisherige Buttons „Tag löschen“ und „Tag trennen“ entfernen
* [ ] stattdessen genau einen Button „Tag-Zuordnung entfernen“ anzeigen
* [ ] Tagaktionen abhängig vom Zuordnungsstatus aktivieren
* [ ] „Tag zuordnen“ deaktivieren beziehungsweise anpassen, wenn bereits korrekt zugeordnet
* [ ] „Tag-Zuordnung entfernen“ nur bei vorhandener Zuordnung anbieten
* [ ] keine technischen Write/Link/Erase/Unlink-Begriffe im UI anzeigen
* [ ] EEZ-Actions entsprechend migrieren
* [ ] Build

## 3.11 Slot-Screens

* [x] Slotdetails
* [x] Tabs
* [x] Slotaktionen
* [x] TraySelect
* [x] Drucker-/AMS-Wechsel

## 3.12 Spulenscreens

Separate Hauptscreens entfallen.

Spulenauswahl erfolgt über Picker.

## 3.13–3.17 Settings/Dialoge

* [x] Settings Home
* [x] Spoolman Settings
* [x] Druckerverwaltung
* [x] WLAN Settings
* [x] Scale Settings
* [x] Device Settings
* [x] Diagnostics
* [x] Firmware
* [x] Dialoge
* [x] Overlays

---

# Phase 4 – Waage

## 4.1–4.8

* [x] HX711
* [x] IRQ
* [x] ScaleTask
* [x] Filter
* [x] Tarierung
* [x] Kalibrierung
* [x] GUI
* [x] Quick Weight
* [x] Advanced Weight
* [x] Tests

---

# Phase 5 – NFC/RFID

## 5.1 Hardware

* [x] PN532
* [x] IRQ
* [x] ISR
* [x] Task Notification

## 5.2 Bus

* [x] Bus geprüft
* [x] Mutex
* [x] Haltezeiten
* [x] Deadlock-Vermeidung

## 5.3 Basis

* [x] UID
* [x] NDEF
* [x] spoolman-Payload
* [x] Bambu
* [x] Legacy
* [x] Schreiben
* [x] Löschen
* [x] Verifizieren
* [x] Entprellen

## 5.4 Parserarchitektur

* [x] TagTechnology
* [x] TagFormat
* [x] TagDefinition
* [x] TagReadResult
* [x] RawTagData
* [x] ITagParser
* [x] Registry
* [x] FilamentStation Parser
* [x] Bambu Parser Integration
* [x] Legacy Integration
* [x] Unknown
* [x] Tests
* [x] Build

## 5.5 Native Tags

* [x] NTAG213
* [x] NTAG215
* [x] NTAG216
* [x] Payload lesen
* [x] Payload schreiben
* [x] Verifikation
* [x] Löschen
* [x] GUI
* [x] Tests

## 5.6 Bambu

* [x] Erkennung
* [x] Definition
* [x] Mapping
* [x] Importworkflow
* [x] read-only
* [x] Tests

## 5.7 OpenPrintTag

* [x] Spezifikation
* [x] Parser
* [x] Definition
* [x] Import
* [x] Mapping
* [x] read-only
* [x] Tests

## 5.8 OpenTag3D

* [x] Spezifikation
* [x] Parser
* [x] Definition
* [x] Import
* [x] Mapping
* [x] read-only
* [x] Tests

## 5.9 Legacy und Unknown

* [x] Legacy Parser
* [x] Migration
* [x] Unknown Screen
* [x] UID-Zuordnung
* [x] sichere Behandlung

## 5.10 Bisherige Tagoperationen

Technische Funktionalität ist bereits implementiert:

* [x] nativen Tag schreiben
* [x] Mapping anlegen
* [x] Mapping entfernen
* [x] nativen Payload löschen
* [x] Bambu nicht beschreiben
* [x] OpenPrintTag nicht beschreiben
* [x] OpenTag3D nicht beschreiben
* [x] Unknown nicht beschreiben
* [x] Fortschritt
* [x] Verifikation
* [x] Fehlerbehandlung

Diese technischen Operationen bleiben erhalten.

Die Benutzeroberfläche wird in 5.12 vereinheitlicht.

## 5.11 Tag-Mappings

* [x] nfc-spools.json
* [x] bambu-tags.json
* [x] open-tags.json
* [x] Schema
* [x] normalisierte UID
* [x] Tagformat
* [x] Spool-ID
* [x] StorageTask
* [x] Konflikte
* [x] doppelte UID
* [x] ungültige Spool-ID
* [x] Mapping entfernen
* [x] Mapping ersetzen
* [x] beschädigte Datei behandeln

---

# 5.12 UX-Migration: einheitliche Tag-Zuordnung

Diese Aufgabe ist die nächste offene Aufgabe.

Bestehende Write/Link/Erase/Unlink-Implementierungen werden nicht entfernt, sondern hinter einem einheitlichen Workflow gekapselt.

## 5.12.1 Capability-Modell

* [x] bestehendes `TagReadResult` analysieren
* [x] `TagCapabilities` ergänzen oder vorhandenes Modell erweitern
* [x] `canAssociateByUid`
* [x] `canWriteFilamentStationPayload`
* [x] `canClearFilamentStationPayload`
* [x] `preserveOriginalContent`
* [x] TagCapabilities für NTAG213
* [x] TagCapabilities für NTAG215
* [x] TagCapabilities für NTAG216
* [x] TagCapabilities für Bambu
* [x] TagCapabilities für OpenPrintTag
* [x] TagCapabilities für OpenTag3D
* [x] TagCapabilities für Legacy
* [x] TagCapabilities für Unknown
* [x] bestehendes einfaches `writable` nicht als alleinige Entscheidungsgrundlage verwenden
* [x] Tests

## 5.12.2 UiAction migrieren

Alte öffentliche Benutzeraktionen:

```text
LinkTag
WriteTag
EraseTag
UnlinkTag
AssignUnknownTag
RewriteLegacyTag
```

werden ersetzt durch:

```text
AssignTag
RemoveTagAssignment
```

Aufgaben:

* [x] bestehende Verwendungen aller alten Actions suchen
* [x] `AssignTag` ergänzen
* [x] `RemoveTagAssignment` ergänzen
* [x] alte Actions aus Benutzeroberfläche entfernen
* [x] alte Actions intern nur vorübergehend als Kompatibilitätsadapter zulassen
* [x] UiBridge migrieren
* [x] AppTask migrieren
* [x] EEZ-Actions migrieren
* [x] Tests anpassen

## 5.12.3 AssignTag-Workflow

* [x] Benutzer wählt nur „Tag zuordnen“
* [x] Spoolman-Spule über Picker auswählen
* [x] UID-Mapping erstellen
* [x] Capability prüfen
* [x] bei nativem beschreibbaren Tag `spoolman:<id>` schreiben
* [x] geschriebenen Payload verifizieren
* [x] bei Bambu nur Mapping
* [x] bei OpenPrintTag nur Mapping
* [x] bei OpenTag3D nur Mapping
* [x] bei Unknown nur Mapping
* [x] Legacy nur schreiben, wenn explizit sicher freigegeben
* [x] Originalinhalt fremder Tags bewahren
* [x] Ergebnisstatus differenzieren
* [x] Tagwechsel während Workflow erkennen
* [x] UID bei Verifikation prüfen

## 5.12.4 Verhalten bei Schreibfehler

Standardpolicy:

> Ein erfolgreich gespeichertes UID-Mapping bleibt bestehen, auch wenn das optionale Beschreiben des Tags fehlschlägt.

* [x] Policy implementieren
* [x] Benutzer verständlich informieren
* [x] Mapping nicht unbemerkt zurückrollen
* [x] Fehlerstatus protokollieren
* [x] erneuten Versuch ermöglichen

Beispiel:

```text
Tag wurde zugeordnet.
Die Zuordnung konnte jedoch nicht auf dem Tag gespeichert werden.
```

## 5.12.5 RemoveTagAssignment-Workflow

* [x] Benutzer wählt nur „Tag-Zuordnung entfernen“
* [x] Bestätigungsdialog
* [x] lokales Mapping entfernen
* [x] Capability prüfen
* [x] nativen FilamentStation-Payload wenn sicher möglich entfernen
* [x] Löschung verifizieren
* [x] Bambu-Inhalt unverändert
* [x] OpenPrintTag unverändert
* [x] OpenTag3D unverändert
* [x] Unknown unverändert
* [x] Legacy nur verändern, wenn explizit sicher
* [x] Ergebnisstatus differenzieren

## 5.12.6 GUI migrieren

### StagingActions

* [x] „Tag schreiben“ entfernen
* [x] „Tag verknüpfen“ entfernen
* [x] „Tag löschen“ entfernen
* [x] „Tag trennen“ entfernen
* [x] „Tag zuordnen“ ergänzen
* [x] „Tag-Zuordnung entfernen“ ergänzen

### Tag-Screens

* [x] `SCR_TAG_ACTION_SELECT` auf einheitlichen Workflow migrieren
* [x] wenn sinnvoll in `SCR_TAG_ASSIGN` umbenennen
* [x] `SCR_TAG_WRITE` nicht mehr als benutzerseitigen „Tag schreiben“-Screen verwenden
* [x] generischen `SCR_TAG_OPERATION` verwenden oder bestehenden Screen entsprechend migrieren
* [x] Review-Screen vereinfachen
* [x] Result-Screen anpassen
* [x] Unknown-Screen anpassen
* [x] Legacy-Screen anpassen

### Beschriftungen

* [x] keine Anzeige „Tag schreiben“
* [x] keine Anzeige „Tag verknüpfen“
* [x] keine Anzeige „Tag löschen“
* [x] keine Anzeige „Tag trennen“
* [x] technische Schritte dürfen nur als Statusinformation erscheinen
* [x] deutscher UI-Text konsistent halten

## 5.12.7 Dynamische GUI

Nicht zugeordnet:

```text
Tag zuordnen
```

Zuordnung vorhanden:

```text
Tag-Zuordnung entfernen
```

Optional zusätzlich bei falscher Zuordnung:

```text
Tag neu zuordnen
```

„Tag neu zuordnen“ darf technisch als `AssignTag` mit neuer Spool-ID umgesetzt werden.

* [x] Zuordnungsstatus anzeigen
* [x] aktuelle Spool-ID anzeigen
* [x] korrekte Buttons aktivieren/deaktivieren
* [x] keine technisch unmögliche Aktion anbieten

## 5.12.8 Ergebnisdialoge

* [x] Mapping + Tag geschrieben
* [x] nur Mapping
* [x] Mapping entfernt + Payload entfernt
* [x] nur Mapping entfernt
* [x] Mapping erfolgreich, Schreiben fehlgeschlagen
* [x] Mapping entfernt, Tagbereinigung fehlgeschlagen
* [x] Tag während Operation entfernt
* [x] UID geändert

## 5.12.9 Tests

AssignTag:

* [x] NTAG → Mapping + Write
* [x] Bambu → Mapping ohne Write
* [ ] OpenPrintTag → Mapping ohne Write
* [ ] OpenTag3D → Mapping ohne Write
* [x] Unknown → Mapping ohne Write
* [x] Legacy SafeRewrite → Mapping + Write
* [ ] Legacy Preserve → Mapping-only

RemoveTagAssignment:

* [x] NTAG → Mapping entfernen + Payload entfernen
* [x] Bambu → Mapping entfernen, kein Erase
* [ ] OpenPrintTag → Mapping entfernen, kein Erase
* [ ] OpenTag3D → Mapping entfernen, kein Erase
* [x] Unknown → Mapping entfernen, kein Erase

Fehler:

* [ ] Write schlägt fehl
* [ ] Verify schlägt fehl
* [ ] Storage schlägt fehl
* [x] Mapping existiert bereits
* [x] Tag entfernt
* [x] UID geändert

## 5.12.10 Build und Migration

* [x] EEZ-Projekt neu generieren
* [x] `pio run`
* [x] vorhandene NFC-Tests
* [x] neue Assignment-Tests
* [x] Compilerwarnungen prüfen
* [x] alte ungenutzte UiActions entfernen
* [x] keine toten EEZ-Actions
* [x] Dokumentation aktualisieren

### Abnahmekriterien 5.12

* Benutzer sieht nur „Tag zuordnen“ und „Tag-Zuordnung entfernen“.
* Benutzer muss nicht wissen, ob ein Mapping oder Schreibvorgang notwendig ist.
* Native NTAGs werden beim Zuordnen automatisch beschrieben.
* Bambu-Tags bleiben unverändert.
* OpenPrintTag/OpenTag3D bleiben unverändert.
* UID-Mapping funktioniert für alle unterstützten Tags.
* Beim Entfernen werden nur von FilamentStation sicher verwaltbare Inhalte gelöscht.
* keine fremden Tagdaten werden beschädigt.
* Build erfolgreich.
* Tests erfolgreich.

---

# Phase 6 – WiFiManager

## 6.1 WiFiManager

* [x] feste Bibliotheksversion
* [x] Instanz im NetworkTask
* [x] Captive Portal
* [x] AP-Passwort
* [x] Portal-Timeout
* [x] Verbindungs-Timeout

## 6.2 Portalbetrieb

* [x] nur NetworkTask
* [x] UI bleibt aktiv
* [x] AppTask erhält Status
* [x] Abbruch
* [x] Timeout
* [x] kein process() in loop()

## 6.3 WiFi-Events

* [x] WiFi.onEvent()
* [x] Connected
* [x] Got IP
* [x] Disconnect
* [x] Lost IP
* [x] kurze Callbacks
* [x] Event Group

## 6.4 Netzwerkparameter

* [x] network.json
* [x] Hostname
* [x] DHCP/statisch
* [x] DNS
* [x] Portalname
* [x] Timeouts
* [x] StorageTask

## 6.5 GUI

* [x] WLAN-Status
* [x] SSID
* [x] IP
* [x] RSSI
* [x] Captive Portal
* [x] neu konfigurieren
* [x] Zugangsdaten löschen
* [x] Anleitung

---

# Phase 7 – Spoolman

## 7.1 Konfiguration

* [x] spoolman.json
* [x] GUI laden
* [x] GUI speichern
* [x] URL normalisieren
* [x] Timeout
* [x] Verbindung testen
* [x] Status
* [x] Version

## 7.2 Spulen

* [x] nach ID
* [x] Suche
* [x] Filter
* [x] CMP_SPOOL_PICKER
* [x] kompakte Infos

## 7.3 Hersteller und Filamente

* [x] Vendor suchen
* [x] Vendor anlegen
* [x] Filament suchen
* [x] Filament anlegen
* [x] Dubletten
* [x] Validierung

## 7.4 TagDefinition-Import

* [x] TagDefinition abbilden
* [x] Vendor
* [x] Material
* [x] Filament
* [x] Farbe
* [x] Temperaturen
* [x] Gewicht
* [x] Treffer vorschlagen
* [x] fehlende Datensätze
* [x] Spule anlegen
* [x] Spool-ID
* [x] Dublettenwarnung
* [x] Bambu
* [x] OpenPrintTag
* [x] OpenTag3D
* [x] Legacy

## 7.5 Gewicht

* [ ] Quick Weight
* [ ] Advanced Weight
* [ ] Spule neu laden
* [ ] Staging
* [ ] Fehler
* [ ] Pending

## 7.6 Cache

* [ ] Spulen
* [ ] Filamente
* [ ] Vendor
* [ ] Alter
* [ ] stale
* [ ] nicht führend

---

# Phase 8 – Bambu und mehrere Drucker

## 8.1 Datenmodell

* [ ] mehrere Drucker
* [ ] PrinterId
* [ ] aktiver Drucker
* [ ] Standard
* [ ] AMS je Drucker
* [ ] Cache
* [ ] Slots

## 8.2 Konfiguration

* [ ] bambu.json
* [ ] Name
* [ ] Host
* [ ] Serial
* [ ] Access Code
* [ ] enabled
* [ ] Default
* [ ] selected

## 8.3 BambuTask

* [ ] printerId
* [ ] verbinden
* [ ] trennen
* [ ] testen
* [ ] Status
* [ ] AMS
* [ ] Slots
* [ ] External
* [ ] Slotdaten schreiben
* [ ] Reset
* [ ] Reconnect

## 8.4 Druckerwechsel

* [ ] Zustand sichern
* [ ] wechseln
* [ ] Header
* [ ] AMS
* [ ] Staging erhalten
* [ ] stale Responses
* [ ] printerId prüfen

## 8.5 AMS-Zuordnung

* [ ] Staging
* [ ] Drucker
* [ ] AMS
* [ ] Slot
* [ ] Daten
* [ ] Command
* [ ] Antwort
* [ ] Reload
* [ ] Ergebnis

## 8.6 GUI

* [ ] hinzufügen
* [ ] bearbeiten
* [ ] löschen
* [ ] Default
* [ ] aktiv
* [ ] testen
* [ ] Code maskieren

---

# Phase 9 – Integrierte Workflows

## 9.1 Hauptworkflow

* [ ] Drucker
* [ ] AMS
* [ ] NFC/RFID
* [ ] Staging
* [ ] Gewicht
* [ ] Spoolman
* [ ] Slot
* [ ] Bambu
* [ ] Ergebnis

## 9.2 Native Tags

* [ ] erkennen
* [ ] Spool-ID
* [ ] Staging
* [ ] wiegen
* [ ] aktualisieren
* [ ] AMS

## 9.3 Bambu-Tags

* [ ] erkennen
* [ ] Mapping
* [ ] Definition
* [ ] importieren/zuordnen
* [ ] wiegen
* [ ] AMS
* [ ] unverändert lassen

## 9.4 OpenPrintTag

* [ ] erkennen
* [ ] Definition
* [ ] Match
* [ ] Tag zuordnen
* [ ] importieren
* [ ] Staging

## 9.5 OpenTag3D

* [ ] erkennen
* [ ] Definition
* [ ] Match
* [ ] Tag zuordnen
* [ ] importieren
* [ ] Staging

## 9.6 Legacy

* [ ] erkennen
* [ ] anzeigen
* [ ] importieren
* [ ] Tag zuordnen
* [ ] sichere automatische Migration nur innerhalb AssignTag

## 9.7 Unknown

* [ ] Technologie
* [ ] UID
* [ ] NDEF
* [ ] Tag zuordnen
* [ ] unverändert lassen

## 9.8 Staging-Workflow

* [ ] Quick Weight
* [ ] Advanced Weight
* [ ] Configure Slot
* [ ] Clear Staging
* [ ] Tag zuordnen
* [ ] Tag-Zuordnung entfernen

Nicht mehr Bestandteil des Benutzerworkflows:

```text
Write Tag
Link Tag
Unlink Tag
Erase Tag
```

## 9.9 Slot

* [ ] Details
* [ ] Spule
* [ ] Configure from Staging
* [ ] Configure Manually
* [ ] Untag Slot
* [ ] Reset
* [ ] Reapply
* [ ] Refresh

## 9.10 Zustandsautomat

* [ ] Screens
* [ ] Übergänge
* [ ] Zurück
* [ ] Abbruch
* [ ] requestId
* [ ] printerId
* [ ] verspätete Antworten
* [ ] doppelte Aktionen
* [ ] Tag entfernt
* [ ] UID-Wechsel

---

# Phase 10 – Robustheit

## 10.1 Task-Diagnose

* [ ] Stack
* [ ] Runtime
* [ ] Queues
* [ ] Event Bits
* [ ] Heap
* [ ] PSRAM

## 10.2 Hardware/Speicher

* [ ] SD entfernen
* [ ] Stromausfall
* [ ] HX711 weg
* [ ] PN532 weg
* [ ] langsame SD
* [ ] JSON beschädigt
* [ ] Backup

## 10.3 NFC/RFID

* [ ] Tag während Read weg
* [ ] Tag während Assignment weg
* [ ] zwei Tags schnell
* [ ] unbekannter NDEF
* [ ] beschädigter NDEF
* [ ] falsche Spool-ID
* [ ] Bambu nie schreiben
* [ ] OpenPrintTag nie schreiben
* [ ] OpenTag3D nie schreiben
* [ ] Unknown nie schreiben
* [ ] Mapping-Konflikt
* [ ] Spule existiert nicht
* [ ] Write-Failure nach erfolgreichem Mapping
* [ ] Clear-Failure nach entferntem Mapping
* [ ] UID-Wechsel vor Verify

## 10.4 Netzwerk

* [ ] WLAN weg
* [ ] Spoolman weg
* [ ] langsam
* [ ] ungültig
* [ ] Reconnect
* [ ] MQTT

## 10.5 Mehrdrucker

* [ ] Wechsel während MQTT
* [ ] Wechsel während Update
* [ ] offline
* [ ] mehrere offline
* [ ] aktiven löschen
* [ ] Default löschen
* [ ] alte Antwort
* [ ] AMS weg
* [ ] Daten trennen

## 10.6 Workflow

* [ ] Spoolman bei Import weg
* [ ] NFC während Wizard weg
* [ ] Waage instabil
* [ ] Queue voll
* [ ] Antwort spät
* [ ] Abbruch
* [ ] doppelte Messung

## 10.7 Langzeit

* [ ] Task Blocking
* [ ] Critical Sections
* [ ] Mutex
* [ ] Langzeit
* [ ] Speicher
* [ ] UI
* [ ] Dateien
* [ ] Neustart

---

# Phase 11 – Dokumentation

## 11.1 Technik

* [ ] Architektur
* [ ] Tasks
* [ ] Queues
* [ ] Events
* [ ] IRQ
* [ ] Prioritäten
* [ ] Stacks
* [ ] GPIO
* [ ] Verdrahtung
* [ ] BOM

## 11.2 NFC/RFID

* [ ] Tagtechnologien
* [ ] Tagformate
* [ ] FilamentStation-Payload
* [ ] NTAG215
* [ ] NTAG213
* [ ] NTAG216
* [ ] Bambu read-only
* [ ] OpenPrintTag
* [ ] OpenTag3D
* [ ] Legacy
* [ ] Unknown
* [ ] Parser
* [ ] Capabilities
* [ ] Zuordnungsworkflow
* [ ] Remove-Workflow

## 11.3 Daten

* [ ] JSON
* [ ] Verzeichnisse
* [ ] Backup
* [ ] Cache
* [ ] Pending
* [ ] Mappings

## 11.4 Workflows

* [ ] Screens
* [ ] Navigation
* [ ] Hauptworkflow
* [ ] Staging
* [ ] Slot
* [ ] Tag zuordnen
* [ ] Tag-Zuordnung entfernen
* [ ] Bambu
* [ ] OpenPrintTag
* [ ] OpenTag3D
* [ ] Legacy
* [ ] Unknown
* [ ] Mehrdrucker

## 11.5 Benutzeranleitung

* [ ] Installation
* [ ] WLAN
* [ ] Spoolman
* [ ] Waage
* [ ] NFC
* [ ] Tag zuordnen
* [ ] Tag-Zuordnung entfernen
* [ ] Bambu importieren
* [ ] Open Tags
* [ ] Drucker
* [ ] AMS
* [ ] Update

## 11.6 Entwickler

* [ ] Build
* [ ] Upload
* [ ] Tests
* [ ] EEZ Export
* [ ] Screen
* [ ] Action
* [ ] Task
* [ ] JSON
* [ ] Tagparser

## 11.7 Release

* [ ] Lizenzen
* [ ] SpoolEase-Code nicht kopiert
* [ ] Quellen
* [ ] eigene Lizenz
* [ ] Version
* [ ] Changelog
* [ ] Release
* [ ] reproduzierbar
* [ ] Known Issues
* [ ] kein Security-Key-Workflow
