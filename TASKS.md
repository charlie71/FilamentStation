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

* [x] WiFiManager-Debugausgabe über offizielle API deaktivieren
* [x] Netzwerkstatus selbst über `[NET]` loggen
* [x] Arduino-ESP32-Core-Debuglevel prüfen
* [x] unnötige Core-Debugausgabe reduzieren
* [x] andere Library-Debugausgaben soweit offiziell möglich deaktivieren
* [x] keine Bibliothek nur für Logformat patchen
* [x] Boot-/Panic-/Exception-Output unverändert lassen

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

* [x] `default`
* [x] `esp32_exception_decoder`
* [x] `time`
* [x] `log2file`
* [x] Monitor mit normalem Lauf testen
* [x] Logdatei erzeugen
* [x] Exception-Decoder nicht durch Logger beeinträchtigen

## 7.6.6 Cleanup

* [x] keine direkten `Serial.print*`-Runtime-Logs außerhalb Logger
* [x] keine eigenen `log_i/log_d/...` mehr
* [x] keine gemischten Prefixe
* [x] keine `*wm:` Debugmeldungen im normalen Betrieb
* [x] keine sensiblen Daten loggen
* [x] `docs/logging.md`

## 7.6.7 Tests

* [x] jede Logstufe
* [x] jede Kernkomponente
* [x] key=value-Felder
* [x] parallele Taskausgabe
* [x] Level-Filter
* [x] lange Nachricht
* [x] Logzeile endet exakt einmal mit Newline
* [x] `pio device monitor`
* [x] `log2file`

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

* [x] alle Zugriffe auf `nfc-spools.json` suchen
* [x] alle Zugriffe auf `bambu-tags.json` suchen
* [x] alle Zugriffe auf `open-tags.json` suchen
* [x] Mapping-Service/Repository identifizieren
* [x] StorageCommands für Tag-Mappings identifizieren
* [x] AppTask Mappingpfade identifizieren
* [x] UI-Abhängigkeiten identifizieren
* [x] Tests mit lokalen Mappings identifizieren

## 7.7.2 TagIdentity

* [x] `TagIdentitySource`
* [x] `TagIdentity`
* [x] Normalisierung Großbuchstaben
* [x] Doppelpunkte entfernen
* [x] Bindestriche entfernen
* [x] NTAG UID normalisieren
* [x] Unknown UID normalisieren
* [x] OpenTag UID normalisieren
* [x] Bambu UUID verwenden
* [x] Identität während Workflow fixieren
* [x] Tests

## 7.7.3 Spoolman Extra Field `tag`

* [x] Spoolman Extra-Field-Unterstützung der verwendeten Version prüfen
* [x] vorhandene `tag`-Definition lesen
* [x] Key `tag` prüfen
* [x] Typ Text prüfen
* [x] Feld bei Bedarf automatisch anlegen
* [x] Fehler bei inkompatiblem Feld behandeln
* [x] Status an AppTask melden
* [x] Status in Spoolman Settings anzeigen

## 7.7.4 SpoolmanClient API

Implementieren:

* [x] `ensureTagExtraField()`
* [x] `findSpoolByTag()`
* [x] `setSpoolTag()`
* [x] `clearSpoolTag()`
* [x] Extra-Field-Werte korrekt decodieren
* [x] Extra-Field-Werte korrekt encodieren
* [x] exakte Übereinstimmung
* [x] kein Treffer
* [x] ein Treffer
* [x] mehrere Treffer
* [x] HTTP-Fehler
* [x] Timeout
* [x] Tests

Keine REST-Endpunkte erfinden.

API gegen die tatsächlich verwendete Spoolman-Version prüfen.

## 7.7.5 AssignTag migrieren

* [x] Spoolman-Verbindung voraussetzen
* [x] lokale Mapping-Speicherung entfernen
* [x] TagIdentity bestimmen
* [x] bestehenden `extra.tag` Treffer prüfen
* [x] gleiche Zuordnung idempotent behandeln
* [x] Zuordnung zu anderer Spule erkennen
* [x] Benutzerbestätigung bei Neu-Zuordnung
* [x] alten Spoolman-Wert entfernen
* [x] neuen Spoolman-Wert setzen
* [x] Update verifizieren
* [x] best-effort Rollback bei partieller Reassignment-Störung
* [x] nativen NTAG anschließend optional beschreiben
* [x] NDEF anschließend verifizieren
* [x] Bambu unverändert
* [x] OpenPrintTag unverändert
* [x] OpenTag3D unverändert
* [x] Unknown unverändert

## 7.7.6 RemoveTagAssignment migrieren

* [x] Spoolman-Verbindung voraussetzen
* [x] Tag anhand `extra.tag` suchen
* [x] eindeutige Spule bestimmen
* [x] `extra.tag` leeren
* [x] Serverantwort prüfen
* [x] nativen FilamentStation-Payload optional löschen
* [x] Payload-Löschung verifizieren
* [x] Bambu unverändert
* [x] OpenPrintTag unverändert
* [x] OpenTag3D unverändert
* [x] Unknown unverändert
* [x] lokales Mapping nicht mehr verwenden

## 7.7.7 Native Payload Konsistenz

* [x] `spoolman:<id>` gegen Spoolman-Zuordnung prüfen
* [x] konsistenter Fall
* [x] Payload-ID falsch
* [x] Spoolman-Zuordnung fehlt
* [x] Tag ist anderer Spule zugeordnet
* [x] Konflikt verständlich anzeigen
* [x] Spoolman als führende Quelle behandeln

## 7.7.8 Duplicate Handling

* [x] gleiche `extra.tag` ID auf mehreren Spulen erkennen
* [x] keine automatische Auswahl
* [x] Error Event
* [x] GUI-Meldung
* [x] Log mit Level ERROR/WARN
* [x] keine automatische Datenänderung

## 7.7.9 Legacy-Mapping-Migration

Übergang für bestehende Geräte:

* [x] alte Mapping-Dateien optional erkennen
* [x] nur bei verbundener Spoolman-Instanz migrieren
* [x] UID normalisieren
* [x] Spool-ID validieren
* [x] `extra.tag` des Zielspools prüfen
* [x] bestehende andere Zuordnung prüfen
* [x] eindeutige Einträge migrieren
* [x] Konflikte nicht überschreiben
* [x] vollständige Migration protokollieren
* [x] Datei nur nach erfolgreicher vollständiger Migration löschen
* [x] fehlende Mapping-Dateien normal behandeln

## 7.7.10 Lokale Mapping-Architektur entfernen

Nach erfolgreicher Migration:

* [x] Runtime-Zugriff auf `nfc-spools.json` entfernen
* [x] Runtime-Zugriff auf `bambu-tags.json` entfernen
* [x] Runtime-Zugriff auf `open-tags.json` entfernen
* [x] StorageCommands für NFC-Mappings entfernen
* [x] lokale Mapping-Repository-Klasse entfernen
* [x] tote Modelle entfernen
* [x] tote Tests entfernen/anpassen
* [x] `/mappings/printer-slots.json` nur beibehalten, falls weiterhin benötigt
* [x] Dokumentation aktualisieren

## 7.7.11 Tests

* [x] NTAG Assignment
* [x] Bambu Assignment
* [x] OpenPrintTag Assignment
* [x] OpenTag3D Assignment
* [x] Unknown Assignment
* [x] gleiche Spule
* [x] andere Spule
* [x] Duplicate
* [x] Remove NTAG
* [x] Remove Bambu
* [x] Spoolman offline
* [x] API Timeout
* [x] Extra Field fehlt
* [x] Extra Field falscher Typ
* [x] NDEF/Spoolman Inkonsistenz
* [x] Legacy Migration

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

* [x] Tag zuordnen ohne Spoolman blockieren
* [x] Tag-Zuordnung entfernen ohne Spoolman blockieren
* [x] Spulensuche ohne Spoolman blockieren
* [x] Import ohne Spoolman blockieren
* [x] Weight Update ohne Spoolman blockieren
* [x] AMS-Spoolman-Zuordnung ohne Spoolman blockieren
* [x] verständliche GUI-Meldung

Standardmeldung:

```text
Spoolman ist nicht verbunden.
Diese Funktion benötigt eine aktive Spoolman-Verbindung.
```

## 7.8.2 Pending Measurements entfernen

Die bisherige Implementierung aus 7.5 wird nicht mehr benötigt.

* [x] automatische Pending-Measurement-Erzeugung entfernen
* [x] Retry-Queue entfernen
* [x] `/queue/pending-measurements.json` nicht mehr verwenden
* [x] Fehler direkt anzeigen
* [x] manuellen Retry erlauben
* [x] alte Pending-Datei optional löschen
* [x] Tests anpassen

## 7.8.3 Persistenten Spoolman-Cache entfernen

Die bisher geplante Cache-Phase entfällt.

Nicht mehr verwenden:

```text
/cache/spools.json
/cache/filaments.json
/cache/vendors.json
```

Aufgaben:

* [x] vorhandene persistente Spool-Caches suchen
* [x] Filament-Caches suchen
* [x] Vendor-Caches suchen
* [x] Offline-Fallback entfernen
* [x] alte Dateien ignorieren/löschen
* [x] keine Spoolman-Daten von SD als Wahrheit verwenden

## 7.8.4 RAM-Cache

Optional erlaubt:

* [x] prüfen, ob RAM-Cache überhaupt nötig ist – derzeit nicht erforderlich
* [x] TTL definieren – entfällt ohne RAM-Cache
* [x] maximale Größe definieren – null Cache-Einträge
* [x] nach Write invalidieren – entfällt ohne RAM-Cache
* [x] bei Disconnect verwerfen – kein wiederverwendbarer Cache vorhanden
* [x] niemals Offline-Workflow ermöglichen

Dieser Unterpunkt darf entfallen, wenn kein RAM-Cache benötigt wird.

## 7.8.5 AppState

* [x] `SpoolmanUnavailable`
* [x] `SpoolmanReady`
* [x] `TagFieldUnavailable`
* [x] Buttons entsprechend aktivieren/deaktivieren
* [x] Home-Status aktualisieren
* [x] Settings weiterhin erreichbar

### Abnahmekriterien 7.8

* keine Spoolman-Schreiboperation wird offline gepuffert
* keine lokale Spoolman-Kopie wird als Offline-Datenquelle verwendet
* ohne Spoolman sind abhängige Funktionen eindeutig deaktiviert
* Wiederverbindung ermöglicht sofort wieder normalen Betrieb
* Build erfolgreich

---

# Phase 8 – Bambu und mehrere Drucker

## 8.1 Datenmodell

* [x] mehrere Drucker
* [x] stabile PrinterId
* [x] aktiver Drucker
* [x] Standarddrucker
* [x] aktives AMS je Drucker
* [x] Druckerstatus
* [x] Slots

## 8.2 Konfiguration

* [x] bambu.json
* [x] Name
* [x] Host
* [x] Seriennummer
* [x] LAN Access Code
* [x] enabled
* [x] Default
* [x] selected

## 8.3 BambuTask

* [x] Commands mit printerId
* [x] Events mit printerId
* [x] verbinden
* [x] trennen
* [x] testen
* [x] Status
* [x] AMS
* [x] Slots
* [x] External
* [x] Slotdaten schreiben
* [x] Reset
* [x] Reconnect

Hinweis: Das LAN-MQTT-Protokoll ist Community-Wissen (siehe
`docs/bambu-protocol.md`) und noch nicht an echter Druckerhardware
verifiziert. `BambuProtocol` (Parsing/Encoding) ist nativ getestet;
`BambuTask` (MQTT/TLS-Transport) erfordert einen realen Bambu-Drucker zur
Verifikation.

## 8.4 Druckerwechsel

* [x] Zustand sichern
* [x] wechseln
* [x] Header
* [x] AMS
* [x] Staging erhalten
* [x] stale Responses
* [x] printerId prüfen

Hinweis: `AppTask` führt jetzt eine echte `printerCollection`
(`models::PrinterStateCollection`), aktualisiert sie aus den Bambu-Events aus
Phase 8.3 und blendet Header/AMS-Updates für nicht mehr fokussierte Drucker
aus (stale Responses). Das Laden des Druckerbestands aus `bambu.json` sowie
automatisches Verbinden beim Wechsel sind nicht Teil von Phase 8.4 und bleiben
offen (Settings-GUI/Workflow-Phasen). Die UI-Rendering-Seite (`UiBridge.cpp`)
verwendet für Header/AMS-Anzeige weiterhin ihre eigenen Mock-Daten.

## 8.5 AMS-Zuordnung

* [x] Staging
* [x] Drucker
* [x] AMS
* [x] Slot
* [x] Spoolman-Spule
* [x] Daten
* [x] Command
* [x] Antwort
* [x] Reload
* [x] Ergebnis

Hinweis: `ConfigureSlot`/`ConfigureSlotFromStaging` validieren jetzt
Drucker/AMS/Slot/Spule, laden die Spule erneut (`pendingSlotAssignment`,
Stage `LoadingSpool`), senden `BambuCommand::AssignTray` mit
material/colorHex (Stage `WritingSlot`) und zeigen nach der Antwort einen
Reload (`RequestStatus`) plus Erfolg/Fehler-Dialog. Offen: Spoolman liefert
dem Staging-Endpunkt keine Nozzle-Temperaturen (nur der separate
Filament-Katalog kennt einen einzelnen Wert), daher bleiben
`nozzleTempMinC/MaxC` bei 0 -- es wurden bewusst keine Temperaturwerte
erfunden. Der tatsächliche Auslöser auf dem TraySelect-Bildschirm
(Bestätigen der Drucker-/AMS-/Slot-Auswahl) ist erst Phase 9.9 und
existiert im generierten EEZ-UI noch nicht.

## 8.6 GUI

* [x] hinzufügen
* [x] bearbeiten
* [x] löschen
* [x] Default
* [x] aktiv
* [x] testen
* [x] Access Code maskieren

Hinweis: `AppTask` führt jetzt `printerConfigs`
(`models::BambuConfigCollection`, Phase-8.2-Schema), lädt es beim SD-Mount
aus `/config/bambu.json` und persistiert es bei jeder Änderung (Speichern,
Löschen, Default/Aktiv/Enabled-Umschalten) über `StorageTask`. "Testen"
sendet jetzt ein echtes `BambuCommand::TestConnection` mit den aktuellen
Formulardaten. Die Maskierung des Access Codes (`printer_edit_mask`,
`"********"`) existierte bereits vollständig UI-seitig in
`UiBridge.cpp` und wurde nicht verändert -- sie funktioniert jetzt mit
echten statt Mock-Daten weiter. Offen: Der beim Booten geladene
Druckerbestand wird nicht in die (weiterhin mit drei Mock-Einträgen
vorbelegte) Listenansicht von `UiBridge.cpp` gespiegelt; nur Aktionen, die
bereits bestehende UI-Hooks nutzen (Save -> `ShowStatus value=202`,
Enable/Default/Aktiv/Löschen -> `UpdatePrinterList`), zeigen echte
Daten. Ein vollständiger Listen-Reload aus `printerConfigs` beim Boot
ist nicht Teil dieser Phase.

---

# Phase 9 – Integrierte Workflows

## 9.1 Hauptworkflow

* [x] Spoolman-Verbindung prüfen
* [x] Drucker auswählen
* [x] AMS auswählen
* [x] NFC/RFID lesen
* [x] `extra.tag` Lookup
* [x] Staging
* [x] Gewicht
* [x] Spoolman aktualisieren
* [x] Slot auswählen
* [x] Bambu konfigurieren
* [x] Ergebnis

Hinweis: Die meisten Teilschritte waren bereits aus früheren Phasen
vollständig (Spoolman-Gate, Druckerwechsel 8.4, NFC 5.x, `extra.tag` 7.7,
Staging/Gewicht 3.10/4.x/7.5, AssignTray-Backend 8.5). Die einzige echte
Lücke im Hauptworkflow war "Slot auswählen" → "Bambu konfigurieren": Der
`TraySelect`-Bildschirm hat im EEZ-Design keinen eigenen
Bestätigen-Button, daher tat Antippen eines Slots bisher nur eine
vorläufige, nie committete Auswahl. `trayTargetClicked` löst jetzt direkt
`ConfigureSlotFromStaging` aus (Tap = Auswahl + Commit, analog zur
Drucker-Auswahl anderswo in der App). Die Zuordnung zum externen Slot
("Extern"-Taste) wird von `AppTask`/`BambuTask` weiterhin korrekt
abgelehnt, da für den externen Slot kein verifiziertes Bambu-Kommando
existiert (siehe docs/bambu-protocol.md).

## 9.2 Native Tags

* [x] UID
* [x] extra.tag Lookup
* [x] NDEF-Konsistenz
* [x] Spule
* [x] Staging
* [x] wiegen
* [x] aktualisieren
* [x] AMS

Hinweis: UID/extra.tag-Lookup/NDEF-Konsistenz/Spule-Auflösung waren bereits
aus Phase 5.x/7.7 vollständig vorhanden. Die einzige echte Lücke war
"Staging": Das Lesen eines bereits zugeordneten Tags navigierte immer nur
zum `TagActionSelect`-Bildschirm ("Tag zuordnen"/"Tag-Zuordnung
entfernen"/"Löschen") – es gab keinen Pfad, der die aufgelöste Spule
tatsächlich ins Staging lud. `showNativeTagAction()` löst jetzt zusätzlich
`requestStagingSpool()` aus, sobald eine Spule aufgelöst ist. "wiegen"/
"aktualisieren"/"AMS" liefen bereits generisch über die vorhandene
Staging-Infrastruktur (Phase 4.x/7.5/8.5) und funktionieren dadurch jetzt
transitiv mit.

## 9.3 Bambu

* [x] UUID
* [x] extra.tag Lookup
* [x] Definition
* [x] importieren/zuordnen
* [x] wiegen
* [x] AMS
* [x] Originaltag unverändert

Hinweis: Die generische Tag-Pipeline (TagParserRegistry, TagIdentity,
TagWritePolicy) deckt UUID/extra.tag Lookup/Definition/importieren/zuordnen/
Originaltag-Schutz für Bambu-Tags bereits vollständig ab, unabhängig vom
Tag-Format: `BambuLabTagParser` liest die authentifizierte 16-Byte
Bambu-Tray-UUID aus Block 9 und `TagParserRegistry::parse` bevorzugt sie
gegenüber der NFC-UID als Identity (`TagIdentitySource::BambuUuid`), die
generische FindSpoolByTag-Auflösung nutzt diese Identity transparent für
alle Formate. `TagWritePolicy::capabilitiesFor` erlaubt für
`TagFormat::BambuLab` niemals `canWriteFilamentStationPayload`, wodurch
`assignmentEffect`/`removalEffect` immer `MappingOnly` liefern -- der
physische Bambu-Tag wird also strukturell nie beschrieben, auch nicht bei
Zuordnen/Entfernen. Import (`ImportTagDefinition`) und Zuordnen
(`AssignTag`/Spool-Picker) waren bereits generisch für BambuLab verdrahtet.

Der einzige tatsächliche Bug war "wiegen": Der Bambu-Tag-Zweig in
AppTask.cpp (currentTag.format == BambuLab, mappedSpool != 0) hat beim
Anzeigen des TagResult-Screens nur `command.spoolId`/`command.text`
gesetzt, aber nie die Spule ins Staging geladen. Die Wiege-Buttons auf
TagResult (`tag_result_quick_weight`/`tag_result_advanced_weight`) hängen
an `stagingActionClicked`, das ausschließlich `stagingState.spoolId`/
`stagingSpoolState` liest -- nie `command.spoolId`. Ohne Fix hätte
"Schnell/Erweitert wiegen" von einem bereits zugeordneten Bambu-Tag aus
lautlos die zuvor gestagte (falsche) Spule benutzt, oder gar keine. Fix:
im Bambu-Zweig zusätzlich `requestStagingSpool(ctx, event.requestId,
mappedSpool)` aufrufen (gegen `pendingStagingSpoolRequestId == 0`
abgesichert) -- exakt das gleiche Muster wie der Phase-9.2-Fix in
`showNativeTagAction`. Dadurch wird auch "AMS" (Slot-Zuordnung der
gestagten Spule via `ConfigureSlotFromStaging`) für zugeordnete
Bambu-Tags korrekt nutzbar, da diese ebenfalls von `stagingState.spoolId`
abhängt.

Bekannte, bewusst nicht in diesem Schritt behobene Lücke (nicht
Bambu-spezifisch, außerhalb 9.3-Scope): derselbe TagResult/Staging-Bug
besteht strukturell identisch auch für den "mapped spool"-Zweig von
OpenPrintTag/OpenTag3D (Phase 9.4/9.5) und Unknown (Phase 9.7) -- dort
noch nicht behoben, da außerhalb der explizit angeforderten Phase 9.3.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

## 9.4 OpenPrintTag

* [x] TagIdentity
* [x] extra.tag Lookup
* [x] Definition
* [x] Match
* [x] zuordnen
* [x] importieren
* [x] Staging

Hinweis: Wie bereits in 9.3 vermutet, bestand hier exakt derselbe
Staging-Bug wie beim Bambu-Zweig. TagIdentity/extra.tag Lookup/Definition/
Match/zuordnen/importieren waren bereits vollständig durch die generische
Pipeline abgedeckt: OpenPrintTag definiert laut Primärspezifikation
(docs/openprinttag.md) kein eigenes Identitätsfeld -- `TagParserRegistry`
verwendet daher konsistent die NFC-UID als Identity
(`TagIdentitySource::NfcUid`), die generische FindSpoolByTag-Auflösung und
"Match" (`findImportVendor`/`findImportFilament` in SpoolmanTask.cpp, per
`test_spoolman_catalog` abgedeckt) arbeiten format-agnostisch über
`models::TagDefinition`. Zuordnen/Importieren liefen bereits generisch
über `AssignTag`/`ImportTagDefinition`.

Fix ("Staging"): Der gemeinsame OpenPrintTag/OpenTag3D-Zweig in
AppTask.cpp (`currentTag.format == OpenPrintTag || OpenTag3D`) hat beim
Anzeigen von TagResult für eine bereits zugeordnete Spule ebenfalls nie
`requestStagingSpool()` ausgelöst -- identisch zum Bambu-Bug aus 9.3.
Behoben mit demselben Muster (`requestStagingSpool` gegen
`pendingStagingSpoolRequestId == 0` abgesichert). Da OpenPrintTag und
OpenTag3D denselben Codezweig teilen, behebt dieser Fix strukturell auch
das "Staging"-Item aus Phase 9.5 mit -- dort aber noch nicht abgehakt, da
9.5 separat angefordert werden muss.

Unverändert aus Phase 5.7: Der PN532 unterstützt kein ISO 15693/NFC-V, ein
reales OpenPrintTag-MK1 kann daher mit der aktuellen Hardware nicht
gelesen werden (dokumentiertes Hardwarelimit, kein Software-Bug).

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

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
