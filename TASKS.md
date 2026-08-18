# TASKS.md – FilamentStation

## Allgemeine Regeln

* Phasen in Reihenfolge bearbeiten.
* Keine schnelle Polling-Schleife.
* Busy Waiting verboten.
* Nur StorageTask greift direkt auf SD zu.
* Nur UiTask greift auf LVGL zu.
* AppTask koordiniert Workflows.
* Druckerbezogene Nachrichten enthalten `printerId`.
* Spoolman ist führende Datenbank.
* NFC/RFID→Spule-Zuordnungen werden ausschließlich in Spoolman `extra.tag` gespeichert.
* Kein Offline-Spoolman-Betrieb.
* Runtime-Logging ausschließlich über den zentralen Logger.
* Nur tatsächlich erledigte Aufgaben abhaken.

---

# Phase 0 – PlatformIO und Projektbasis

## 0.1 PlatformIO

* [x] ESP32-S3-Projekt
* [x] Arduino Framework
* [x] C++17
* [x] Monitor 115200 Baud
* [x] Flash
* [x] PSRAM
* [x] `.gitignore`
* [x] Build-Anleitung

## 0.2 Grundstruktur

* [x] Verzeichnisstruktur
* [x] BoardConfig
* [x] AppConfig
* [x] TaskConfig
* [x] Secrets.example
* [x] Modelle
* [x] Message-Typen
* [x] Bibliotheksgrundlage

## 0.3 Minimaler Build

* [x] Startmeldung
* [x] Chipmodell
* [x] Heap
* [x] PSRAM
* [x] `pio run`

---

# Phase 1 – FreeRTOS

## 1.1 RtosContext

* [x] Task-Handles
* [x] Queue-Handles
* [x] Event Group
* [x] Mutexes
* [x] Fehlerbehandlung

## 1.2 Nachrichten

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

## 1.4 Konfiguration

* [x] Namen
* [x] Stackgrößen
* [x] Prioritäten
* [x] Core-Affinitäten
* [x] Dokumentation

## 1.5 Kommunikation

* [x] UiTask → AppTask
* [x] AppTask → UiTask
* [x] Queue Timeout
* [x] Queue Overflow
* [x] Logging-Grundlage

---

# Phase 2 – SD und JSON

## 2.1 Hardware

* [x] SD-Schnittstelle
* [x] Pins
* [x] StorageTask-only
* [x] Card Detect
* [x] Interrupt
* [x] Remove/Insert

## 2.2 Verzeichnisse

* [x] `/config`
* [x] `/cache`
* [x] `/queue`
* [x] `/mappings`
* [x] `/diagnostics`
* [x] `/logs`

## 2.3 JsonStorage

* [x] Laden
* [x] Validieren
* [x] Speichern
* [x] Größenlimit
* [x] Fehlercodes
* [x] schemaVersion
* [x] Defaultwerte
* [x] Migration

## 2.4 Atomisches Speichern

* [x] tmp
* [x] flush
* [x] close
* [x] validate
* [x] backup
* [x] rename
* [x] cleanup
* [x] Wiederherstellungstest

## 2.5 Storage Queue

* [x] Lesen
* [x] Schreiben
* [x] Antworten
* [x] mehrere Anfragen
* [x] StorageTask-only

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

## 3.1–3.9 Basis

* [x] Display
* [x] Touch
* [x] LovyanGFX
* [x] LVGL
* [x] UiTask
* [x] EEZ-Migration
* [x] Designsystem
* [x] UI-Datenmodelle
* [x] Home
* [x] Druckerauswahl

## 3.10 Staging

* [x] StagingDetails
* [x] Spooldaten
* [x] Gewichte
* [x] NFC-Status
* [x] Quick Weight
* [x] Advanced Weight
* [x] Slot konfigurieren
* [x] Staging leeren
* [x] Spule auswählen
* [x] nur „Tag zuordnen“
* [x] nur „Tag-Zuordnung entfernen“
* [x] dynamischer Zuordnungsstatus
* [x] keine Write/Link/Erase/Unlink-Begriffe
* [x] EEZ-Actions
* [x] Build

## 3.11 Slot

* [x] Slotdetails
* [x] Tabs
* [x] Aktionen
* [x] TraySelect
* [x] Drucker-/AMS-Wechsel

## 3.12 Spulenauswahl

* [x] separate Hauptscreens entfallen
* [x] Picker-Komponente

## 3.13–3.17 Settings und Dialoge

* [x] Settings Home
* [x] Spoolman
* [x] Drucker
* [x] WLAN
* [x] Scale
* [x] Device
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
* [x] Haltezeit
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
* [x] Entprellung

## 5.4 Parser

* [x] TagTechnology
* [x] TagFormat
* [x] TagDefinition
* [x] TagReadResult
* [x] RawTagData
* [x] ITagParser
* [x] Registry
* [x] FilamentStation Parser
* [x] Bambu Parser
* [x] Legacy Parser
* [x] Unknown
* [x] Tests
* [x] Build

## 5.5 Native Tags

* [x] NTAG213
* [x] NTAG215
* [x] NTAG216
* [x] Lesen
* [x] Schreiben
* [x] Verifizieren
* [x] Löschen
* [x] GUI
* [x] Tests

## 5.6 Bambu

* [x] Erkennung
* [x] Definition
* [x] bisheriges Mapping
* [x] Import
* [x] read-only
* [x] Tests

## 5.7 OpenPrintTag

* [x] Spezifikation
* [x] Parser
* [x] Definition
* [x] Import
* [x] bisheriges Mapping
* [x] read-only
* [x] Tests

## 5.8 OpenTag3D

* [x] Spezifikation
* [x] Parser
* [x] Definition
* [x] Import
* [x] bisheriges Mapping
* [x] read-only
* [x] Tests

## 5.9 Legacy und Unknown

* [x] Legacy Parser
* [x] Migration
* [x] Unknown Screen
* [x] UID-Zuordnung
* [x] sichere Behandlung

## 5.10 Technische Tagoperationen

* [x] nativen Payload schreiben
* [x] Mapping anlegen
* [x] Mapping entfernen
* [x] nativen Payload löschen
* [x] Bambu read-only
* [x] OpenPrintTag read-only
* [x] OpenTag3D read-only
* [x] Unknown unverändert
* [x] Fortschritt
* [x] Verifikation
* [x] Fehlerbehandlung

Hinweis:

Die lokale Mapping-Implementierung ist abgeschlossene Altimplementierung und wird in Phase 7.7 ersetzt.

## 5.11 Lokale Tag-Mappings – Altimplementierung

* [x] nfc-spools.json
* [x] bambu-tags.json
* [x] open-tags.json
* [x] Schema
* [x] UID
* [x] Tagformat
* [x] Spool-ID
* [x] StorageTask
* [x] Konflikte
* [x] doppelte UID
* [x] ungültige Spool-ID
* [x] Entfernen
* [x] Ersetzen
* [x] beschädigte Datei

Diese Architektur ist ab Phase 7.7 deprecated.

## 5.12 Einheitliche Benutzeraktionen

* [x] TagCapabilities
* [x] AssignTag
* [x] RemoveTagAssignment
* [x] UiBridge
* [x] AppTask
* [x] EEZ-Actions
* [x] AssignTag Workflow
* [x] Remove Workflow
* [x] GUI
* [x] dynamischer Status
* [x] Ergebnisdialoge
* [x] Fehlerfälle
* [x] Tests
* [x] Build
* [x] Dokumentation

---

# Phase 6 – WiFiManager

## 6.1 WiFiManager

* [x] Bibliotheksversion
* [x] NetworkTask
* [x] Captive Portal
* [x] AP-Passwort
* [x] Portal Timeout
* [x] Connect Timeout

## 6.2 Portal

* [x] NetworkTask-only
* [x] UI bleibt aktiv
* [x] AppTask Status
* [x] Abbruch
* [x] Timeout
* [x] kein process() in loop()

## 6.3 Events

* [x] WiFi.onEvent
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

* [x] WLAN Status
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
* [x] URL
* [x] Timeout
* [x] Test
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

## 7.4 TagDefinition Import

* [x] TagDefinition
* [x] Vendor
* [x] Material
* [x] Filament
* [x] Farbe
* [x] Temperaturen
* [x] Gewicht
* [x] Treffer
* [x] fehlende Datensätze
* [x] Spule
* [x] Spool-ID
* [x] Dublettenwarnung
* [x] Bambu
* [x] OpenPrintTag
* [x] OpenTag3D
* [x] Legacy

## 7.5 Gewicht

* [x] Quick Weight
* [x] Advanced Weight
* [x] Spule neu laden
* [x] Staging
* [x] Fehler
* [x] bisherige Pending-Implementierung

---

# 7.6 Einheitliches Logging

Diese Aufgabe ist die nächste offene Aufgabe.

## 7.6.1 Bestandsaufnahme

* [x] alle `Serial.print*` im Anwendungscode suchen
* [x] alle `printf`-Logs suchen
* [x] Arduino `log_*` suchen
* [x] ESP-IDF `ESP_LOG*` suchen
* [x] vorhandene Logger-Klassen suchen
* [x] WiFiManager-Debugausgabe prüfen
* [x] weitere Library-Debugausgaben identifizieren
* [x] Logging aus ISRs suchen
* [x] Log-Komponenten dokumentieren

## 7.6.2 Zentraler Logger

* [x] zentrale Logger-Klasse implementieren oder vorhandene vereinheitlichen
* [x] Level `E`
* [x] Level `W`
* [x] Level `I`
* [x] Level `D`
* [x] Level `T`
* [x] feste Component-Tags
* [x] `FS_LOGE`
* [x] `FS_LOGW`
* [x] `FS_LOGI`
* [x] `FS_LOGD`
* [x] `FS_LOGT`
* [x] compile-time beziehungsweise zentrale Level-Konfiguration
* [x] keine dynamischen Strings pro Logzeile nötig
* [x] thread-sichere Ausgabe
* [x] vollständige Zeile atomar ausgeben
* [x] Logging aus ISR verhindern

## 7.6.3 Standardformat

Alle eigenen Logs auf:

```text
<LEVEL> [<COMPONENT>] <message> key=value
```

migrieren.

Beispiele:

```text
I [STORAGE] Initial JSON files loaded count=7
D [NET] WiFi scan completed networks=3
I [NET] WiFi connected ip=192.168.1.42
D [NFC] Tag classified tech=NTAG215 format=FilamentStation
I [SPOOLMAN] Weight updated spool_id=42 weight_g=824.3
```

Aufgaben:

* [x] APP migrieren
* [x] RTOS migrieren
* [x] UI migrieren
* [x] STORAGE migrieren
* [x] NET migrieren
* [x] SCALE migrieren
* [x] NFC migrieren
* [x] SPOOLMAN migrieren
* [x] BAMBU migrieren
* [x] sonstige eigene Module migrieren

## 7.6.4 Drittanbieterlogs

* [ ] WiFiManager-Debugausgabe über offizielle API deaktivieren
* [ ] Netzwerkstatus selbst über `[NET]` loggen
* [ ] Arduino-ESP32-Core-Debuglevel prüfen
* [ ] unnötige Core-Debugausgabe reduzieren
* [ ] andere Library-Debugausgaben soweit offiziell möglich deaktivieren
* [ ] keine Bibliothek nur für Logformat patchen
* [ ] Boot-/Panic-/Exception-Output unverändert lassen

## 7.6.5 PlatformIO Monitor

`platformio.ini` ergänzen:

```ini
monitor_speed = 115200

monitor_filters =
    default
    esp32_exception_decoder
    time
    log2file
```

Aufgaben:

* [ ] `default`
* [ ] `esp32_exception_decoder`
* [ ] `time`
* [ ] `log2file`
* [ ] Monitor mit normalem Lauf testen
* [ ] Logdatei erzeugen
* [ ] Exception-Decoder nicht durch Logger beeinträchtigen

## 7.6.6 Cleanup

* [ ] keine direkten `Serial.print*`-Runtime-Logs außerhalb Logger
* [ ] keine eigenen `log_i/log_d/...` mehr
* [ ] keine gemischten Prefixe
* [ ] keine `*wm:` Debugmeldungen im normalen Betrieb
* [ ] keine sensiblen Daten loggen
* [ ] `docs/logging.md`

## 7.6.7 Tests

* [ ] jede Logstufe
* [ ] jede Kernkomponente
* [ ] key=value-Felder
* [ ] parallele Taskausgabe
* [ ] Level-Filter
* [ ] lange Nachricht
* [ ] Logzeile endet exakt einmal mit Newline
* [ ] `pio device monitor`
* [ ] `log2file`

### Abnahmekriterien 7.6

* alle eigenen Runtime-Logs besitzen dasselbe Format
* keine vermischten Taskzeilen
* WiFiManager-Debug ist im Normalbetrieb deaktiviert
* PlatformIO ergänzt Zeitstempel
* Logdatei wird erzeugt
* ESP32 Exception Decoder bleibt verwendbar
* `pio run` erfolgreich

---

# 7.7 NFC-Zuordnung nach Spoolman `extra.tag` migrieren

## 7.7.1 Bestehende lokale Architektur analysieren

* [ ] alle Zugriffe auf `nfc-spools.json` suchen
* [ ] alle Zugriffe auf `bambu-tags.json` suchen
* [ ] alle Zugriffe auf `open-tags.json` suchen
* [ ] Mapping-Service/Repository identifizieren
* [ ] StorageCommands für Tag-Mappings identifizieren
* [ ] AppTask Mappingpfade identifizieren
* [ ] UI-Abhängigkeiten identifizieren
* [ ] Tests mit lokalen Mappings identifizieren

## 7.7.2 TagIdentity

* [ ] `TagIdentitySource`
* [ ] `TagIdentity`
* [ ] Normalisierung Großbuchstaben
* [ ] Doppelpunkte entfernen
* [ ] Bindestriche entfernen
* [ ] NTAG UID normalisieren
* [ ] Unknown UID normalisieren
* [ ] OpenTag UID normalisieren
* [ ] Bambu UUID verwenden
* [ ] Identität während Workflow fixieren
* [ ] Tests

## 7.7.3 Spoolman Extra Field `tag`

* [ ] Spoolman Extra-Field-Unterstützung der verwendeten Version prüfen
* [ ] vorhandene `tag`-Definition lesen
* [ ] Key `tag` prüfen
* [ ] Typ Text prüfen
* [ ] Feld bei Bedarf automatisch anlegen
* [ ] Fehler bei inkompatiblem Feld behandeln
* [ ] Status an AppTask melden
* [ ] Status in Spoolman Settings anzeigen

## 7.7.4 SpoolmanClient API

Implementieren:

* [ ] `ensureTagExtraField()`
* [ ] `findSpoolByTag()`
* [ ] `setSpoolTag()`
* [ ] `clearSpoolTag()`
* [ ] Extra-Field-Werte korrekt decodieren
* [ ] Extra-Field-Werte korrekt encodieren
* [ ] exakte Übereinstimmung
* [ ] kein Treffer
* [ ] ein Treffer
* [ ] mehrere Treffer
* [ ] HTTP-Fehler
* [ ] Timeout
* [ ] Tests

Keine REST-Endpunkte erfinden.

API gegen die tatsächlich verwendete Spoolman-Version prüfen.

## 7.7.5 AssignTag migrieren

* [ ] Spoolman-Verbindung voraussetzen
* [ ] lokale Mapping-Speicherung entfernen
* [ ] TagIdentity bestimmen
* [ ] bestehenden `extra.tag` Treffer prüfen
* [ ] gleiche Zuordnung idempotent behandeln
* [ ] Zuordnung zu anderer Spule erkennen
* [ ] Benutzerbestätigung bei Neu-Zuordnung
* [ ] alten Spoolman-Wert entfernen
* [ ] neuen Spoolman-Wert setzen
* [ ] Update verifizieren
* [ ] best-effort Rollback bei partieller Reassignment-Störung
* [ ] nativen NTAG anschließend optional beschreiben
* [ ] NDEF anschließend verifizieren
* [ ] Bambu unverändert
* [ ] OpenPrintTag unverändert
* [ ] OpenTag3D unverändert
* [ ] Unknown unverändert

## 7.7.6 RemoveTagAssignment migrieren

* [ ] Spoolman-Verbindung voraussetzen
* [ ] Tag anhand `extra.tag` suchen
* [ ] eindeutige Spule bestimmen
* [ ] `extra.tag` leeren
* [ ] Serverantwort prüfen
* [ ] nativen FilamentStation-Payload optional löschen
* [ ] Payload-Löschung verifizieren
* [ ] Bambu unverändert
* [ ] OpenPrintTag unverändert
* [ ] OpenTag3D unverändert
* [ ] Unknown unverändert
* [ ] lokales Mapping nicht mehr verwenden

## 7.7.7 Native Payload Konsistenz

* [ ] `spoolman:<id>` gegen Spoolman-Zuordnung prüfen
* [ ] konsistenter Fall
* [ ] Payload-ID falsch
* [ ] Spoolman-Zuordnung fehlt
* [ ] Tag ist anderer Spule zugeordnet
* [ ] Konflikt verständlich anzeigen
* [ ] Spoolman als führende Quelle behandeln

## 7.7.8 Duplicate Handling

* [ ] gleiche `extra.tag` ID auf mehreren Spulen erkennen
* [ ] keine automatische Auswahl
* [ ] Error Event
* [ ] GUI-Meldung
* [ ] Log mit Level ERROR/WARN
* [ ] keine automatische Datenänderung

## 7.7.9 Legacy-Mapping-Migration

Übergang für bestehende Geräte:

* [ ] alte Mapping-Dateien optional erkennen
* [ ] nur bei verbundener Spoolman-Instanz migrieren
* [ ] UID normalisieren
* [ ] Spool-ID validieren
* [ ] `extra.tag` des Zielspools prüfen
* [ ] bestehende andere Zuordnung prüfen
* [ ] eindeutige Einträge migrieren
* [ ] Konflikte nicht überschreiben
* [ ] vollständige Migration protokollieren
* [ ] Datei nur nach erfolgreicher vollständiger Migration löschen
* [ ] fehlende Mapping-Dateien normal behandeln

## 7.7.10 Lokale Mapping-Architektur entfernen

Nach erfolgreicher Migration:

* [ ] Runtime-Zugriff auf `nfc-spools.json` entfernen
* [ ] Runtime-Zugriff auf `bambu-tags.json` entfernen
* [ ] Runtime-Zugriff auf `open-tags.json` entfernen
* [ ] StorageCommands für NFC-Mappings entfernen
* [ ] lokale Mapping-Repository-Klasse entfernen
* [ ] tote Modelle entfernen
* [ ] tote Tests entfernen/anpassen
* [ ] `/mappings/printer-slots.json` nur beibehalten, falls weiterhin benötigt
* [ ] Dokumentation aktualisieren

## 7.7.11 Tests

* [ ] NTAG Assignment
* [ ] Bambu Assignment
* [ ] OpenPrintTag Assignment
* [ ] OpenTag3D Assignment
* [ ] Unknown Assignment
* [ ] gleiche Spule
* [ ] andere Spule
* [ ] Duplicate
* [ ] Remove NTAG
* [ ] Remove Bambu
* [ ] Spoolman offline
* [ ] API Timeout
* [ ] Extra Field fehlt
* [ ] Extra Field falscher Typ
* [ ] NDEF/Spoolman Inkonsistenz
* [ ] Legacy Migration

### Abnahmekriterien 7.7

* `extra.tag` ist einzige persistente NFC/RFID-Zuordnung
* keine lokale UID→Spool-ID-Datenbank
* Spoolman-Auflösung funktioniert für alle unterstützten Tagtypen
* native NTAGs können zusätzlich beschrieben werden
* Bambu/Open Tags bleiben unverändert
* Duplicate-Zuordnungen werden erkannt
* Build und Tests erfolgreich

---

# 7.8 Online-only-Spoolman-Betrieb

## 7.8.1 Keine Offline-Workflows

* [ ] Tag zuordnen ohne Spoolman blockieren
* [ ] Tag-Zuordnung entfernen ohne Spoolman blockieren
* [ ] Spulensuche ohne Spoolman blockieren
* [ ] Import ohne Spoolman blockieren
* [ ] Weight Update ohne Spoolman blockieren
* [ ] AMS-Spoolman-Zuordnung ohne Spoolman blockieren
* [ ] verständliche GUI-Meldung

Standardmeldung:

```text
Spoolman ist nicht verbunden.
Diese Funktion benötigt eine aktive Spoolman-Verbindung.
```

## 7.8.2 Pending Measurements entfernen

Die bisherige Implementierung aus 7.5 wird nicht mehr benötigt.

* [ ] automatische Pending-Measurement-Erzeugung entfernen
* [ ] Retry-Queue entfernen
* [ ] `/queue/pending-measurements.json` nicht mehr verwenden
* [ ] Fehler direkt anzeigen
* [ ] manuellen Retry erlauben
* [ ] alte Pending-Datei optional löschen
* [ ] Tests anpassen

## 7.8.3 Persistenten Spoolman-Cache entfernen

Die bisher geplante Cache-Phase entfällt.

Nicht mehr verwenden:

```text
/cache/spools.json
/cache/filaments.json
/cache/vendors.json
```

Aufgaben:

* [ ] vorhandene persistente Spool-Caches suchen
* [ ] Filament-Caches suchen
* [ ] Vendor-Caches suchen
* [ ] Offline-Fallback entfernen
* [ ] alte Dateien ignorieren/löschen
* [ ] keine Spoolman-Daten von SD als Wahrheit verwenden

## 7.8.4 RAM-Cache

Optional erlaubt:

* [ ] prüfen, ob RAM-Cache überhaupt nötig ist
* [ ] TTL definieren
* [ ] maximale Größe definieren
* [ ] nach Write invalidieren
* [ ] bei Disconnect verwerfen
* [ ] niemals Offline-Workflow ermöglichen

Dieser Unterpunkt darf entfallen, wenn kein RAM-Cache benötigt wird.

## 7.8.5 AppState

* [ ] `SpoolmanUnavailable`
* [ ] `SpoolmanReady`
* [ ] `TagFieldUnavailable`
* [ ] Buttons entsprechend aktivieren/deaktivieren
* [ ] Home-Status aktualisieren
* [ ] Settings weiterhin erreichbar

### Abnahmekriterien 7.8

* keine Spoolman-Schreiboperation wird offline gepuffert
* keine lokale Spoolman-Kopie wird als Offline-Datenquelle verwendet
* ohne Spoolman sind abhängige Funktionen eindeutig deaktiviert
* Wiederverbindung ermöglicht sofort wieder normalen Betrieb
* Build erfolgreich

---

# Phase 8 – Bambu und mehrere Drucker

## 8.1 Datenmodell

* [ ] mehrere Drucker
* [ ] stabile PrinterId
* [ ] aktiver Drucker
* [ ] Standarddrucker
* [ ] aktives AMS je Drucker
* [ ] Druckerstatus
* [ ] Slots

## 8.2 Konfiguration

* [ ] bambu.json
* [ ] Name
* [ ] Host
* [ ] Seriennummer
* [ ] LAN Access Code
* [ ] enabled
* [ ] Default
* [ ] selected

## 8.3 BambuTask

* [ ] Commands mit printerId
* [ ] Events mit printerId
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
* [ ] Spoolman-Spule
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
* [ ] Access Code maskieren

---

# Phase 9 – Integrierte Workflows

## 9.1 Hauptworkflow

* [ ] Spoolman-Verbindung prüfen
* [ ] Drucker auswählen
* [ ] AMS auswählen
* [ ] NFC/RFID lesen
* [ ] `extra.tag` Lookup
* [ ] Staging
* [ ] Gewicht
* [ ] Spoolman aktualisieren
* [ ] Slot auswählen
* [ ] Bambu konfigurieren
* [ ] Ergebnis

## 9.2 Native Tags

* [ ] UID
* [ ] extra.tag Lookup
* [ ] NDEF-Konsistenz
* [ ] Spule
* [ ] Staging
* [ ] wiegen
* [ ] aktualisieren
* [ ] AMS

## 9.3 Bambu

* [ ] UUID
* [ ] extra.tag Lookup
* [ ] Definition
* [ ] importieren/zuordnen
* [ ] wiegen
* [ ] AMS
* [ ] Originaltag unverändert

## 9.4 OpenPrintTag

* [ ] TagIdentity
* [ ] extra.tag Lookup
* [ ] Definition
* [ ] Match
* [ ] zuordnen
* [ ] importieren
* [ ] Staging

## 9.5 OpenTag3D

* [ ] TagIdentity
* [ ] extra.tag Lookup
* [ ] Definition
* [ ] Match
* [ ] zuordnen
* [ ] importieren
* [ ] Staging

## 9.6 Legacy

* [ ] TagIdentity
* [ ] extra.tag Lookup
* [ ] Definition
* [ ] importieren
* [ ] zuordnen
* [ ] sichere physische Migration optional

## 9.7 Unknown

* [ ] UID
* [ ] extra.tag Lookup
* [ ] zuordnen
* [ ] physischer Tag unverändert

## 9.8 Staging

* [ ] Quick Weight
* [ ] Advanced Weight
* [ ] Configure Slot
* [ ] Clear Staging
* [ ] Tag zuordnen via Spoolman
* [ ] Tag-Zuordnung entfernen via Spoolman

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

* [ ] Spoolman Required
* [ ] Screens
* [ ] Übergänge
* [ ] Zurück
* [ ] Abbruch
* [ ] requestId
* [ ] printerId
* [ ] stale Responses
* [ ] doppelte Aktionen
* [ ] Tag entfernt
* [ ] TagIdentity geändert

---

# Phase 10 – Robustheit und Diagnose

## 10.1 Task-Diagnose

* [ ] Stack
* [ ] Runtime
* [ ] Queues
* [ ] Event Bits
* [ ] Heap
* [ ] PSRAM

## 10.2 Speicher

* [ ] SD entfernen
* [ ] Stromausfall
* [ ] HX711 trennen
* [ ] PN532 trennen
* [ ] langsame SD
* [ ] JSON beschädigt
* [ ] Backup

## 10.3 NFC/Spoolman-Zuordnung

* [ ] Tag während Read entfernen
* [ ] Tag während Assign entfernen
* [ ] zwei Tags schnell
* [ ] unbekannter NDEF
* [ ] beschädigter NDEF
* [ ] falsche Payload-Spool-ID
* [ ] Bambu nie schreiben
* [ ] OpenPrintTag nie schreiben
* [ ] OpenTag3D nie schreiben
* [ ] Unknown nie schreiben
* [ ] `extra.tag` Duplicate
* [ ] Spule existiert nicht
* [ ] Spoolman fällt während Assignment aus
* [ ] Spoolman Update erfolgreich / NFC Write fehlschlägt
* [ ] Clear extra.tag erfolgreich / NFC Clear fehlschlägt
* [ ] UID-Wechsel vor Verify
* [ ] Tag-Feld fehlt
* [ ] Tag-Feld falscher Typ

## 10.4 Netzwerk

* [ ] WLAN weg
* [ ] Spoolman weg
* [ ] langsame Antwort
* [ ] ungültige Antwort
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

* [ ] Spoolman beim Import weg
* [ ] NFC während Wizard weg
* [ ] Waage instabil
* [ ] Queue voll
* [ ] Antwort spät
* [ ] Abbruch
* [ ] doppelte Messung

## 10.7 Langzeit

* [ ] Tasks blockieren korrekt
* [ ] Critical Sections
* [ ] Mutex
* [ ] mehrstündiger Test
* [ ] Speicher
* [ ] UI
* [ ] Dateien
* [ ] Reconnect
* [ ] Logger

---

# Phase 11 – Dokumentation und Release

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

## 11.2 Logging

* [ ] kanonisches Format
* [ ] Level
* [ ] Components
* [ ] Logger API
* [ ] PlatformIO monitor_filters
* [ ] WiFiManager-Debug
* [ ] Debug/Release Level
* [ ] sensitive Daten

## 11.3 NFC/RFID

* [ ] Tagtechnologien
* [ ] Tagformate
* [ ] TagIdentity
* [ ] UID-Normalisierung
* [ ] Bambu UUID
* [ ] FilamentStation Payload
* [ ] Capabilities
* [ ] Spoolman `extra.tag`
* [ ] AssignTag
* [ ] RemoveTagAssignment
* [ ] Duplicate Handling
* [ ] kein lokales Mapping

## 11.4 Daten

* [ ] lokale JSON-Dateien
* [ ] Verzeichnisse
* [ ] Backup
* [ ] keine NFC-Mapping-Dateien
* [ ] kein Pending Spoolman Write
* [ ] kein persistenter Offline-Spoolman-Cache

## 11.5 Workflows

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
* [ ] Spoolman Offline Error Flow

## 11.6 Benutzeranleitung

* [ ] Installation
* [ ] WLAN
* [ ] Spoolman
* [ ] Extra-Feld `tag`
* [ ] Waage
* [ ] NFC
* [ ] Tag zuordnen
* [ ] Tag-Zuordnung entfernen
* [ ] Bambu importieren
* [ ] Drucker
* [ ] AMS
* [ ] Firmware

## 11.7 Entwickler

* [ ] Build
* [ ] Upload
* [ ] Tests
* [ ] EEZ Export
* [ ] Logger
* [ ] Screen
* [ ] Action
* [ ] Task
* [ ] JSON
* [ ] Tagparser
* [ ] Spoolman Extra Field

## 11.8 Release

* [ ] Lizenzen
* [ ] SpoolEase-Code nicht kopiert
* [ ] Quellen
* [ ] eigene Lizenz
* [ ] Version
* [ ] Changelog
* [ ] Release
* [ ] reproduzierbarer Build
* [ ] Known Issues
* [ ] kein Security-Key
* [ ] keine lokale NFC-Zuordnungsdatenbank
