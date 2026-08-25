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

Nachtrag (Hardware-Test, 2026-08-22, nach Phase 9.9): mehrere reale Bugs
beim Zuordnen auf einen bereits belegten AMS-Slot gefunden und behoben --
(1) AMS-Nummer wurde 1-basiert (UI) statt 0-basiert (Bambu-Protokoll) an
den Drucker gesendet, sodass der Befehl eine nicht existierende AMS-Einheit
adressierte; (2) `nozzleTempMinC/MaxC` werden jetzt aus den
Spoolman-Filament-Extra-Feldern `bambu_temp_min`/`bambu_temp_max` gelesen
(Hinweis im Dialog, falls diese fehlen, statt weiter bei 0 zu bleiben);
(3) das bisher fehlende Feld `tray_info_idx` (Bambus interne
Filament-Profil-ID) wird jetzt aus dem Material abgeleitet gesendet, siehe
`BambuProtocol::bambuGenericTrayInfoIdx()`. Mit allen drei Fixes nimmt der
Drucker die Zuordnung sichtbar an.

Update (2026-08-22, weitere Untersuchung): die zunächst als "Drucker-
Firmware-Limitation" eingestufte Rücksetzung nach 3-5 Sekunden ist doch ein
behebbarer Fehler dieser App. Vergleich mit zwei Referenzprojekten
(Fire-Devils/filaman-bambulab-plugin, yanshay/spoolease) zeigte: unser
`ams_filament_setting`-Payload fehlte (a) das Feld `slot_id` und (b) ein
komplettes zweites Folgekommando `extrusion_cali_sel`, ohne das der
Drucker die Änderung offenbar nur provisorisch übernimmt und wieder
verwirft. Beide Fixes implementiert (`BambuProtocol::bambuBuildAmsFilamentSetting()`,
neu `bambuBuildExtrusionCaliSel()`, aufgerufen aus
`BambuTask::handleAssignTray()`); `PrinterState::nozzleDiameter` neu aus
`print.nozzle_diameter` in Statusberichten gelesen (Pflichtfeld für das
neue Kommando).

Update (2026-08-22, tatsächliche Ursache gefunden): auch mit beiden obigen
Fixes reagierte der Drucker weiterhin nicht. Externe Zweitmeinung
identifizierte die tatsächliche Ursache: aktuelle P1S-Firmware verlangt bei
Cloud-Kopplung eine kryptografische Kommandoverifikation, die ein reiner
`bblp`+Access-Code-MQTT-Client nicht erfüllt -- Lesen funktioniert, Schreiben
wird abgelehnt (`HMS_0500_0500_0001_0007`). **Fix: Developer Mode am Drucker
aktiviert** (kein Code-Fix). Per Hardwaretest bestätigt: Slot-Zuordnung
funktioniert jetzt. Zusätzlich zwei von derselben Zweitmeinung gefundene,
unabhängige Fehler behoben: fehlendes Diagnose-Logging für
`command`/`result`/`reason`/`err_code` in Kommando-Antworten (bisher
unsichtbar, `BambuTask::handleReportPayload()`), und ein
Adressierungsfehler in `bambuBuildExtrusionCaliSel()` (`tray_id` muss der
globale Index über alle AMS-Einheiten sein, nicht der lokale -- wirkte sich
bei nur einer AMS-Einheit nicht aus).

Dritter Punkt derselben Zweitmeinung ebenfalls umgesetzt: `AssignTray`
meldete bisher direkt nach erfolgreichem `publish()` Erfolg, obwohl das nur
bestätigt, dass der MQTT-Broker das Paket angenommen hat, nicht dass der
Drucker es anwendete. Neu: `PrinterConnection::pending`
(`PendingTrayAssignment`) verzögert Erfolgsmeldung und
Spoolman-Zuordnung (`spoolId`), bis eine nachfolgende Drucker-Telemetrie
die erwarteten `tray_type`/`tray_color`-Werte bestätigt
(`checkPendingTrayAssignment()`, aus `handleReportPayload()`), oder meldet
nach `kBambuAssignConfirmTimeoutMs` (8s) explizit einen Fehler
(`serviceConnections()`). Details siehe docs/bambu-protocol.md.

Nachtrag (2026-08-22, Nutzerwunsch): die Wartezeit auf diese
Drucker-Bestätigung zeigte bisher nur einen statischen Text ohne
Zeitangabe. Neuer, auf 1/s gedrosselter `AppEventType::BambuAssignProgress`
(`BambuTask::serviceConnections()`) treibt jetzt einen echten Countdown im
bereits vorhandenen, zuvor nie bewegten Fortschrittsbalken
(`overlayProgress`) sowie einen "noch N s"-Text. Details siehe
docs/bambu-protocol.md.

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

Nachtrag (2026-08-22, zwei Nutzer-gemeldete Bugs behoben):

* Der Home-Screen aktualisierte sein Staging-Widget (`objects.home_staging`)
  nicht sofort, wenn eine Spule per "Spule auswählen" ins Staging
  übernommen wurde. `UiBridge::processUiCommand()`s `UpdateStaging`-Handler
  rief bisher nur `updateStagingContent()` auf, nie `updateHomeContent()` --
  Home blieb dadurch veraltet, bis irgendein unabhängiges Ereignis es zufällig
  neu zeichnete. Beide `UpdateStaging`-Zweige (leeren und befüllen) rufen
  jetzt zusätzlich `updateHomeContent()` auf.
* Klick auf "Staging" bei leerem Staging navigierte immer erst zum
  Status-Screen (`StagingDetails`), der dort nichts anzuzeigen hatte.
  `AppTask` verfolgt jetzt mit `stagingSpoolId` (aktualisiert an allen drei
  Stellen, die `UpdateStaging` senden) eigenständig, ob eine Spule gestagt
  ist, und überspringt `StagingDetails` bei leerem Staging direkt zu
  `StagingActions` -- "Zurück" von dort geht dann ebenso direkt zu Home
  statt zum übersprungenen Status-Screen.

Nachtrag (2026-08-22, Home-Screen-Redesign auf Nutzerwunsch):

* **AMS-Buttons**: kein Text mehr ("AMS N X/4"), stattdessen 4 farbige
  Slot-Bereiche je Button (Slot-Farbe wenn belegt, sonst neutral grau) und
  ein farbiger Rand (statt Volltonfarbe) am aktuell gewählten AMS.
* **Drucker-Button unten links entfernt**: dieselbe Funktion
  (Druckerauswahl) ist bereits über die Titelleiste erreichbar.
* **Tray-Info** (home_tray_1-4, home_external): zeigt jetzt Material
  (echt, vom Drucker), Restgewicht, Spoolman-ID (ohne #) und K-Faktor
  statt bisher nur "Slot N\n<Material>". Restgewicht/Spoolman-ID/K-Faktor
  sind auf Nutzerwunsch vorerst Mockdaten (kein Spoolman-Abgleich für
  beliebige AMS-Slots vorhanden, nur für von dieser App zugeordnete Spulen
  theoretisch möglich) -- echte Anbindung ist eine spätere Aufgabe. Der
  "Slot N"/"Extern"-Titel entfällt, die feste Bildschirmposition zeigt
  weiterhin, welcher Slot gemeint ist.
* **Titelleiste**: zeigt nur noch Druckername + Verbindungsstatus (vorher
  zusätzlich "AMS N"/"kein AMS") -- welches AMS gerade betrachtet wird, ist
  jetzt am farbigen Rand des AMS-Buttons selbst erkennbar. Betrifft
  `updateHeaders()`/`updateAmsOverview()`, wirkt sich damit auf die
  gemeinsame Titelleiste aller Screens aus (`setAllHeaderTexts()`).

Nachtrag (2026-08-22, Nutzer-Feedback nach erstem Entwurf):

* **AMS-Farbflächen**: statt dynamisch per Code erzeugter Overlay-Quadrate
  (`createHomeAmsSlotSquares()`, wieder entfernt) färbt `updateHomeContent()`
  jetzt vier vom Nutzer direkt im EEZ-Projekt angelegte Container-Objekte
  pro AMS-Button ein (`home_ams_<N>_1..4`, `bg_opa`/`bg_color` statt
  Position/Größe -- Layout kommt vollständig aus dem EEZ-Export).
* **K-Faktor-Format** korrigiert auf "K (#.###)" (ein Digit, drei
  Nachkommastellen -- passend zur Größenordnung echter Bambu-K-Faktoren)
  statt zuvor "K (XX.X)".
* **Farbkonstanten**: alle 24 in `UiBridge.cpp` verwendeten `0xRRGGBB`-Werte
  in benannte, kommentierte `constexpr`-Konstanten (`kColorPrimaryBlue`,
  `kColorDangerRed`, `kColorWarningAmber`, ...) am Dateianfang ausgelagert,
  jeder bisherige Streuvorkommen-Aufruf ersetzt.
* `objects.home_bottom_printers` wurde vom Nutzer im EEZ-Projekt selbst
  entfernt (nicht mehr nur code-seitig ausgeblendet); der entsprechende
  Verweis in `UiBridge.cpp` wurde entfernt, da das Objekt nicht mehr
  existiert.

### UI-Architektur-Refactor (2026-08-23, Nutzerwunsch, mehrteilig)

Ziel: (1) durchgängig EEZ-Styles/-Themes statt C++-Farbkonstanten, spätere
Unterstützung für Light/Dark-Mode; (2) GUI-Layout ausschließlich über EEZ
Studio änderbar, kein dynamisch in C++ erzeugtes/positioniertes Layout mehr;
(3) Labels, die zur Laufzeit per `styleLabelButton()` zu Buttons gemacht
werden, sollen echte EEZ-Buttons werden.

Analyse ergab: `styles.c`/`screens.c` haben bereits eine vollständige,
kaum genutzte Style-/Theme-Infrastruktur (`theme_colors[N][4]`,
`change_color_theme(index)`, ein Style `ButtonPrimary`) -- der native
Mechanismus für Light/Dark existiert also schon, nur mit einer einzigen
Theme-Zeile befüllt. Von 155 klickbaren Controls waren nur 26 echte
`LVGLButtonWidget`, 124 waren `LVGLLabelWidget`, die `styleLabelButton()`
optisch/funktional zu Buttons macht (verifiziert durch Abgleich jedes
`styleLabelButton()`-Ziels -- direkt und über alle Loop-Arrays -- gegen den
tatsächlichen Objekttyp in `screens.c`; 9 weitere klickbare, aber nie
`styleLabelButton()`-behandelte Labels wie `tag_action_header` bleiben
bewusst Labels, da sie reine, unauffällige Tap-Zonen sind, keine
Button-Attrappen).

Erster Umsetzungsschritt (nur der Label→Button-Teil, Styles/Themes und die
Migration der übrigen dynamischen Layout-Funktionen sind noch offen):
`scripts/convert_label_buttons.py` wandelt die 124 `LVGLLabelWidget` in
`FilamentStation.eez-project` direkt im JSON in `LVGLButtonWidget` um
(Text wandert in ein neues, zentriertes Kind-Label, `useStyle` wird auf
`ButtonPrimary` gesetzt). Skript unterstützt `--only <ids>` zum Pilotieren
und schreibt vor jedem `--apply` ein Backup. An 2 Widgets pilotiert und per
`cli-anything-eez-studio project validate`/`project widgets` gegengeprüft,
bevor alle 124 auf einmal umgewandelt wurden (`project validate` meldet
danach weiterhin `"valid": true`).

Re-Export durch den Nutzer bestätigt (`lv_button_create()` + `add_style_button_primary()`
+ zentriertes Kind-Label in `screens.c` für alle 124 verifiziert, Build
0 Warnungen). Anschließend `UiBridge.cpp` angepasst:
* `styleLabelButton()`: die jetzt tote "Label→Caption-Kind"-Injektion
  (galt für 0 verbleibende Ziele) entfernt; setzt nur noch
  Clickable-Flag + Farbe (EEZ markiert diese Buttons nicht selbst als
  klickbar).
* `setControlText()`: rief bisher immer `centerButtonLabel()` auf, das
  Position/Größe des Kind-Labels bei jedem Text-Update neu berechnete --
  bei den 11 live editierten Feldern (Spoolman-/Drucker-Einstellungen,
  Text ändert sich bei jedem Tastendruck) reproduzierte das denselben
  Sprung-Bug wie zuvor bei den Headern. `centerButtonLabel()` komplett
  entfernt (jetzt unnötig: EEZ definiert Position/Ausrichtung des
  Kind-Labels bereits korrekt, LVGL zentriert ein content-großes Label bei
  Textänderung automatisch neu). `setHeaderText()` (Vorrunden-Fix) dadurch
  redundant geworden und wieder entfernt, `setAllHeaderTexts()` nutzt
  wieder `setControlText()`.

Nachtrag (2026-08-23, Header-Vereinheitlichung, Nutzerwunsch): Header sollen
einheitlich zeigen: [3D-Drucker-Icon] [Druckername] [Drucker-Verbindungs-Icon]
... [WLAN-Icon] [Spoolman-Icon]. Neues Skript
`scripts/add_header_status_icons.py` fügt jedem der 23 Header genau 3 neue
`LVGLImageWidget`-Kinder hinzu (`{header}_printer`, `{header}_wifi`,
`{header}_spoolman`, rechtsbündig, 8px Rand, 8px Abstand). Bewusst nur je
ein Bild-Objekt pro Status statt eines connected/disconnected-Paares --
C++ tauscht das `lv_img_dsc_t` per `lv_image_set_src()` zur Laufzeit aus
(Bild-Assets `conneced W`/`disconneced W`, `WIFI connected W`/
`WIFI disconnected W`, `Spoolman connected W`/`Spoolman disconneced`
existierten bereits im Projekt, waren aber nirgends platziert). Pilotiert
an `home_header`, dann alle 23 angewendet, `project validate` weiterhin
`"valid": true` (Widget-Count 519→588, exakt 23×3). Die 7 `tag_*_header`
sind (anders als die 124 Buttons) weiterhin `LVGLLabelWidget` -- bewusst
nicht konvertiert (siehe oben), das Skript akzeptiert beide Typen als
Elternobjekt für die neuen Icons.

Re-Export durch den Nutzer bestätigt; ein Zwischenfall dabei: `settings_header`
verlor beim ersten Re-Export seine 3 Icons wieder (vermutlich hatte der
Nutzer diesen Screen offen, als das Skript lief -- Speichern in EEZ Studio
überschrieb den Stand nur für diesen einen Screen). Mit
`--only settings_header --apply` gezielt nachgezogen, erneuter Re-Export
bestätigt.

`UiBridge.cpp` fertig verdrahtet:
* `setAllHeaderTexts()`-Aufrufe zeigen nur noch den Druckernamen (kein
  "| Verbindungsstatus"-Suffix mehr -- übernimmt jetzt das Icon).
* Neue Funktion `updateHeaderStatusIcons()`: tauscht für alle 23 Header
  die drei `_printer`/`_wifi`/`_spoolman`-Icons per `lv_image_set_src()`
  auf den jeweils passenden Bild-Deskriptor (`img_conneced_w`/
  `img_disconneced_w`, `img_wifi_connected_w`/`img_wifi_disconnected_w`,
  `img_spoolman_connected_w`/`img_spoolman_disconneced`) -- Quellen:
  `printerEntry(currentPrinterId)->connectionState`, `currentNetworkState`,
  `spoolmanAppState` (über die bereits existierende
  `models::spoolmanOperationsAvailable()`). Aufgerufen aus `updateHeaders()`,
  `updateAmsOverview()` sowie den `UpdateNetworkStatus`-/
  `UpdateSpoolmanState`-Command-Handlern, damit alle drei Icons unabhängig
  vom gerade fokussierten Screen aktuell bleiben.
* Nebenbei behoben: der Nutzer hatte in EEZ Studio `settings_settings`
  (den redundanten, selbstreferenzierenden Einstellungen-Button auf dem
  Einstellungen-Screen selbst) entfernt -- die zwei verwaisten
  `objects.settings_settings`-Referenzen in `UiBridge.cpp` (ein
  `bindClick()` und ein bereits totes, auskommentiertes Array) entfernt.
* Zwei weitere Re-Export-Zwischenfälle behoben: `settings_header` verlor
  seine 3 Icons ein zweites Mal, diesmal weil auf `SCR_SETTINGS_HOME` 3
  verwaiste, falsch benannte Icon-Objekte lagen (kollidierten mit
  `select_header`s echten Icon-Identifiern -- vermutlich ein
  Copy-Paste-Rest des Nutzers). Direkt im `.eez-project` bereinigt
  (verwaiste Objekte entfernt, `settings_header`-Icons per
  `add_header_status_icons.py --only settings_header` neu ergänzt),
  `project validate` bestätigt, danach vom Nutzer re-exportiert.

Nachtrag (2026-08-23, Spoolman-Einstellungen-Bug, Nutzer-Report): "Testen"
verband sich nach einer Hostname-Änderung zum ursprünglichen statt zum
gerade eingegebenen Server. Ursache: Das Textfeld-Editor-Overlay
(`spoolmanEditor`/`spoolmanKeyboard`, y=0-220) hat keinen Backdrop und
blockiert die tiefer sitzenden Test-/Speichern-Buttons (y=264) nicht --
wurde eine neue URL eingetippt, aber die Bildschirmtastatur nicht per
"OK" bestätigt, bevor Testen/Speichern angetippt wurde, verwendeten beide
Aktionen den alten, noch nicht in `spoolmanDraft` übernommenen Wert
(`spoolmanSettingsFromDraft()` liest korrekt aus dem Draft, nur der Draft
selbst war noch nicht aktualisiert). Fix: neue Funktion
`commitSpoolmanEditorIfOpen()`, aufgerufen aus `spoolmanActionClicked()`
(Testen/Speichern) vor dem eigentlichen Senden der Aktion -- übernimmt den
gerade sichtbaren Editor-Text automatisch, unabhängig davon ob "OK"
gedrückt wurde. "Abbrechen" bleibt bewusst unverändert (verwirft
Änderungen weiterhin korrekt, das war laut Nutzer schon richtig).

Nachtrag (2026-08-23, Named Styles je Farbrolle, Nutzerwunsch erledigt):
neues Skript `scripts/add_button_role_styles.py` legt zwei neue benannte
Theme-Farben (`ButtonNeutralActive` #455a64, `ButtonDangerActive` #c62828,
Werte identisch zu den bisherigen `kColorNeutralGrey`/`kColorDangerRed`
C++-Konstanten) sowie zwei neue Styles `ButtonNeutral`/`ButtonDanger` an
(gleiche Struktur wie das bestehende `ButtonPrimary`, referenzieren die
Theme-Farben statt Rohwerten) und setzt `useStyle` auf den 15
Neutral-/6 Danger-Buttons (Audit siehe Skript-Kommentar; `home_active_ams`/
`home_ams_4` bewusst ausgenommen, deren Farbe ist vollstaendig laufzeit-/
datengetrieben). `project validate` weiterhin `"valid": true`.
`UiBridge.cpp::styleLabelButton()` vereinfacht: nimmt keinen Farbparameter
mehr entgegen, setzt nur noch Clickable-Flag/Ausrichtung/Radius -- alle 35
Aufrufstellen mit explizitem `kColorXxx`-Argument bereinigt (Skript, um
Abtippfehler zu vermeiden). Build (0 Warnungen), 52 native Tests gruen.
**Vor dem Flashen muss der Nutzer erneut in EEZ Studio exportieren** --
bis dahin wuerden alle Buttons wieder einheitlich blau erscheinen (C++
ueberschreibt die Farbe nicht mehr, die alten generierten Dateien kennen
`add_style_button_neutral/danger()` noch nicht).

Nachtrag (2026-08-23, Restliche Feedback-Punkte, Nutzerwunsch erledigt):
5 gemeldete Punkte behoben.

1. **Laufzeit-Farbfehler bei `settings_spoolman`/`staging_details_quick_weight`**:
   root cause war eine zweite, von der `styleLabelButton()`-Vereinfachung
   nicht erfasste Funktion `setLabelButtonAvailable()` (steuert den
   verfuegbar/gesperrt-Zustand von 20 Buttons), die weiterhin per Parameter
   eine Farbe entgegennahm und `bg_color`/`text_color` hart ueberschrieb --
   das uebermalte in jedem Refresh den frisch zugewiesenen EEZ-Style. Fix:
   Funktion nimmt keinen Farbparameter mehr entgegen, schaltet stattdessen
   nativ `LV_STATE_DISABLED` (`lv_obj_add_state`/`remove_state`); die
   DISABLED-Variante jedes Button-Styles (`ButtonPrimary`/`Neutral`/`Danger`)
   ist in EEZ bereits mit `LV_PART_MAIN | LV_STATE_DISABLED` registriert
   (siehe `styles.c`), das Umschalten greift also ohne weiteres Zutun. Ein
   weiterer manueller `bg_color`-Override direkt bei `tag_legacy_erase`
   (gleiche Baustelle, war nie durch `setLabelButtonAvailable()` gelaufen)
   ebenfalls durch den Aufruf der Funktion ersetzt. Alle ~20 Aufrufstellen
   in `UiBridge.cpp` angepasst.
2. **`home_status` (SCR_HOME) entfernt**: Info (NFC/Spoolman/WLAN-Status)
   ist jetzt über die Header-Status-Icons abgedeckt. Widget per Skript aus
   dem `.eez-project` geloescht, der zugehoerige tote Code-Block in
   `updateHomeContent()` (baute `statusText` und rief `setButtonText()`
   darauf auf) von Hand entfernt.
3. **`select_bottom_status` (SCR_PRINTER_SELECT)** und **die 7
   `tag_*_header`** (SCR_TAG_ACTION_SELECT/TAG_REVIEW/TAG_WRITE/TAG_RESULT/
   TAG_DEFINITION_IMPORT/TAG_LEGACY/TAG_UNKNOWN) waren die einzigen
   verbliebenen `LVGLLabelWidget`, die wie ein Button benutzt werden --
   urspruenglich bei der 124er-Migration bewusst ausgenommen ("plain
   tappable status region"), Einschaetzung vom Nutzer korrigiert. Neues
   Skript `scripts/convert_remaining_labels.py` konvertiert beide Faelle zu
   `LVGLButtonWidget`; bei den 7 Headern werden die bereits vorhandenen
   3 Status-Icon-Kinder (aus `add_header_status_icons.py`) erhalten, statt
   wie bei der einfachen Konvertierung durch ein neues `children`-Array
   ersetzt zu werden (sonst waeren die Icons stillschweigend geloescht
   worden). `useStyle` = `ButtonPrimary`, `project validate` weiterhin
   `"valid": true`, per objID-Cross-Check ueber `project widgets`
   verifiziert (Caption-Label + alle 3 Icons als Kinder vorhanden).

Build (0 Warnungen), 52 native Tests gruen.
**Vor dem Flashen muss der Nutzer erneut in EEZ Studio exportieren** --
bis dahin fehlen `select_bottom_status`/die 7 Tag-Header als echte Buttons
und `home_status` waere weiterhin als generiertes Objekt vorhanden.
Re-Export durch den Nutzer bestaetigt, Build + 52 Tests danach erneut
gruen, geflasht.

Nachtrag (2026-08-23, zwei weitere Nutzer-Reports, erledigt):

1. **SCR_PRINTER_SELECT liess nur 3 statt 4 Drucker waehlen**: Root Cause
   war rein im Layout -- die Seite hatte tatsaechlich nur 3 Zeilen
   (`select_printer_1/2/3`), `updatePrinterList()` in `UiBridge.cpp` iterierte
   entsprechend nur ueber 3 Buttons, obwohl `printerEntries` intern schon
   ein `std::array<PrinterUiEntry, 4>` ist (die Drucker-Verwaltung auf
   SCR_PRINTER_SETTINGS hat bereits 4 Zeilen, nur die Auswahlseite hinkte
   hinterher). Neues Skript `scripts/add_fourth_printer_row.py` verkleinert
   die 3 bestehenden Zeilen von 56px auf 42px Hoehe (Abstand 78/124/170 statt
   78/137/196) und dupliziert Zeile 3 als `select_printer_4` bei top=216 --
   passt mit 10px Abstand vor die untere Buttonleiste (top=268), genau wie
   vorher zwischen Zeile 3 und der Leiste. `updatePrinterList()`s
   `buttons`-Array und `bindClick(objects.select_printer_4, printerClicked, 4)`
   ergaenzt.
   Dabei zusaetzlich aufgefallen und mitbereinigt: `select_bottom_status`
   ("Drucker verwalten") wurde trotz der laengst erfolgten Umstellung auf
   einen echten EEZ-Button (`useStyle: ButtonPrimary`) in `updatePrinterList()`
   weiterhin bei jedem Refresh hart per `lv_obj_set_style_bg_color()` usw.
   eingefaerbt -- toter Code aus der Zeit, als es noch ein Label war, jetzt
   entfernt (Farbe kommt nur noch aus dem Style).
2. **7 fehlende `_settings`-Zahnrad-Buttons auf allen SCR_TAG_*-Screens**
   (`tag_action_settings`, `tag_review_settings`, `tag_write_settings`,
   `tag_result_settings`, `tag_definition_import_settings`,
   `tag_legacy_settings`, `tag_unknown_settings`): identische Form wie das
   bereits konvertierte `device_settings_settings` (Label, `text: ""`,
   412/0/68/40), waren aber in der urspruenglichen 124er-Migration nicht
   erfasst, weil sie ueber `bindClick(..., settingsClicked)` direkt gebunden
   werden statt ueber `styleLabelButton()` zu laufen -- fielen dadurch aus dem
   damaligen Audit heraus. `scripts/convert_remaining_labels.py` um diese 7
   erweitert und konvertiert (keine Kinder zu erhalten, einfache Konvertierung
   wie `select_bottom_status`).

`project validate` durchgehend `"valid": true` (Widget-Count 586 -> 595 -> 604
ueber beide Skript-Laeufe). Re-Export durch Nutzer bestaetigt, Build (0
Warnungen) + 52 native Tests danach gruen, generierte `screens.c` per Grep
verifiziert (`select_printer_4`/`tag_action_settings` jetzt `lv_button_create`),
geflasht.

Nachtrag (2026-08-23, Home-Screen Multicolor-Filamentanzeige, Nutzerwunsch
erledigt): Farbe 1 eines Filaments faerbt weiterhin den Button selbst
(`home_tray_1..4`/`home_external`/`home_staging`, unveraendert), Farbe 2 und
3 bekommen jetzt je einen eigenen kleinen Container-Swatch daneben statt wie
bisher verloren zu gehen -- analog zu den vier AMS-Slot-Farbcontainern.

Der Nutzer hatte dafuer bereits 11 von 12 benoetigten Containern
(`home_tray_1_1/_2` ... `home_staging_1/_2`) selbst in EEZ Studio angelegt
(screen-level Geschwister von SCR_HOME, exakt wie die AMS-Container, nicht
in den Button verschachtelt); nur `home_tray_4_2` fehlte (Copy-Paste-
Versehen). Per Skript ergaenzt (`scripts/add_missing_home_tray_4_2.py`,
Klon von `home_tray_4_1` inkl. Position). Dabei kollidierte der Nutzer's
parallele eigene Ergaenzung desselben Objekts mit dem Skript-Lauf (gleiches
bekanntes Muster wie bei frueheren Merge-Konflikten in dieser Session) --
`home_tray_4_2` existierte kurzzeitig doppelt im Export
(`duplicate member` Compilerfehler), Duplikat identifiziert und entfernt.

`createHomeColorStrips()`/`updateHomeColorStrips()` (Laufzeit-Overlay-Kreise,
bei jedem Refresh neu positioniert) ersatzlos entfernt, durch
`updateHomeColorSwatches()` ersetzt (direktes `lv_obj_set_style_bg_color()`
auf den EEZ-Containern, wie beim AMS-Vorbild). `updateTrayButton()` nimmt
jetzt die zwei Swatch-Objekte als Parameter statt eines Gruppenindexes.

**Bekannte Einschraenkung:** `TrayUiEntry::colorHex` ist ein einzelnes
Hex-Feld (Bambu-Protokoll liefert pro AMS-Fach nur eine Farbe) -- die neuen
Swatches bleiben bei `home_tray_1..4`/`home_external` deshalb vorerst leer
(transparent), bis echte Mehrfarb-AMS-Daten verfuegbar sind. Bei
`home_staging` funktioniert es sofort, da `stagingState.colorRgb`/
`colorCount` (Spoolman-Spulendaten) bereits bis zu 3 Farben tragen.

`project validate` durchgehend `"valid": true`. Re-Export durch Nutzer
bestaetigt (zweimal, wegen der Duplikat-Kollision), Build (0 Warnungen) +
52 native Tests danach gruen, geflasht.

Nachtrag (2026-08-23, CMP_TRAY_CARD-Komponente, Nutzerumbau + Code-Anpassung):
Nutzer hat die Home-Screen-Tray-Buttons in EEZ Studio komplett auf eine
wiederverwendbare Komponente umgestellt (`CMP_TRAY_CARD`, 5 Instanzen:
`home_tray_1..4`, `home_tray_external` -- ersetzt das bisherige
`home_external`). EEZ generiert Komponenten ueber eine gemeinsame
`create_user_widget_cmp_tray_card()`-Funktion mit Index-Offset
(`((lv_obj_t**)&objects)[startWidgetIndex + N]`); jede Instanz bekommt ihre
Sub-Widgets als `objects.home_tray_<N>__<name>` (doppelter Unterstrich):
`__tray` (das eigentliche `LVGLButtonWidget`, `home_tray_N` selbst ist nur
noch ein transparenter Wrapper), `__label`, `__color_1`/`__color_2` (loesen
die alten `home_tray_N_1/_2`-Swatches ab, gleiche Rolle), neu
`__spoolmanager_id_container`/`__spoolmanager_id` (Spoolman-ID-Badge) und
`__nozzle_icon` (Duesen-Icon fuer "Filament aktiv").

`UiBridge.cpp` entsprechend angepasst:
* `bindClick()` und alle Farb-/Text-Setter zielen jetzt auf `__tray` statt
  auf den Wrapper.
* `updateTrayButton()` um vier Parameter erweitert (`label`,
  `spoolIdContainer`, `spoolIdLabel`, `nozzleIcon`); Spoolman-ID (weiterhin
  Mockdaten wie Restgewicht/K-Faktor, siehe bisherige Kommentare) wandert
  aus dem kombinierten Slot-Text in das eigene `spoolmanager_id`-Label um,
  Container + Label werden bei leerem Fach versteckt (`LV_OBJ_FLAG_HIDDEN`).
* Neu: "Duese aktiv"-Anzeige (`nozzle_icon`), ebenfalls Mockdaten -- da es
  im Datenmodell noch keine echte "aktuell druckende Duese"-Zuordnung gibt,
  gilt das erste belegte Fach je AMS (bzw. das externe Fach) als Mock-Regel
  als aktiv. Icon-Variante (`img_3_d_printer_nozzle` dunkel /
  `img_3_d_printer_nozzle_w` hell) richtet sich nach der Helligkeit der
  Button-Hintergrundfarbe -- dafuer `setButtonColors()`s bisher inline
  berechnete Luma-Formel in eine wiederverwendbare `isLightBackground()`
  extrahiert statt sie zu duplizieren.

`project validate` nicht anwendbar (reine Nutzer-Umstrukturierung in EEZ
Studio, kein Skript-Edit dieses Mal). Build (0 Warnungen) + 52 native Tests
gruen, geflasht.

Nachtrag (2026-08-23, CMP_STAGING_CARD-Komponente, Nutzerumbau + Code-
Anpassung): Nutzer hat auch den Staging-Button auf SCR_HOME auf eine
Komponente umgestellt (`CMP_STAGING_CARD`, eine Instanz, Name `staging`).
Gleiches Namensschema wie bei CMP_TRAY_CARD: `objects.staging` ist nur
noch der transparente Wrapper, `objects.staging__staging` das eigentliche
`LVGLButtonWidget` (Klick-Ziel), `objects.staging__label` der Spoolinfo-
Text. Abweichend vom Tray-Card-Wortlaut des Nutzers heissen die beiden
Zusatzfarb-Container in der generierten Komponente tatsaechlich
`staging__color_3`/`staging__color_4` (nicht `_1`/`_2` -- vermutlich um
Bezeichner-Kollisionen mit CMP_TRAY_CARD innerhalb desselben EEZ-Projekts
zu vermeiden), `objects.staging__spoolmanager_id_container`/
`__spoolmanager_id` wie beim Tray-Card, und neu `objects.staging__staging_label`
(rotiertes "STAGING"-Seitenlabel, `STAGING_LABEL` im Projekt) -- dessen
Textfarbe laut Nutzervorgabe je nach Helligkeit der Button-Hintergrundfarbe
umspringt (schwarz auf hell, weiss auf dunkel), separat gesetzt, da es
nicht Kind-Label des Buttons ist und daher nicht automatisch von
`setButtonColors()`s bestehender `buttonLabel()`-Logik erfasst wird.

Anders als bei den AMS-Faechern ist `stagingState.spoolId` bereits eine
echte (nicht gemockte) Spoolman-ID, ueber den Spulen-Picker zugeordnet --
wird direkt angezeigt statt wie beim Tray-Card eine Mock-ID zu erfinden.
Nur der K-Faktor bleibt Mockdaten (kein Feld in `UiStagingSummary`,
analog zum Tray-Card-Mock).

`project validate` nicht anwendbar (reine Nutzer-Umstrukturierung in EEZ
Studio). Build (0 Warnungen) + 52 native Tests gruen, geflasht.

**Noch offen / nächste Schritte:**
* Zweite Theme-Zeile (Dark) + Umschalt-UI + Persistierung
  (`StorageDocumentType::Ui`, `/config/ui.json` existiert bereits als
  Ablageort).
* Verbleibende dynamische Layout-Funktion `createStagingTableDecoration()`
  durch echte EEZ-Objekte ersetzen, analog zu den AMS-Containern.
* Mehrfarb-AMS-Daten vom Bambu-Protokoll (`tray->colorHex` derzeit nur 1
  Farbe) waeren Voraussetzung dafuer, dass `color_1`/`color_2` bei echten
  AMS-Faechern sichtbar werden.
* Echte "aktuell druckende Duese"-Zuordnung im Datenmodell nachruesten,
  um das Mock-Verhalten von `nozzle_icon` durch echte Daten zu ersetzen.

Nachtrag (2026-08-23, Spoolman-ID ueber Drucker-Telemetrie aufloesen,
Nutzerwunsch erledigt): Bisher lebte die Zuordnung "Slot -> Spoolman-Spule"
nur lokal im ESP32-RAM (`checkPendingTrayAssignment()` setzte `spoolId`
einmalig bei Bestaetigung einer eigenen `AssignTray`) -- eine Neuverbindung
oder ein Neustart verlor diese Information komplett, obwohl der Drucker
weiterlief. Neue eigene Konvention (kein Bambu-Standardfeld): beim
Zuordnen wird das Feld `tray_id_name` mit `"SM:<spoolmanId>"` beschrieben
(`BambuProtocol::bambuBuildAmsFilamentSetting()`, ueber ein neues
`BambuTrayFilament::spoolmanId`, von `BambuTask::handleAssignTray()` aus
`command.spoolId` befuellt); der Drucker "versteht" das Feld nicht, gibt es
aber unveraendert bei jedem folgenden Statusbericht zurueck.
`BambuProtocol::bambuApplyReport()` parst es (neue `parseSpoolmanTrayIdName()`,
anonymous namespace) zurueck in `PrinterSlotStateData::spoolId` -- **nur
wenn das Feld im Report vorhanden ist** (fehlt es komplett, bleibt ein
bekannter Wert unangetastet, Absicherung gegen Reports ohne dieses Feld);
ist es vorhanden, aber nicht im `"SM:<Zahl>"`-Format, wird `spoolId` auf 0
("unbekannt") zurueckgesetzt. `UiBridge.cpp::updateTrayButton()` zeigt bei
belegtem Fach mit `spoolId == 0` jetzt `?` im `spoolmanager_id`-Label statt
wie zuvor eine erfundene Mock-ID -- die Spoolman-ID ist damit fuer AMS-
Faecher keine Mockdaten mehr, nur Restgewicht/K-Faktor/"Duese aktiv"
bleiben es (siehe oben). Betroffene Doku-Kommentare in
`services/BambuProtocol.h`/`models/PrinterState.h` und
`docs/bambu-protocol.md` (Anfrage- und Statusbericht-Abschnitte)
aktualisiert -- der alte "Drucker kennt keine Spoolman-IDs, spoolId wird
nie veraendert"-Vertrag gilt so nicht mehr.

4 neue Tests in `test_bambu_protocol` (Encoding `tray_id_name`,
Decoding gueltiger/ungueltiger Werte, externes Fach); bestehender Test
`testApplyReportNeverTouchesSpoolId` in
`testApplyReportPreservesSpoolIdWhenTrayIdNameFieldAbsent` umbenannt (Test
selbst unveraendert gueltig -- sein JSON enthaelt `tray_id_name` nicht,
genau der Fall, den die neue, engere Zusicherung noch abdeckt). 56 native
Tests gruen (vorher 52). Kein `.eez-project`-Eingriff, kein Re-Export
noetig. Build (0 Warnungen), geflasht.

Nachtrag (2026-08-23, `tray_id_name`-Lesepfad per Hardwaretest widerlegt
und zurueckgebaut): der Nutzer berichtete, dass die Spoolman-ID nach einem
ESP32-Neustart wieder "?" zeigt, statt aus der Drucker-Telemetrie
aufgeloest zu werden -- Verdacht: Auswertungsfehler speziell beim
Pushall-Report. Neuer, gezielter Diagnose-Log in `BambuTask.cpp`
(`Report raw tray_id_name`, liest `tray_id_name` direkt aus dem noch in
Scope befindlichen `JsonDocument`, da die bestehende "Report raw
payload"-Zeile bei `kLogMessageCapacity` = 320 Byte weit vor dem `ams`-Teil
abgeschnitten wird) zeigte den tatsaechlichen Befund: `field_present=1
tray_id_name=""` fuer alle drei belegten Faecher -- **nicht nur nach dem
Neustart**, sondern schon im allerersten periodischen Report danach. Der
Drucker nimmt `tray_id_name` beim Schreiben an, gibt es aber nie befuellt
zurueck, auch nicht innerhalb derselben Session. Ursache war also keine
Pushall-spezifische Auswertungsluecke, sondern eine falsche Grundannahme:
der Lesepfad haette schon Sekunden nach jeder Zuordnung die gerade lokal
bestaetigte `spoolId` wieder auf 0 zurueckgesetzt (aktiver Rueckschritt
gegenueber dem vorherigen Verhalten).

Lesepfad vollstaendig zurueckgebaut: `applyTrayOccupancy()` fasst `spoolId`
wieder nie an (wie vor der vorherigen Aenderung), Doku-Kommentare in
`BambuProtocol.h`/`PrinterState.h`/`UiBridge.cpp` und
`docs/bambu-protocol.md` entsprechend korrigiert (inkl. neuem Abschnitt mit
dem konkreten Log-Beleg). Schreibpfad (`tray_id_name` = `"SM:<id>"` beim
Zuordnen) bleibt bestehen (harmlos, evtl. auf anderer Firmware nuetzlich),
wird aber nirgends mehr gelesen. Die 3 Lesepfad-Tests aus dem vorherigen
Nachtrag entfernt, `testApplyReportNeverTouchesSpoolId` wiederhergestellt
(jetzt mit explizit leerem `tray_id_name` im Test-JSON, um genau die
Regression abzudecken, die hier aufgetreten war); Encoding-Test fuer
`tray_id_name` bleibt. 53 native Tests gruen. Der Diagnose-Log bleibt im
Code (geringe Kosten, hilfreich falls das Verhalten auf anderer
Firmware/anderem Druckermodell doch funktioniert). Build (0 Warnungen),
geflasht.

**Offene Frage unveraendert:** "Zuordnung uebersteht Neustart" ist damit
noch nicht geloest -- `spoolId` lebt weiterhin nur im ESP32-RAM. Ein echter
Fix muesste auf der ESP32-Seite selbst persistieren (z. B. `StorageTask`),
unabhaengig vom Drucker.

Nachtrag (2026-08-23, `tray_id_name` endgueltig als nicht persistent
bestaetigt): Nutzer vermutete, der Status werde mit dem falschen Befehl
abgefragt (evtl. traegt nur ein voller Pushall, nicht der periodische
`push_status`, das `tray_id_name`-Feld). Erste Diagnose-Log-Version
(`FS_LOGD`, 3 Zeilen pro Report) tauchte im Log gar nicht auf -- Ursache:
die Log-Queue verwirft neue Zeilen stillschweigend, wenn sie voll ist
(`RtosContext::enqueueLogLine()`, 10ms-Timeout), und diese Reports erzeugen
bereits 8-9 Zeilen Log-Spam in unter 15ms. Zu einer Zeile zusammengefasst
und auf `FS_LOGI` gehoben (`BambuTask.cpp`), danach sichtbar.

Gezielter Test: Zuordnung gesetzt, `AssignTray confirmed` abgewartet, dann
explizit ueber den "Aktualisieren"-Button (`RefreshSlot` ->
`RequestStatus` -> frisches `pushall`) eine neue volle Statusabfrage
ausgeloest. Auch deren Antwort zeigte `tray_id_name` weiterhin leer fuer
alle drei belegten Faecher. Damit ist der urspruengliche Verdacht
widerlegt -- es liegt nicht am Abfragebefehl, der Drucker speichert das
Feld nachweislich nirgends. `docs/bambu-protocol.md` um diesen finalen
Befund ergaenzt. Der Diagnose-Log bleibt im Code (geringe Kosten).

Nachtrag (2026-08-23, `tray_id_name`-Format ohne Trennzeichen, Nutzerwunsch):
Format von `"SM:<spoolmanId>"` auf `"SM<spoolmanId>"` umgestellt (Test-
Hypothese: der Doppelpunkt koennte vom Feld verworfen werden) --
`bambuBuildAmsFilamentSetting()`, Test/Doku entsprechend angepasst. Noch
nicht auf Hardware verifiziert, ob das Speicherverhalten damit anders
ausfaellt. Build (0 Warnungen), 53 native Tests gruen, geflasht.

Nachtrag (2026-08-24, `tray_id_name`-Ansatz komplett verworfen, stattdessen
lokaler persistierter Cache, Nutzerwunsch): Nutzer hat entschieden, den
gesamten "Spoolman-ID ueber den Drucker durchreichen"-Ansatz aufzugeben
(nach dem Hardwarebefund vom Vortag: der Drucker speichert `tray_id_name`
nachweislich nirgends) und stattdessen einen lokalen Cache
Drucker->AMS/Fach->Spoolman-Spule zu verwenden, der bei jedem Statusabgleich
gegen die vom Drucker gemeldete Farbe/Material geprueft wird -- stimmen sie
nicht mehr ueberein, gilt die Zuordnung als unbekannt (`?` in der UI). K-Faktor
und Restgewicht sollen auf den Tray-Buttons nicht mehr angezeigt werden
(waren ohnehin nur Mockdaten).

**Rueckbau `tray_id_name`:** `BambuTrayFilament::spoolmanId`-Feld und die
Kodierung in `bambuBuildAmsFilamentSetting()` entfernt (`BambuProtocol.h/.cpp`).
`PrinterSlotStateData` hat kein `spoolId`-Feld mehr (`PrinterState.h`) --
`BambuTask::checkPendingTrayAssignment()` setzt es folglich auch nicht mehr,
`PendingTrayAssignment::spoolId` (BambuTask-intern, war nur fuer diese
Zuweisung da) ebenfalls entfernt. Das Diagnose-Log aus der Vortags-
Untersuchung (`Report raw tray_id_name`) ist mit entfernt, ebenso `spool_id`
aus der "Report tray detail"-Logzeile.

**Neuer lokaler Cache** (`src/models/TraySpoolCache.h`, neu): Eintrag
`{printerId, amsId, trayId, spoolId, material, colorHex}`, `amsId`/`trayId`
entweder ein echter AMS-Slot oder beide `kExternalTraySentinel` (0xFF, wie
ueberall sonst im Projekt fuer das externe Fach). Kapazitaet bewusst auf
16 Eintraege begrenzt (realistische Nutzung statt theoretischem Maximum von
4 Druckern x 4 AMS x 4 Faechern = 64 -- haelt `AppEvent` in der
Groessenordnung von `BambuConfigCollection`, degradiert bei Ueberlauf
kontrolliert mit Log-Warnung statt Datenverlust).

Persistiert unter `/mappings/printer-slots.json` (in `AGENTS.md` §21 bereits
als Pfad fuer genau diesen Zweck reserviert) -- neuer
`rtos::StorageDocumentType::TraySpoolCache`, vollstaendig durchgezogen durch
`JsonStorage.h/.cpp` (Groessenbucket, Defaults, `documentTypeName`,
`createDefault`, `validate` inkl. neuer `validateTraySpoolCacheEntries()`
mit Adressvalidierung/Duplikatpruefung) und `StorageTask.cpp`
(`kInitialDocuments[]`-Eintrag, Lade-Block analog zum bestehenden Bambu-Muster).
`isAllowedJsonPath()` erlaubte den neuen Pfad bereits generisch (jedes
`/mappings/*.json`), keine Anpassung noetig; die separate, restriktivere
`isMappingPath()`-Pruefung betrifft nur die 3 alten Legacy-NFC-Mapping-
Dateien und war nicht zu beruehren.

**AppTask.cpp** (alleiniger Besitzer des Caches): `traySpoolCache`-Modul-
variable, `requestTraySpoolCache()`/`persistTraySpoolCache()` (Laden beim
Boot analog `requestBambuConfiguration()`, Speichern als Fire-and-Forget
ohne Dialog -- ein fehlgeschlagener Save ist beim naechsten erfolgreichen
Zuordnen einfach erneut versucht) und `resolveTraySpoolCacheSpoolId()`
(Cache-Lookup + Abgleich material/colorHex gegen die *aktuell* vom Drucker
gemeldeten Werte, 0 bei Nichtuebereinstimmung). Cache-Aktualisierung direkt
am bestehenden Bestaetigungspunkt in der `pendingSlotAssignment`-Zustands-
maschine eingehaengt (dort, wo bisher nur der Fortschrittsdialog geschlossen
wurde): bei erfolgreicher Zuordnung wird die Spoolman-ID mit
material/colorHex aus dem soeben bestaetigten `PrinterState` upserted und
persistiert, bei ResetSlot/UntagSlot (`wasClearing`) wird der Eintrag
entfernt. `syncAmsToUi()` nutzt `resolveTraySpoolCacheSpoolId()` statt des
entfernten `slot.spoolId` fuer sowohl AMS-Faecher als auch das externe Fach
(Cache findet dort nie einen Eintrag, da externe Faecher nie ueber
`AssignTray` laufen -- unveraendert, war vorher genauso immer leer).

**UI:** `UiBridge.cpp::updateTrayButton()` zeigt auf dem Tray-Button jetzt
nur noch das Material (oder "belegt"), kein Mock-Restgewicht/K-Faktor mehr.
Spoolmanager-ID-Anzeige (`?` bei unbekannt/nicht mehr passend) unveraendert.
Staging-Karte (`home_staging`) bleibt unveraendert -- deren Spoolman-ID ist
weiterhin echt (ueber den Spulen-Picker zugeordnet), keine Mockdaten, vom
Nutzerwunsch nicht betroffen.

**Tests:** `testAmsFilamentSettingEncodesSpoolmanTrayIdName` entfernt (Feature
weg); `testApplyReportNeverTouchesSpoolId` entfernt (Feld existiert nicht
mehr, nichts mehr zu testen); `test_printer_model`s
`testAmsAndSlotLookup` auf `material`/`state` statt `spoolId` umgestellt.
51 native Tests gruen (vorher 53). `test_json_storage` deckt den neuen
Storage-Pfad nicht ab (existiert nicht in einer `native-*`-Umgebung, JsonStorage.cpp
haengt an Arduino-`FS.h`/`Print.h` -- wie bei allen anderen Storage-
Dokumenttypen unveraendert nur ueber den Geraete-Build abgesichert).

RAM-Zuwachs durch `TraySpoolCache` in `AppEvent` (16-tief gequed): ca. 5.7 KB
(34.4% -> 36.2% von 320 KB), Flash 26.0% -> 26.5%. Build (0 Warnungen),
geflasht.

**Noch nicht auf echter Hardware verifiziert:** ob die neue Zuordnung
tatsaechlich einen Neustart uebersteht (persistiert jetzt rein lokal, sollte
funktionieren, aber der komplette Lade-/Speicherpfad ueber `StorageTask`/SD
ist fuer diesen neuen Dokumenttyp noch nicht auf Hardware getestet) und ob
die Abgleichslogik (`material`/`colorHex`-Vergleich) bei einem echten
Spulenwechsel am Drucker korrekt "?" zeigt.

Nachtrag (2026-08-24, Restgewicht/K-Faktor aus Spoolman nachladen,
Nutzerwunsch): sobald eine Spule per obigem Cache identifiziert ist, werden
Restgewicht und K-Faktor jetzt echt aus Spoolman nachgeladen und auf dem
Tray-Button angezeigt (vorher: gar keine Anzeige, davor: Mockdaten).

**Neues Spoolman-Extra-Feld** `bambu_k_factor` (`models/SpoolmanSpool.h`:
`bambuKFactorPresent`/`bambuKFactorValid`/`bambuKFactor`, analog zu
`bambu_temp_min`/`_max`) -- projektspezifisch, keine Bambu-/Spoolman-
Standardgroesse; `SpoolmanTask::parseSpool()` dekodiert es ueber die
bestehende `SpoolmanClient::decodeNumberExtraField()`. Anders als die
Duesentemperaturen nur fuers Anzeigen gedacht, fliesst nicht in ein an den
Drucker gesendetes Kommando ein. `remainingWeightGrams` existiert bereits
als Standard-Spoolman-Feld, keine Aenderung noetig.

**Neuer Fetch-Pfad in AppTask.cpp:** kleiner, rein lokaler (nicht
persistierter) Cache `traySpoolDetails` (8 Eintraege, per `spoolId`
keyed) -- anders als `traySpoolCache` echte Live-Spoolman-Daten
(Restgewicht aendert sich durch Verbrauch), daher nicht auf SD gespeichert,
frisch bei jedem Boot. `resolveTraySpoolDetails()` liefert den Cache-Treffer
zurueck oder loest (falls noch keiner in Flug) einen `LoadSpool`-Abruf aus;
Request-IDs aus einem eigenen Bereich (`kTraySpoolDetailsRequestIdBase`,
Index-basiert wie das bestehende `kLegacyMigrationDeleteRequestBase`-Muster)
identifizieren die Antwort im grossen `SpoolmanResponse`-Dispatch --
erfolgreiche Antwort speichert das Ergebnis und stoesst sofort einen
`syncAmsToUi()` fuer den aktiven Drucker an, statt auf den naechsten
ohnehin faelligen Sync zu warten. Bewusst ein einmaliger Snapshot, keine
TTL/Refresh-Logik -- eine spaeter am Drucker/in der App verbrauchte Menge
aktualisiert die Anzeige daher erst nach einem Neustart oder wenn der Cache-
Slot durch einen anderen Spool verdraengt wird.

**UI:** `syncAmsToUi()` fuellt `UiCommand.spool` (bereits vorhandenes Feld,
bisher nur fuer `UpdateStaging` genutzt) mit einer minimalen `SpoolmanSpool`
(nur `id`/`remainingWeightGrams`/`bambuKFactorValid`/`bambuKFactor` gesetzt)
sobald Details geladen sind -- keine Aenderung an `rtos::UiCommand` noetig.
`TrayUiEntry` (`UiBridge.cpp`) um `detailsLoaded`/`remainingWeightGrams`/
`kFactorValid`/`kFactor` erweitert; `detailsLoaded` unterscheidet bewusst
"noch nicht geladen" von einem echten Restgewicht 0 (aus `command.spool.id
!= 0`, das nur bei erfolgreichem Ladevorgang gesetzt wird). Tray-Button-Text
jetzt wieder dreizeilig, sobald identifiziert *und* geladen:
`<Material>\n<Restgewicht>g\nK (<Faktor>)` (K-Zeile nur wenn
`bambu_k_factor` in Spoolman hinterlegt) -- vorher/bei unbekannter
Zuordnung weiterhin nur das Material.

Build (0 Warnungen), 51 native Tests gruen, RAM-Zuwachs minimal (+0.5 KB
gegenueber dem Cache-Nachtrag). Geflasht.

**Noch nicht auf echter Hardware verifiziert:** der komplette Spoolman-
Abruf-/Anzeige-Pfad fuer dieses Feature (Fetch-Timing, ob `bambu_k_factor`
korrekt dekodiert wird, ob die Anzeige nach Abschluss des asynchronen
Abrufs wie erwartet nachzieht).

Nachtrag (2026-08-24, `bambu_temp_min`/`bambu_temp_max`/`bambu_k_factor`
sind Filament-, nicht Spulen-Eigenschaften -- Nutzerhinweis + Anfrage
angepasst): bisher wurden alle drei Felder aus dem in einer
`LoadSpool`-Antwort verschachtelten `filament`-Objekt gelesen -- strukturell
zwar bereits auf Filament-Ebene, aber implizit auf die Vollstaendigkeit
dieses eingebetteten Objekts angewiesen. Auf Nutzerwunsch komplett
umgestellt (beide betroffenen Pfade, siehe unten): explizites
`GET /filament/{id}` statt sich auf die Spool-Antwort zu verlassen.

**Neuer Spoolman-Request:** `SpoolmanCommandType::LoadFilament`
(`rtos/Commands.h`), `SpoolmanCommand::filamentId` (`rtos/Messages.h`),
`SpoolmanTask::loadFilamentDetails()`/`sendFilamentDetails()` (`GET
/filament/{id}`, `AppEventType::SpoolmanResponse` mit neuem
`AppEvent::filament`-Feld). `SpoolmanTask::parseFilament()` dekodiert
`bambu_temp_min`/`bambu_temp_max`/`bambu_k_factor` jetzt von dort (Root-
Ebene der Antwort, nicht mehr unter einem verschachtelten `"filament"`-
Schluessel wie in einer Spool-Antwort).

**Modelle:** die drei Felder von `models::SpoolmanSpool` nach
`models::SpoolmanFilament` verschoben (`bambuTempFieldsPresent/Valid/MinC/
MaxC`, `bambuKFactorPresent/Valid/KFactor`); `SpoolmanSpool` bekommt
stattdessen `filamentId` (aus `filament["id"]` der Spool-Antwort geparst),
um den Folge-Request adressieren zu koennen. `parseSpool()` parst die drei
Felder nicht mehr selbst.

**AssignTray-Ablauf** (`AppTask.cpp`, bisher bereits auf echter Hardware
validiert -- mit Bedacht angepasst): `SlotAssignmentStage` um `LoadingFilament`
erweitert (`SelectingSpool -> LoadingSpool -> LoadingFilament -> WritingSlot`).
`LoadingSpool`s Erfolgsantwort baut nicht mehr direkt das `AssignTray`-
Kommando, sondern merkt sich material/colorHex in
`PendingSlotAssignment::trayType/trayColorHex` und stoesst per
`requestFilamentDetails()` den Filament-Fetch an. Neue
`LoadingFilament`-Erfolgs-/Fehlerbehandlung baut das `AssignTray`-Kommando
(neue Helper-Funktion `sendPendingSlotAssignTray()`, liest die gemerkten
Felder + die per Parameter uebergebene Temperatur). Schlaegt der Filament-
Fetch fehl (Netzwerkfehler, nicht "Feld fehlt/ungueltig"), wird die
Zuordnung trotzdem abgeschlossen, nur ohne Temperatur -- dieselbe
Nutzerfreundlichkeit wie beim bisherigen "Felder fehlen/ungueltig"-Fall,
keine harte Fehlerabbruch mehr fuer einen reinen Netzwerk-Hickup beim
zweiten Request.

**Home-Tray-Karte** (`resolveTraySpoolDetails()`, gestern neu eingefuehrt):
`TraySpoolDetailsEntry` um einen `TraySpoolDetailsStage`-Zustand erweitert
(`Idle -> LoadingSpool -> LoadingFilament -> Loaded`) statt der bisherigen
`pending`/`loaded`-Bools, gleiches Muster wie der AssignTray-Ablauf.
`rtos::UiCommand` um `kFactorValid`/`kFactor` erweitert (K-Faktor ist jetzt
keine `SpoolmanSpool`-Eigenschaft mehr, `command.spool` kann es nicht mehr
tragen); Restgewicht nutzt neu das bereits vorhandene `UiCommand::weightGrams`
statt `command.spool.remainingWeightGrams`. `UiBridge.cpp`s
`detailsLoaded`-Erkennung (`command.spool.id != 0`) unveraendert gueltig,
liest Gewicht/K-Faktor jetzt aber aus den neuen Feldern.

Build (0 Warnungen), 51 native Tests gruen, RAM +6.2 KB (36.3% -> 38.2% von
320 KB, groesster Einzelsprung bisher -- die zusaetzlichen `filament`-Felder
in `AppEvent`/`SpoolmanCommand`, beide bereits recht grosse gequeute
Strukturen). Geflasht.

**Noch nicht auf echter Hardware verifiziert:** der komplette neue
Filament-Fetch-Pfad (beide Verwendungen), insbesondere ob
`GET /filament/{id}` bei diesem Spoolman-Server tatsaechlich `extra` auf
Root-Ebene zurueckgibt wie angenommen, und ob der AssignTray-Ablauf mit dem
zusaetzlichen Zwischenschritt weiterhin zuverlaessig funktioniert (er war
vor dieser Aenderung bereits hardwaregetestet).

Nachtrag (2026-08-24, Log-Spam bei jedem periodischen `push_status` auf
Trace-Level, Nutzerwunsch): fuenf `FS_LOGD`/`FS_LOGI`-Zeilen in
`BambuTask.cpp` ("Report raw payload", "MQTT command reply", "Report
applied", "Report tray detail" je Fach) und `AppTask.cpp` ("Bambu event
received") feuerten bisher bei *jedem* Statusbericht (alle paar Sekunden),
nicht nur bei echten Ereignissen -- auf `FS_LOGT` (Trace) umgestellt. Build
(0 Warnungen), 51 native Tests gruen, geflasht.

Nachtrag (2026-08-24, Feldname-Korrektur + fehlende Diagnose-Logs, nach
Hardware-Test): Nutzer meldete anhand eines echten Zuordnungs-Logs, dass
trotz auf Filament-Ebene konfigurierter `bambu_temp_min`/`bambu_temp_max`/
`flow_dynamics_k_factor`-Extra-Felder weiterhin `nozzle_temp_min=0
nozzle_temp_max=0` an den Drucker gesendet wurde, und stellte klar, dass
der korrekte Spoolman-Extra-Feldname fuer den K-Faktor
`flow_dynamics_k_factor` ist (nicht das von mir angenommene
`bambu_k_factor`). Zwei Aenderungen: (1) Feldname in `SpoolmanTask.cpp`
(`parseFilament()`) sowie alle Kommentare in `SpoolmanCatalog.h`,
`SpoolmanSpool.h`, `Messages.h`, `AppTask.cpp`, `UiBridge.cpp` von
`bambu_k_factor` auf `flow_dynamics_k_factor` korrigiert. (2) Der
Filament-Abruf (`getJson()`/`loadFilamentDetails()` in `SpoolmanTask.cpp`)
hatte bislang *keinerlei* Logging -- weder Erfolg noch Fehlschlag --
wodurch die eigentliche Ursache fuer `temp=0` nicht diagnostizierbar war.
`getJson()` loggt jetzt jeden fehlgeschlagenen GET (`FS_LOGE`, URL +
Fehlertext + ggf. JSON-Parse-Fehler) sowie jeden erfolgreichen GET
(`FS_LOGT`, URL). `loadFilamentDetails()` loggt zusaetzlich nach dem
Parsen die tatsaechlich extrahierten Werte (`FS_LOGD`: filament_id,
temp_fields_present/valid, temp_min/max, kfactor_present/valid/wert) bzw.
bei Parse-Fehlschlag den Grund (`FS_LOGE`). Damit sollte ein erneuter
Zuordnungsversuch zeigen, ob (a) der HTTP-Request fehlschlaegt, (b) die
Antwort geparst, aber `bambuTempFieldsValid=false` ist, oder (c) die
Werte tatsaechlich korrekt ankommen und das Problem woanders liegt. Die
eigentliche `temp=0`-Ursache ist damit noch nicht behoben, nur
diagnostizierbar gemacht -- naechster Schritt ist ein erneuter
Hardware-Test mit den neuen Logs. Ausserdem einen durch das vorherige
sed-Refactoring versehentlich in sich widerspruechlichen Kommentar
(`"flow_dynamics_k_factor", nicht "flow_dynamics_k_factor"`) auf
`nicht "bambu_k_factor"` korrigiert. Build (0 Warnungen), 51 native Tests
gruen, geflasht.

Nachtrag (2026-08-24, weitere Diagnose-Logs nach erneutem Hardware-Test):
Nutzer-Log zeigte `[SPOOLMAN] Filament loaded ... temp_fields_valid=1
temp_min=191 temp_max=241 ...` (der Filament-Abruf war also erfolgreich
und korrekt geparst), aber trotzdem weiterhin `nozzle_temp_min=0
nozzle_temp_max=0` in der ausgehenden `ams_filament_setting`-MQTT-Nachricht
242ms spaeter. Ausfuehrliche statische Codepruefung der gesamten Kette
`SpoolmanTask::loadFilamentDetails()` -> `AppTask`s
`SlotAssignmentStage::LoadingFilament`-Antwort-Handler ->
`sendPendingSlotAssignTray()` -> `BambuTask::handleAssignTray()` fand
keinen Logikfehler; die requestId-Verkettung (`action.requestId` ->
LoadSpool -> LoadFilament -> `pendingSlotAssignment.requestId`) ist
durchgaengig konsistent. Da die Ursache dadurch nicht abschliessend
lokalisiert werden konnte, zwei weitere gezielte Logs ergaenzt, um den
naechsten Hardware-Test eindeutig zu machen: `[SPOOLMAN] Filament loaded`
traegt jetzt `request_id=` (zum Abgleich mit der Zuordnung); neu
`[APP] Sending AssignTray request_id=... spool_id=... nozzle_temp_min=...
nozzle_temp_max=...` direkt beim Enqueuen des `BambuCommand`s, sowie
`[APP] LoadFilament skipped request_id=... spool_filament_id=...` falls
der Filament-Folge-Request uebersprungen wird (z. B. `filamentId=0`). Noch
kein Fix, nur weitere Diagnose. Build (0 Warnungen), 51 native Tests
gruen, geflasht.

Nachtrag (2026-08-24, drei Nutzer-Reports zu Leergewicht/Bruttogewicht/
K-Faktor beim Staging, noch vor Antwort auf die obige Diagnose-Anfrage):
(1) "Leergewicht wird immer 0g ausgegeben" -- Ursache gefunden (reiner
UI-Bug, unabhaengig von der Spoolman-Antwort selbst):
`UiBridge.cpp`s `UpdateStaging`-Handler initialisierte die lokalen
Variablen `emptyWeightGrams`/`initialWeightGrams` *immer* aus
`command.weightUpdate.*` (fuer den manuellen Text-Eingabe-Pfad gedacht),
auch im `hasReloadedSpool`-Zweig (echter Spoolman-Reload) -- dort ist
`command.weightUpdate` nie gesetzt (0), wodurch das per `command.spool.
emptyWeightGrams` korrekt geladene Leergewicht direkt danach wieder auf 0
ueberschrieben wurde. Jetzt quellenspezifisch vorbelegt
(`hasReloadedSpool ? command.spool.* : command.weightUpdate.*`). (2)
"Bruttogewicht zeigt Restgewicht-Leergewicht" -- war zuvor tatsaechlich der
*Live-Waagenwert* (`liveWeight.grossWeightGrams`), voellig unabhaengig von
der gestagten Spule; korrekte Semantik laut Mockup-Vorlage (Leer=250,
Brutto=1247, Rest=997, also Brutto=Leer+Rest) jetzt nachgebaut:
`stagingState.grossWeightGrams = emptyWeightGrams + remainingWeightGrams`.
(3) "K-Faktor bei Staging-Spulenauswahl falsch/Mockdaten" -- bestaetigt:
`updateHomeContent()`s Staging-Kachel (`objects.staging__staging`) zeigte
seit der Restgewicht/K-Faktor-Einfuehrung (siehe Nachtrag oben) bewusst
Mockdaten (`mockKFactorThousandths`, ein Rechenwert aus `spoolId % 5`).
Die AMS-Faecher wurden damals bereits auf echte Spoolman-Werte umgestellt,
die Staging-Kachel jedoch nicht. Fix: `AppTask` laedt beim Staging-Reload
jetzt denselben LoadSpool->LoadFilament-Zweischritt wie beim
AssignTray-Ablauf (neuer `PendingStagingFilamentLoad`-State plus
`sendStagingUpdate()`-Helper) und liefert `emptySpoolWeightGrams`/
`bambuKFactorValid`/`bambuKFactor` vom Filament-Endpoint an die UI
(`UiCommand::kFactorValid`/`kFactor`, wiederverwendet von der
Tray-Karten-Anzeige). `UiModels.h`s `UiStagingSummary` um `kFactorValid`/
`kFactor` erweitert; `updateHomeContent()`s Staging-Kachel und
`updateStagingContent()` (Detailtabelle) zeigen jetzt echte Werte statt
Mock/Live-Waage, mit graceful degradation (Filament-Fetch schlaegt fehl ->
Anzeige ohne K-Faktor-Zeile/mit Leergewicht 0, keine erfundenen Werte).
Ausserdem auf Nutzerwunsch das Spoolman-Logging erhoeht: `loadSpools()`
(Einzel-Spule und Suche) loggt jetzt vor dem Request die abgefragte URL
und nach dem Parsen die vollstaendigen Ergebniswerte (`FS_LOGD`: spool_id,
filament_id, vendor, material, empty/initial/remaining_weight bzw. bei
der Suche die Trefferzahl) oder den Parse-Fehlschlag (`FS_LOGE`). Build
(0 Warnungen), 51 native Tests gruen, geflasht.

Nachtrag (2026-08-24, K-Faktor wird geladen, aber nirgends angezeigt):
Hardware-Log bestaetigte, dass der K-Faktor-Fetch selbst funktioniert
(`kfactor_valid=1 kfactor=0.100`), aber `stagingState.kFactorValid/kFactor`
hatte in der Staging-Detailtabelle (`updateStagingContent()`,
`scr_staging_details`) bislang schlicht keine eigene Zeile -- nur die
Home-Kachel (`objects.staging__staging`) zeigt K-Faktor, die
Detailtabelle (Spoolman-ID/Hersteller/Material/Farben/Leergewicht/
Bruttogewicht/Restgewicht/NFC) aber nicht. `stagingTableRows` ist
programmatisch erzeugt (`createStagingTableDecoration()`, 20px/Zeile ab
y=80), nicht EEZ-Studio-generiert, daher ohne Studio-Aenderung um eine
9. Zeile erweiterbar: Array-Groesse 8 -> 9, neue Zeile "K-Faktor: %.3f"
(oder "nicht hinterlegt") zwischen Restgewicht und NFC eingefuegt. Letzte
Zeile liegt jetzt bei y=240..260, die Aktionsbuttons beginnen bei y=264 --
passt ohne Ueberlappung. Build (0 Warnungen), 51 native Tests gruen,
geflasht.

Nachtrag (2026-08-24, CMP_TRAY_CARD/CMP_STAGING_CARD auf drei Labels
umgebaut, Nutzerumbau im ui-project): Nutzer hat in EEZ Studio das
einzelne mehrzeilige "label"-Sub-Widget beider Komponenten durch drei
eigene Labels ersetzt (material/weight/k_factor) und den Export bereits
ausgefuehrt (`screens.c`/`screens.h` neu generiert,
`objects.home_tray_N__material/weight/k_factor` bzw.
`objects.staging__material/weight/k_factor`). `updateTrayButton()` in
`UiBridge.cpp` nahm bisher genau ein `label`-Zielobjekt entgegen und
baute einen kombinierten mehrzeiligen String -- Signatur auf drei
separate `materialLabel`/`weightLabel`/`kFactorLabel`-Parameter
umgestellt, alle fuenf Aufrufstellen (tray_1..4, external) sowie die
Staging-Kachel in `updateHomeContent()` entsprechend angepasst. Leere
Zeilen (kein K-Faktor hinterlegt, Restgewicht noch nicht geladen,
Fach/Staging leer) setzen jetzt einfach leeren Text statt eine Zeile im
kombinierten String wegzulassen -- bei fester Positionierung (kein
Textumbruch-Layout) visuell aequivalent. Build (0 Warnungen), 51 native
Tests gruen, geflasht.

Nachtrag (2026-08-24, K-Faktor wird aus Spoolman geladen, aber im
Staging-Kachel-Label `k_factor` nicht angezeigt -- Material/Gewicht
zeigen laut Nutzer korrekt): Hardware-Log bestaetigt erneut einen
korrekten Fetch (`kfactor_valid=1 kfactor=0.200`). Codepruefung der
Kette `AppTask::sendStagingUpdate()` -> `UiBridge`s `UpdateStaging`-
Handler -> `updateHomeContent()`s Staging-Kachel-Block fand keinen
Logikfehler; alle drei Stellen setzen/lesen `kFactorValid`/`kFactor`
konsistent, und der EEZ-Export-Offset fuer `objects.staging__k_factor`
wurde gegen den Objekt-Erzeugungscode gegengeprueft (9 Sub-Widgets je
Kartenprofil + 1 Wrapper-Slot, passt zur 10er-Schrittweite zwischen den
`create_user_widget_cmp_*`-Aufrufen in `screens.c`). Da die Ursache
dadurch nicht lokalisiert werden konnte, drei gezielte Logs ergaenzt:
`[APP] Sending UpdateStaging ... kfactor_valid=... kfactor=...` beim
Senden, `[UI] UpdateStaging received ... kfactor_valid=... kfactor=...`
beim Empfang, und `[UI] Staging card k_factor label set/cleared
text=... obj=<Pointer>` direkt vor dem `setControlText()`-Aufruf --
letzteres insbesondere um zu pruefen, ob `objects.staging__k_factor`
ueberhaupt ein gueltiges (nicht-null) LVGL-Objekt ist. Noch kein Fix,
nur weitere Diagnose. Build (0 Warnungen), 51 native Tests gruen,
geflasht.

Nachtrag (2026-08-24, eigentliche Ursache gefunden -- betrifft auch den
laengst zurueckliegenden `temp=0`-Bug): Nutzer-Log zeigte den
entscheidenden Hinweis: `[APP] Sending UpdateStaging ... kfactor_valid=0`
erschien **vor** `[SPOOLMAN] Filament loaded ... kfactor_valid=1`
derselben Anfrage-ID -- die UI wurde also aktualisiert, *bevor* die
echte Filament-Antwort überhaupt eintraf. Ursache:
`SpoolmanTask::loadSpools()` sendet nach *jeder* `LoadSpool`-Anfrage
(auch fuer eine einzelne Spule) zusaetzlich zur Spule selbst ein
Abschluss-Event (`"N Spulen gefunden"`, `value=-1`, leeres `spool`/
`filament`) mit dergleichen `requestId` -- urspruenglich fuer den
Spulen-Picker gedacht ("Suche fertig"). Da `AssignTray`s
`LoadingFilament`-Schritt, `AppTask::sendStagingUpdate()`s
`PendingStagingFilamentLoad` und `resolveTraySpoolDetails()`s
`TraySpoolDetailsEntry` alle den `LoadFilament`-Folge-Request unter
*derselben* `requestId` wie die vorausgehende `LoadSpool`-Anfrage
stellen, wurde dieses Abschluss-Event faelschlich als Filament-Antwort
interpretiert -- es kommt strukturell *immer* zuerst an (direkt nach der
LoadSpool-Antwort, noch bevor der Filament-Request den Server überhaupt
erreicht hat). Die Handler lasen `value=-1` als "Fetch fehlgeschlagen",
loeschten dabei aber ihren Pending-State; die echte Antwort landete
danach in keinem Handler mehr. Fix an allen drei betroffenen Stellen in
`AppTask.cpp`: Bedingung um `event.filament.id != 0` ergaenzt (nur eine
echte Filament-Antwort setzt dieses Feld) -- der Abschluss-Marker faellt
jetzt unbehandelt durch, der Pending-State bleibt bis zur echten Antwort
bestehen. Erklaert rueckwirkend auch das nie aufgeklaerte
`nozzle_temp_min=0 nozzle_temp_max=0`-Symptom vom AssignTray-Test sowie
vermutlich denselben Fehler unbemerkt bei den AMS-Tray-Karten (Gewicht
wird schon in der LoadSpool-Stufe gesetzt, daher unauffaellig -- nur
K-Faktor war betroffen). `docs/bambu-protocol.md` entsprechend ergaenzt.
Build (0 Warnungen), 51 native Tests gruen, geflasht -- Hardware-Test
fuer Staging-K-Faktor UND AssignTray-Temperaturen steht noch aus.

Nachtrag (2026-08-24, echtes Duesen-Icon statt Mockformel, Nutzerwunsch):
Bambu meldet ueber `print.ams.tray_now`, welches Fach gerade in der Duese
aktiv ist -- global ueber alle AMS-Einheiten hinweg adressiert (`0..15` =
`amsId*4+trayId`, `254` = externe Spule/`vt_tray`, `255` = keins aktiv).
Bisher zeigte `UiBridge.cpp`s `updateTrayButton()` das Duesen-Icon per
Mockformel (immer das erste belegte Fach je AMS). Umgesetzt: neues
`PrinterState::activeTrayNow`-Feld (Default `kActiveTrayNowNone=255`,
neue Konstanten `kActiveTrayNowExternal=254`/`kActiveTrayNowNone=255` in
`PrinterState.h`), `BambuProtocol::bambuApplyReport()` parst
`tray_now` (eigener `parseTrayNow()`-Helper, da der volle 0..255-Bereich
gebraucht wird -- der bestehende `parseTrayIndex()` nimmt eine
`uint8_t`-Obergrenze und kann daher kein `maxExclusive=256` ausdruecken).
`AppTask::syncAmsToUi()` berechnet je Fach den globalen Index
(`amsIndex*kSlotsPerAms+trayIndex`) und vergleicht gegen
`activeTrayNow`, kodiert das Ergebnis als Bit 1 in `UiCommand::value`
(Bit 0 bleibt "belegt", bestehende Konvention). `UiBridge.cpp`:
`TrayUiEntry` um `isActiveNozzle` erweitert, `UpdateTrayDetails`-Handler
dekodiert Bit 1, `updateTrayButton()` zeigt das Duesen-Icon jetzt nur
noch beim tatsaechlich aktiven Fach -- Bildauswahl (hell -> "3D Printer
Nozzle", dunkel -> "3D Printer Nozzle W") unveraendert, war schon
korrekt implementiert. Neuer Test `testApplyReportParsesTrayNow`
(numerische/String-Form, AMS-Fach, extern, keins, Feld fehlt -> alter
Wert bleibt). Build (0 Warnungen), 52 native Tests gruen, geflasht.

Nachtrag (2026-08-24, hintergrundabhaengige Label-Styles, Nutzerwunsch):
material/weight/k_factor (Tray- und Staging-Karte) sowie STAGING_LABEL
sollen je nach Helligkeit der ersten Filamentfarbe/des Kartenhintergrunds
zwischen der hellen (`LabelStandart`/`LabelHeader`, dunkler Text) und
dunklen (`LabelStandart_W`/`LabelHeader_W`, heller Text) Style-Variante
wechseln. Neue Helper-Funktion `applyBackgroundAwareLabelStyle()` in
`UiBridge.cpp` (entfernt immer zuerst beide Varianten, fuegt dann die
passende hinzu -- sonst haengen beim Umschalten alte Styles an, LVGL
ersetzt Styles nicht automatisch), angewendet in `updateTrayButton()`
(alle 5 Tray-Karten) und im Staging-Widget in `updateHomeContent()`
(ersetzt dort den bisherigen rohen `lv_obj_set_style_text_color()`-Aufruf
fuer STAGING_LABEL). `LabelHeader_W` existierte zwar schon im
ui-project, war aber noch nicht exportiert -- `styles.h` war zudem
bislang gar nicht in `UiBridge.cpp` eingebunden (nie direkt gebraucht,
Style-Funktionen liefen bisher nur ueber generierten Screen-Code); beides
ergaenzt (`#include "ui/generated/styles.h"`), nach Nutzer-Export von
`styles.c`/`.h` fertig gebaut. Build (0 Warnungen), 52 native Tests
gruen, geflasht.

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

* [x] TagIdentity
* [x] extra.tag Lookup
* [x] Definition
* [x] Match
* [x] zuordnen
* [x] importieren
* [x] Staging

Hinweis: Kein Code-Bug mehr offen, reine Verifikation. OpenTag3D teilt
in AppTask.cpp denselben Codezweig wie OpenPrintTag
(`currentTag.format == OpenPrintTag || OpenTag3D`), daher wurde der
Staging-Fix aus 9.4 (`requestStagingSpool()` beim Anzeigen von TagResult
für eine bereits zugeordnete Spule) strukturell bereits mit erledigt.
TagIdentity/extra.tag Lookup/Match/zuordnen/importieren laufen ebenfalls
über dieselbe generische, formatunabhängige Pipeline wie bei OpenPrintTag:
Laut Primärspezifikation (docs/opentag3d.md) definiert OpenTag3D kein
eigenes Identitätsfeld, `TagParserRegistry` verwendet daher konsequent die
NFC-UID als Identity. Definition-Parsing (`OpenTag3D.cpp`, Core/Extended
Felder, Byte-Layout aus `spec.json`) stammt bereits vollständig aus Phase
5.x und ist durch den nativen Testvektor abgedeckt.

Unverändert aus Phase 5.x: Der PN532 unterstützt nur ISO/IEC 14443A
(NTAG213/215/216, Core-Format); die SLIX2-Variante für Extended-Felder
(ISO/IEC 15693) kann mit der aktuellen Hardware nicht gelesen werden
(dokumentiertes Hardwarelimit, kein Software-Bug).

Keine Codeänderung in diesem Schritt -- Build/Tests/Flash entsprechen dem
bereits in Phase 9.4 verifizierten Stand (`pio run` 0 Warnings, `pio test
-e native-spoolman-tests` 44/44, Firmware bereits auf dem Gerät).

## 9.6 Legacy

* [x] TagIdentity
* [x] extra.tag Lookup
* [x] Definition
* [x] importieren
* [x] zuordnen
* [x] sichere physische Migration optional

Hinweis: Keine Codeänderung nötig, reine Verifikation -- anders als bei
9.3/9.4 gab es hier keinen Staging/wiegen-Bug zu beheben, da der
`TagLegacy`-Bildschirm (im Gegensatz zu `TagResult`) ohnehin keine
Wiege-Buttons besitzt; das Checklist-Item "Staging"/"wiegen" existiert für
9.6 bewusst nicht.

TagIdentity/extra.tag Lookup: Legacy-Tags sind mangels eigenem
Identitätsfeld UID-basiert (generische `TagIdentitySource::NfcUid`über
`TagParserRegistry`); der `TagFormat::Legacy`-Zweig in AppTask.cpp
(`currentTag.format == models::TagFormat::Legacy`, Zeile ~3269) ist nicht
"nativ" (nur `EmptyNdef`/`FilamentStation` gelten als `nativeFormat`),
durchläuft daher korrekt die generische `FindSpoolByTag`-Auflösung und
zeigt das Ergebnis auf dem eigenen `TagLegacy`-Bildschirm.

Definition: `LegacyTagParser` (aus Phase 5.9) parst das dokumentierte
`spool:<id>`-Klartextformat.

importieren/zuordnen: laufen bereits generisch über
`ImportTagDefinition`/`AssignTag` (Legacy ist in beiden Whitelists
enthalten).

Sichere physische Migration (optional): bereits vollständig über die
bestehende Schreib-Pipeline abgedeckt, ohne eigene Aktion. In
`TagParsers.cpp` setzt `LegacyTagParser::parse` bei erkanntem
`spool:<id>`-Payload `safeToRewriteAsFilamentStation = true` -- die einzige
aktuell verifizierte, sicher ersetzbare Legacy-Darstellung. Zusammen mit
`TagWritePolicy::capabilitiesFor` (`canWriteFilamentStationPayload` für
Legacy nur bei beschreibbarem nativem NTAG *und*
`safeToRewriteAsFilamentStation`) liefert `assignmentEffect()` dann
`MappingAndPayload`, wodurch der reguläre `AssignTag`-Ablauf ("zuordnen")
den physischen Tag automatisch und sicher auf `spoolman:<id>` migriert --
kein separater Button/Screen nötig. Der in der EEZ-UI vorhandene, aber
bewusst unverdrahtete und ausgeblendete `tag_legacy_migrate`-Button ist
dadurch obsolet (Migration passiert transparent als Teil von "zuordnen"),
nicht vergessen. Ist die Hardwarevoraussetzung nicht erfüllt (kein
beschreibbarer nativer NTAG), bleibt es bei `MappingOnly`
(`extra.tag`-Zuordnung ohne physisches Schreiben) -- daher "optional".

Build/Tests/Flash entsprechen unverändert dem in Phase 9.4 verifizierten
Stand (`pio run` 0 Warnings, `pio test -e native-spoolman-tests` 44/44,
Firmware bereits auf dem Gerät).

## 9.7 Unknown

* [x] UID
* [x] extra.tag Lookup
* [x] zuordnen
* [x] physischer Tag unverändert

Hinweis: UID/extra.tag Lookup/zuordnen/Schreibschutz waren bereits
generisch abgedeckt -- Unknown definiert kein eigenes Identitätsfeld
(UID-basierte `TagIdentitySource::NfcUid`), durchläuft die generische
`FindSpoolByTag`-Auflösung (nicht "nativ") und nutzt für "zuordnen"
(`TagUnknown`-Bildschirm, unzugeordneter Fall) den generischen
`AssignTag`-Ablauf. `TagWritePolicy::capabilitiesFor` setzt für
`TagFormat::Unknown` keine Schreib-/Löschfähigkeit, wodurch
`assignmentEffect`/`removalEffect` immer `MappingOnly` liefern -- der
physische Tag bleibt also strukturell garantiert unverändert.

Wie in 9.3 angekündigt bestand aber auch hier der identische
Staging-Bug: Der `Unknown`-Zweig in AppTask.cpp zeigt für einen bereits
per UID zugeordneten Tag ebenfalls `TagResult` an (mit
Quick/Advanced-Wiegen-Buttons), hat die aufgelöste Spule aber nie mit
`requestStagingSpool()` ins Staging geladen. Obwohl die Checkliste für
9.7 kein eigenes "Staging"/"wiegen"-Item führt, ist dieser Zweig Teil des
"Unknown"-Workflows und wurde mit demselben Muster wie 9.3/9.4 behoben
(`requestStagingSpool` gegen `pendingStagingSpoolRequestId == 0`
abgesichert), da ein kaputter Wiege-Pfad sonst keinen echten
Hardware-Test des zugeordneten Falls erlaubt hätte.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

## 9.8 Staging

* [x] Quick Weight
* [x] Advanced Weight
* [x] Configure Slot
* [x] Clear Staging
* [x] Tag zuordnen via Spoolman
* [x] Tag-Zuordnung entfernen via Spoolman

Hinweis: Quick Weight/Advanced Weight (Waage-Messwert, Bestätigungs-Overlay,
`SpoolmanCommandType::UpdateWeight`) sowie "Tag zuordnen via
Spoolman"/"Tag-Zuordnung entfernen via Spoolman" (`staging_action_link_tag`/
`staging_action_unlink_tag`, generische `AssignTag`/`RemoveTagAssignment`-
Abläufe) waren bereits vollständig und korrekt implementiert -- inklusive
korrekter Übergabe von `stagingState.spoolId` als Ziel-Spule über
`stagingActionClicked`.

Zwei echte Bugs gefunden und behoben:

1. **Configure Slot**: `staging_action_configure` ("Slot konfigurieren")
   löste bisher denselben `case`-Block wie `ConfigureSlotFromStaging` aus
   (Fallthrough) und war über einen `action.value == 1`-Zweig auf reine
   Navigation zum `TraySelect`-Bildschirm angewiesen. Dieser Zweig war aber
   nie erreichbar: `stagingActionClicked` sendet für `ConfigureSlot`
   grundsätzlich `value == 0`. Der Button versuchte dadurch, direkt einen
   Slot mit `amsId == 0`/`trayId == 0` zu committen, statt erst zur
   Slot-Auswahl zu navigieren. Fix: `ConfigureSlot` und
   `ConfigureSlotFromStaging` in zwei eigene `case`-Blöcke aufgeteilt --
   `ConfigureSlot` navigiert jetzt bedingungslos zu `TraySelect`, der
   eigentliche Commit bleibt exklusiv bei `ConfigureSlotFromStaging` (dem
   Tap auf einen Slot dort, siehe 9.1).

2. **Clear Staging**: Der Button zeigte bisher nur einen Mock-Dialog
   ("Diese Mock-Aktion kann eine Zuordnung entfernen. Fortfahren?") ohne
   jede Funktion -- weder AppTask noch UiBridge verarbeiteten eine
   Bestätigung. Fix: neues Pending-Flag
   `pendingClearStagingConfirmation` (gleiches Muster wie
   `pendingUnlinkConfirmation`), echter Bestätigungstext, und beim
   Bestätigen wird `UpdateStaging` mit `spoolId = 0` gesendet.
   `UiBridge::processUiCommand` behandelt `UpdateStaging` mit `spoolId ==
   0` jetzt als expliziten Leerungsfall (`stagingState`/
   `stagingSpoolState` auf Default zurückgesetzt, keine
   Spool-/Gewichts-Parsing-Logik). Spoolman und der NFC-Tag werden dabei
   bewusst nicht verändert -- es wird nur die lokale Staging-Anzeige
   geleert.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

## 9.9 Slot

* [x] Details
* [x] Spule
* [x] Configure from Staging
* [x] Configure Manually
* [x] Untag Slot
* [x] Reset
* [x] Reapply
* [x] Refresh

Hinweis: Fünf echte Lücken/Bugs gefunden und behoben.

1. **Details**: unverändert bereits vollständig (Slot-Tab: Status/Material/
   Farbe direkt aus dem MQTT-Report, siehe Phase 8.3/9.3-Vorarbeit).

2. **Spule**: Der "Spule"-Tab auf `TrayDetails` zeigte bisher hartkodiert
   "Spoolman-Zuordnung: nicht bekannt" -- mit dem (zum Zeitpunkt korrekten)
   Kommentar, AppTask halte keine Spoolman-Identität pro Slot vor. Das ist
   inzwischen falsch: `BambuTask::handleAssignTray` speichert die
   Spoolman-`spoolId` bereits seit Phase 8.5 dauerhaft in
   `conn->state.amsUnits[amsId].slots[trayId].spoolId` (von
   `bambuApplyReport()` nie angefasst, siehe docs/bambu-protocol.md), nur
   wurde dieser Wert nie bis zur UI durchgereicht. Fix: `spoolId` in
   `PrinterSlotStateData` -> `AppTask::syncAmsToUi` (`UpdateTrayDetails`,
   neues `command.spoolId`) -> `TrayUiEntry.spoolId` (neues Feld) ->
   `updateTrayDetails()` durchgezogen; der Spule-Tab zeigt jetzt die echte
   Spoolman-ID, wenn diese Sitzung eine Zuordnung erfolgte. Bewusst nicht
   ergänzt: ein Live-Abruf von Hersteller/Material/Restgewicht für diesen
   Tab (bräuchte einen eigenen Spoolman-Request-Zyklus) -- nur die
   tatsächlich bekannte ID wird gezeigt, nichts erfunden.

3. **Configure from Staging**: Der Button "Aus Staging" auf
   `TrayActions` sendete `selectedTraySpoolId` (die aktuell im Slot
   gemeldete Spule) statt `stagingState.spoolId` (die im Staging bereit
   liegende Spule) an `ConfigureSlotFromStaging` -- er hätte also
   bestenfalls die bereits vorhandene Zuordnung erneut gesendet, nie die
   tatsächlich gestagte Spule. In UiBridge.cpp behoben: `trayActionClicked`
   (gemeinsamer Handler für alle `tray_action_*`-Buttons) verwendet jetzt
   für `ConfigureSlotFromStaging` gezielt `stagingState.spoolId`, während
   Reapply/Reset/Untag/Refresh weiterhin `selectedTraySpoolId` (die
   aktuell im Slot bekannte Spule) nutzen -- beide Quellen bleiben pro
   Aktion korrekt getrennt, keine Bindung geändert.

4. **Configure Manually**: Der Button "Manuell" sendete `SelectSpool` mit
   Slot-Kontext, aber es gab dafür überhaupt keinen Verarbeitungspfad --
   der Klick landete im generischen Mock-Fallback
   ("Staging-Aktion vorgemerkt"). Fix: neue `SlotAssignmentStage::
   SelectingSpool`-Stufe in `pendingSlotAssignment` merkt sich
   amsId/trayId/printerId, während der generische Spoolman-Spulenpicker
   offen ist (der Picker selbst trägt keinen Slot-Kontext); die Auswahl
   committet über denselben Pfad wie `ConfigureSlotFromStaging`.

5. **Untag/Reset/Reapply/Refresh Slot**: alle vier waren reine
   Mock-Stubs ("Slot-Aktion vorgemerkt"). Fix, jeweils ohne neue
   MQTT-Nachrichtenformen (nur die bereits verifizierten `ams_filament_
   setting`/`pushall`-Kommandos aus docs/bambu-protocol.md):
   - **Reapply** ("Erneut anwenden") teilt sich jetzt den `case`-Block mit
     `ConfigureSlotFromStaging` -- `trayActionClicked` liefert dafür
     bereits `selectedTraySpoolId` (die aktuell im Slot bekannte Spule),
     wodurch derselbe Commit-Pfad genau das gewünschte "erneut senden"
     ergibt.
   - **Reset** ("Slot zurücksetzen") sendet `AssignTray` mit leerem
     `trayType`/`trayColorHex` und `spoolId = 0` -- Slot wird am Drucker
     als leer markiert, lokale Spoolman-Zuordnung ebenfalls entfernt.
   - **Untag** ("Zuordnung entfernen") sendet ebenfalls `AssignTray` mit
     `spoolId = 0`, aber mit dem aktuell vom Drucker gemeldeten
     `trayType`/`trayColorHex` unverändert übernommen -- nur die
     Spoolman-Zuordnung wird entfernt, der physische Slot-Inhalt bleibt
     wie zuletzt berichtet.
   - **Refresh** ("Slot aktualisieren") sendet `BambuCommandType::
     RequestStatus` für den aktuellen Drucker (bereits verifiziertes
     "pushall").
   Die AssignTray-Erfolgs-/Fehlermeldung unterscheidet jetzt "Slot
   konfiguriert" (Spule zugeordnet) von "Slot zurückgesetzt" (spoolId ==
   0 bei Reset/Untag).

Zusätzlich in `rtos::requiresOnlineSpoolman` (Commands.h) korrigiert:
`ConfigureSlot` navigiert seit dem 9.8-Fix nur noch zur Slot-Auswahl (kein
Spoolman-Zugriff mehr) und wurde aus der Liste entfernt, `ReapplySlot`
(neu: lädt eine Spoolman-Spule wie `ConfigureSlotFromStaging`) wurde
ergänzt.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

## 9.10 Zustandsautomat

* [x] Spoolman Required
* [x] Screens
* [x] Übergänge
* [x] Zurück
* [x] Abbruch
* [x] requestId
* [x] printerId
* [x] stale Responses
* [x] doppelte Aktionen
* [x] Tag entfernt
* [x] TagIdentity geändert

Hinweis: Verifikations-/Härtungsphase über den gesamten in 9.1-9.9
gebauten Zustandsautomaten (`pendingTagAssignment`, `pendingTagRemoval`,
`pendingSlotAssignment`, `pendingStagingSpool*`, die diversen
`pending*Confirmation`-Flags), kein Neubau. Ergebnis, Punkt für Punkt:

* **requestId/printerId/stale Responses**: `nextRequestId` (UiBridge.cpp)
  ist ein einziger, für die gesamte App durchlaufender Zähler -- jede
  Antwort wird ausschließlich über `event.requestId ==
  pending*.requestId` zugeordnet, das schließt Kreuz-Drucker-Verwechslungen
  strukturell mit aus (eine andere Anfrage hat immer eine andere
  requestId). Für Bambu-Events zusätzlich der schon in 8.3/9.1 gebaute
  Fokus-Gate (`event.printerId != printerCollection.activePrinterId`) für
  Header/AMS-Sync, während Slot-Zuordnungsergebnisse bewusst unabhängig
  vom fokussierten Drucker angezeigt werden (Kommentar an Ort und Stelle:
  der Nutzer wartet explizit auf genau diesen Vorgang).
* **doppelte Aktionen**: alle Start-Punkte (`AssignTag`,
  `RemoveTagAssignment`, `ConfigureSlotFromStaging`/`ReapplySlot`,
  `ResetSlot`/`UntagSlot`, die neue `SelectingSpool`-Stufe für "Manuell")
  prüfen `stage != None`, bevor sie eine neue Anfrage öffnen.
* **Tag entfernt / TagIdentity geändert**: bereits aus Phase 7.7 sehr
  gründlich abgedeckt -- `NfcTagRemoved` unterscheidet nach exaktem
  Pending-Stage, welche Teil-Rückmeldung (Mapping/Payload) noch zu retten
  ist; `assignmentTagMatches`/`removalTagMatches` werden unmittelbar vor
  jedem physischen Schreib-/Löschvorgang erneut geprüft und fangen damit
  auch den Fall ab, dass zwischenzeitlich ein anderer Tag aufgelegt wurde,
  ohne dass ein sauberes Removed-Event dazwischen kam (`currentTag` wird
  bei jedem `NfcTagRead` unbedingt überschrieben).
* **Abbruch**: hierbei die einzige echte Lücke gefunden und behoben. Der
  generische `Cancel`-Handler (der auch den gemeinsamen
  `overlayCancel`-Button aller ShowDialog-Overlays inkl. Spoolman-
  Spulenpicker bedient) setzte `pendingStagingSpoolSelection` nie zurück
  -- ein Abbruch des Staging-Spulenpickers ließ das Flag auf `true`
  stehen, wodurch der "Spule auswählen"-Button danach dauerhaft
  wirkungslos blieb (der nächste Tastendruck hielt den Picker
  fälschlich für bereits offen und tat mangels ausgewählter Spule gar
  nichts). Derselbe Fehler bestand strukturell für die neue
  `pendingSlotAssignment`-Stufe `SelectingSpool` (Phase 9.9 "Manuell").
  Beide jetzt im `Cancel`-Handler zurückgesetzt.
* **Screens/Übergänge/Zurück**: stichprobenartig für die neuen 9.9-Flows
  geprüft (Slot-Ergebnisdialoge bleiben bewusst auf `TrayActions`, kein
  Screen-Wechsel nötig; der Picker-Overlay-Backdrop blockiert Klicks auf
  darunterliegende Screen-Buttons, wodurch `Back` waehrend eines offenen
  Pickers ohnehin nicht erreichbar ist) -- keine weiteren Lücken
  gefunden.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

---

# Phase 10 – Robustheit und Diagnose

## 10.1 Task-Diagnose

* [x] Stack
* [x] Runtime
* [x] Queues
* [x] Event Bits
* [x] Heap
* [x] PSRAM

Hinweis: Der "Aktualisieren"-Button auf dem Diagnose-Bildschirm sendete
`RefreshDiagnostics`, das in AppTask.cpp bisher im generischen
Mock-Aktions-Block landete; das `diagnostics_settings_tasks`-Label zeigte
immer den hartkodierten Text "Tasks: 8 | Diagnose aktualisiert". Fix:
neue Funktion `logTaskDiagnostics()` in AppTask.cpp, die für alle 9 Tasks
(`RtosContext` haelt bereits jedes `TaskHandle_t`) `uxTaskGetStackHighWaterMark`
(Stack) und `eTaskGetState` (Runtime/Status: running/ready/blocked/
suspended) ausliest, für alle 9 Queues `uxQueueMessagesWaiting` gegen die
konfigurierte Laenge (Queues), sowie `xEventGroupGetBits` mit allen 9
bekannten Bits einzeln decodiert (Event Bits), zusaetzlich Heap/PSRAM
inkl. Minimalwert seit Boot (`ESP.getMinFreeHeap()`/`getMinFreePsram()`).

Das EEZ-Layout hat für diese Daten nur ein einzelnes 464x40px-Label --
zu wenig für eine Aufschlüsselung aller Tasks/Queues auf dem Bildschirm.
Der vollständige Bericht (jede Task/Queue einzeln, alle Event-Bits,
Speicher) geht daher als strukturierte `FS_LOGI`-Zeilen aus
(`LogComponent::Rtos`, per Seriell/Logdatei einsehbar -- gleiches Muster
wie die zuvor erweiterte Bambu-Kommunikationsprotokollierung); das kleine
Label zeigt nur die für den Bildschirm relevante Kurzzusammenfassung
(knappster Task-Stack + vollste Queue + Event-Bits als Hex). Wichtiges
Detail: `uxTaskGetStackHighWaterMark()` liefert auf dem ESP32-Xtensa-Port
Bytes (nicht Worte wie auf manchen anderen 32-Bit-Ports, da
`StackType_t` dort `uint8_t` ist) -- verifiziert, keine falsche
Umrechnung.

Zusätzlich Heap/PSRAM-Labels um den Minimalwert seit Boot ergänzt (real,
kein neuer Roundtrip nötig) und die veraltete "Mock bereit"-Platzhalter-
beschriftung beim ersten Öffnen des Bildschirms durch einen neutralen
Hinweis ersetzt, solange diese Sitzung noch nicht aktualisiert wurde.

Build (`pio run`, 0 Warnings) und native Tests (`pio test -e
native-spoolman-tests`, 44/44) erfolgreich, Firmware auf das Gerät
geflasht.

Nachtrag (Hardware-Test): mehrfaches, schnelles Drücken von
"Aktualisieren" führte auf echter Hardware zu einem Stack-Overflow-Absturz
in AppTask. `handleUiAction` ist eine sehr große, tief verzweigte
Funktion; der zusätzliche Aufruf-Frame von `logTaskDiagnostics()` (zwei
`std::array`-Locals + `DiagnosticsSummary`) hat einen bereits knappen
Spitzenwert überschritten -- dasselbe Bugmuster wie die früheren
ScaleTask-/NfcTask-Abstürze dieser Session. Fix: die drei Locals in
`logTaskDiagnostics()` sind jetzt `static` (AppTask verarbeitet ohnehin
nur eine UI-Aktion seriell, nie parallel/rekursiv, dieselbe Begründung
wie beim ScaleTask/NfcTask-AppEvent-Fix), zusätzlich `kAppTask` in
TaskConfig.h von 8192 auf 12288 Byte angehoben als Sicherheitsmarge.
Build/Tests erneut erfolgreich, Firmware geflasht; Bestätigung durch
erneuten Hardware-Test (mehrfaches Drücken) steht noch aus.

## 10.2 Speicher

* [x] SD entfernen
* [x] Stromausfall
* [x] HX711 trennen
* [x] PN532 trennen
* [x] langsame SD
* [x] JSON beschädigt
* [x] Backup

Bestandsaufnahme: die meisten dieser Szenarien waren durch die Phase-2/
StorageTask-Architektur bereits robust abgedeckt, ohne dass diese Phase
10.2 das explizit dokumentiert hatte. Zwei Luecken (PN532-Laufzeittrennung,
langsame-SD-Sichtbarkeit) waren echt und wurden neu geschlossen; ein
Stub (`CreateBackup`) war totes API und wurde entfernt statt implementiert
(Nutzerentscheidung).

**SD entfernen** -- bereits vorhanden: `StorageTask::storageTask()`
pollt `cardIsAccessible()` alle `kSdHealthCheckIntervalMs` (2000ms), setzt
bei Verlust `removalLatched`, loescht `EVENT_SD_READY`, sendet
`SdRemoved` und verriegelt alle weiteren Befehle bis zum Neustart
("SD unavailable; restart required") -- ein erneutes Einstecken wird
erkannt (`SdReinserted`), aendert aber bewusst nichts an der Verriegelung
(Neustart bleibt erforderlich, da der Zustand aller offenen
Dateihandles/Puffer nach einer Entfernung nicht mehr vertrauenswuerdig
ist).

**Stromausfall** -- bereits vorhanden (Phase 2.4): `JsonStorage::
atomicSave()` schreibt in eine `.tmp.json`, validiert sie, verschiebt die
bisherige Zieldatei nach `.bak.json`, benennt dann erst `.tmp.json` in
die Zieldatei um und loescht abschliessend `.bak.json` -- jeder Schritt
einzeln fuer sich abgesichert. `JsonStorage::recoverAtomicSave()` laeuft
beim Boot fuer jedes Initial-Dokument (`ensureInitialDocument()`) und
stellt nach einem Stromausfall mitten in dieser Sequenz automatisch den
zuletzt gueltigen Stand wieder her (Ziel gueltig -> Reste aufraeumen;
sonst `.tmp.json` falls gueltig uebernehmen; sonst `.bak.json`
zurueckspielen; sonst -- wenn alle drei fehlen -- als leerer/erster Boot
werten). Bereits per "Wiederherstellungstest" (Phase 2.4) verifiziert.

**HX711 trennen** -- bereits vorhanden: `ScaleTask::scaleTask()` erkennt
ausbleibende HX711-Samples ueber `kHx711ReadyTimeoutMs`, loescht
`EVENT_SCALE_READY`, sendet `ScaleError` ("HX711 not responding") und
erkennt eine Rueckkehr am naechsten erfolgreichen Sample (`ScaleReady`
erneut, Zustand automatisch zurueckgesetzt) -- kein Neustart noetig,
anders als bei SD.

**PN532 trennen** -- **Luecke geschlossen** (neu): Initialisierungs-
fehler beim Boot waren bereits abgedeckt (`initializePn532()` meldet
`NfcError` und setzt `EVENT_NFC_READY` nie), eine Trennung *waehrend*
des Betriebs (Kabel loest sich mitten in der Sitzung) wurde bisher aber
gar nicht erkannt -- `scanTarget()`-Kommunikationsfehler loesten nur
einen stillen `resetRfField()`-Wiederholversuch aus, ohne Obergrenze und
ohne Meldung an AppTask/UI; ein zu diesem Zeitpunkt aufgelegtes Tag waere
irgendwann faelschlich als "entfernt" gemeldet worden. Neu:
`sustainedCommErrors`-Zaehler (uebersteht die bestehenden weichen
RF-Resets, im Gegensatz zu `consecutiveScanErrors`) an allen
`scanTarget()`/`readPages()`-Erfolgs- und Fehlerstellen in `NfcTask.cpp`
verdrahtet (`notePn532Responding()`/`notePn532CommError()`); nach
`kPn532DisconnectConfirmationScans` (20, neu in `NfcConfig.h`) direkt
aufeinanderfolgenden Fehlern gilt der Leser als getrennt (`EVENT_NFC_READY`
geloescht, `NfcError` "PN532 not responding; check HSU wiring"), eine
erneute gueltige Antwort (auch ein simples "kein Tag gefunden") meldet
die Rueckkehr (`NfcInitialized` erneut, kein Neustart noetig).

**langsame SD** -- **Sichtbarkeit ergaenzt** (neu): strukturell bereits
robust (dedizierter StorageTask, kein anderer Task wartet synchron auf
SD-I/O, alle Queue-Operationen haben Timeouts statt unbegrenzt zu
blockieren) -- es fehlte aber jede Diagnose-Sichtbarkeit, falls eine
Karte degradiert (spuerbar langsamer statt komplett ausfallend). Neu:
`storageTask()` misst die Dauer jeder `processStorageCommand()`-
Bearbeitung und loggt ab `kSdSlowOperationWarningMs` (750ms, neu in
`BoardConfig.h`) eine `FS_LOGW`-Zeile mit request_id/Typ/Pfad/Dauer --
reine Diagnose, kein Abbruchverhalten.

**JSON beschädigt** -- bereits vorhanden: `JsonStorage::load()` prueft
Groessenlimits, Parse-Erfolg, Schema-Version, `updatedAt`-Format und
dokumenttyp-spezifische Feldvalidierung; jeder Fehlschlag liefert einen
spezifischen `JsonStorageError` statt eines Absturzes oder stillschweigend
uebernommener Teildaten. Fuer die in `kInitialDocuments` gelisteten
Kerndateien greift zusaetzlich die oben beschriebene Boot-Wiederherstellung
(`recoverAtomicSave()`); fuer die Legacy-NFC-Mapping-Dateien (nicht in
dieser Liste, nur On-Demand geladen) meldet ein beschaedigter/fehlender
Stand explizit "Legacy mapping file not found" statt eines Fehlers ohne
Kontext (`isMappingPath()`-Sonderfall in `processLoadCommand()`).

**Backup** -- **als totes API entfernt statt implementiert** (Nutzer-
entscheidung nach Rueckfrage): `StorageCommandType::CreateBackup`
existierte als Stub (lieferte immer `InvalidArgument`, wurde von nirgends
aufgerufen) und war ohnehin praktisch nicht sicher implementierbar, ohne
mit dem oben beschriebenen automatischen `.bak.json`-Mechanismus zu
kollidieren (derselbe Dateiname wird von `atomicSave()`/
`recoverAtomicSave()` als Crash-Recovery-Artefakt behandelt, nicht als
dauerhaftes manuelles Backup). Der eigentliche Schutz -- ein Backup bei
*jedem* Speichern, nicht nur auf Abruf -- ist durch Phase 2.4 bereits
vollstaendig abgedeckt; `CreateBackup` aus `StorageCommandType` entfernt,
mit Verweisen auf Phase 2.4 in `Commands.h` dokumentiert.

Build (0 Warnungen), 52 native Tests gruen, geflasht.

## 10.3 NFC/Spoolman-Zuordnung

* [x] Tag während Read entfernen
* [x] Tag während Assign entfernen
* [x] zwei Tags schnell
* [x] unbekannter NDEF
* [x] beschädigter NDEF
* [x] falsche Payload-Spool-ID
* [x] Bambu nie schreiben
* [x] OpenPrintTag nie schreiben
* [x] OpenTag3D nie schreiben
* [x] Unknown nie schreiben
* [x] `extra.tag` Duplicate
* [x] Spule existiert nicht
* [x] Spoolman fällt während Assignment aus
* [x] Spoolman Update erfolgreich / NFC Write fehlschlägt
* [x] Clear extra.tag erfolgreich / NFC Clear fehlschlägt
* [x] UID-Wechsel vor Verify
* [x] Tag-Feld fehlt
* [x] Tag-Feld falscher Typ

Bestandsaufnahme (kein neuer Code, reine Verifikation + Belege): anders
als bei 10.2 waren hier alle 18 Punkte bereits durch die bestehende
NfcTask/AppTask/TagWritePolicy/NfcPayload-Architektur abgedeckt, in vielen
Faellen mit expliziten nativen Tests. Geprueft per direkter Code-Lektuere
(Punkte 1-2, 16) und per Explore-Agent-Audit mit anschliessender
Stichprobenverifikation dreier kritischer Befunde (Rollback-Logik,
Konflikt-Dialoge, Malformed-NDEF-Test -- alle drei per Read gegengeprueft
und korrekt bestaetigt).

**Tag während Read entfernen** -- `NfcTask.cpp`s `reportTag()` liest
mehrere Bloecke nacheinander (Bambu-Keys, NDEF, Lock-Metadaten); jeder
Einzelschritt scheitert einzeln und ohne Absturz (`raw.ndefPresent`
bleibt false, `ntagWritableForPages()` liefert `resultKnown=false`), der
Hauptloop erkennt die Entfernung im naechsten Zyklus normal.

**Tag während Assign entfernen** -- `PendingTagAssignment`/
`PendingTagRemoval` haben ein `tagRemoved`-Feld (`AppTask.cpp` ~189-237),
ausgewertet u. a. bei 1802-1811 ("Tag wurde in Spoolman zugeordnet. Der
Tag wurde entfernt..."), 4152-4180, 4996-5003 -- die Spoolman-Zuordnung
bleibt bestehen (Server ist fuehrend), der Nutzer wird explizit gewarnt,
dass die physische Tag-Nutzlast nicht aktualisiert wurde.

**zwei Tags schnell** -- `InListPassiveTarget` fordert bewusst nur ein
Target an (`NfcTask.cpp:311-334`, "Activate at most one passive
ISO14443A target"), `uidLength` wird vor jedem Kopieren begrenzt
(324-327). Ein waehrend der Entfernungsbestaetigung neu gefundenes Tag
wird nicht verworfen, sondern direkt uebernommen (1222-1230).

**unbekannter NDEF** -- `TagParserRegistry::parse()` faellt auf
`TagFormat::Unknown` zurueck (`TagParserRegistry.cpp:93-114`, schreib-
und loeschgeschuetzt per TagWritePolicy). Tests:
`test_unknown_tag_has_no_definition_or_write_capability`,
`test_unknown_data_does_not_match_rejecting_parser`.

**beschädigter NDEF** -- `parseType2Ndef()` prueft jede TLV-Laenge/
-Position gegen die Puffergrenze (`NfcPayload.cpp:67-113`), `readNdef()`
in `NfcTask.cpp:532-596` ebenso (monoton wachsender, begrenzter
Seitenzeiger). Test `test_malformed_payload_is_rejected` (per Read
verifiziert: 3-Byte-TLV mit vorgetaeuschter Laenge 0x20 liefert
`NfcPayloadType::Invalid`, kein Crash).

**falsche Payload-Spool-ID** -- eine im NDEF codierte spoolId wird nie
direkt vertraut: `pendingNativeConsistency` gleicht die UID gegen
Spoolmans `extra.tag` ab und vergleicht das Ergebnis mit der Payload-ID;
`NotFound` und Mismatch erzeugen unterschiedliche Konfliktdialoge
("...in Spoolman fehlt die Zuordnung" bzw. "Spoolman ist fuehrend"),
`AppTask.cpp:4914-4972` (per Read verifiziert).

**Bambu/OpenPrintTag/OpenTag3D/Unknown nie schreiben** -- zentral in
`nfc/TagWritePolicy.h`s `capabilitiesFor()`: fuer alle vier Formate
bleibt `canWriteFilamentStationPayload`/`canClearFilamentStationPayload`
auf ihrem Default `false` (Kommentar: "Their original data is never
modified"). Test `test_assignment_effects_for_all_supported_tag_formats`
prueft alle vier Formate explizit in einer Schleife
(`MappingOnly`+`preserveOriginalContent=true`),
`test_removal_effects_for_native_and_bambu_tags` das Gegenstueck fuer
`removalEffect()`.

**`extra.tag` Duplicate** -- `TagLookupStatus::Duplicate`/
`SpoolmanTagDuplicate` ist in allen vier Ablaufpfaden verdrahtet:
Native-Konsistenzpruefung (4871-4878, 4915-4926), Entfernen (4980-4986),
Zuordnen (5023-5029) und die generische Tag-Aufloesung fuer nicht-native
Tags (4648-4657, Catch-all bei 4843-4893).

**Spule existiert nicht** -- eine in Spoolman auf eine mittlerweile
geloeschte Spule zeigende `extra.tag`-Zuordnung scheitert beim Staging-
Nachladen als `LoadSpool`-404, abgefangen im `SpoolmanError`-Handler
(`AppTask.cpp:5259-5266`, "Spule konnte nicht geladen werden"); beim
Gewichts-Update-Pfad zusaetzlich mit gezielterer Meldung ("Bitte eine
vorhandene Spule neu auswaehlen", 5240-5257).

**Spoolman fällt während Assignment aus** -- `SpoolmanError` deckt jede
`pendingTagAssignment`-Stufe ausser `None`/`SelectingSpool`/
`WritingPayload` (das laeuft ueber `NfcError`, siehe naechster Punkt) ab
(`AppTask.cpp:5205-5238`, per Read verifiziert): `SettingTarget`
versucht einen Rollback (`RollingBackPrevious`); scheitert der
Rollback ebenfalls, erscheint ein eigener "Zuordnung inkonsistent"-
Dialog statt eines generischen Fehlers.

**Spoolman Update erfolgreich / NFC Write fehlschlägt** -- neben dem
Tag-entfernt-Sonderfall (1804-1810) faengt ein genereller `NfcError`
waehrend `WritingPayload` (4292-4299) jeden anderen Schreib-/
Verifikationsfehler ab und ruft `reportAssignmentWriteFailure()` mit der
Standardmeldung "Tag wurde zugeordnet... Ein erneuter Versuch ist
moeglich" -- die Spoolman-Zuordnung bleibt in jedem Fall bestehen.

**Clear extra.tag erfolgreich / NFC Clear fehlschlägt** -- symmetrisch:
`NfcError` waehrend `ClearingPayload` (4301-4315) loggt
`mapping_removed=true payload_cleared=false` und zeigt "Zuordnung
teilweise entfernt"; ein UID-Wechsel beim Verify des Loeschens hat einen
eigenen Zweig in der `NfcTagErased`-Behandlung (4250-4264).

**UID-Wechsel vor Verify** -- `NfcTask.cpp`s `handleWrite()` vergleicht
per `sameUid()` nach dem erneuten Scan gegen die urspruengliche UID
(direkt gegengeprueft); `writePage()`s Retry-Schleife reaktiviert und
prueft ebenfalls dieselbe UID, bevor ein Seiten-Schreibversuch wiederholt
wird.

**Tag-Feld fehlt** -- OpenPrintTags `parseMain()` verlangt das
Pflichtfeld (Materialklasse) explizit vor Erfolg
(`OpenPrintTag.cpp:194-291`), OpenTag3D lehnt fehlendes `material`/
`vendor` ab (`OpenTag3D.cpp:116-123`); beide liefern `TagParseResult::
Invalid` statt out-of-bounds zu lesen.

**Tag-Feld falscher Typ** -- OpenPrintTags CBOR-Leser pruefen den
Major-Type vor der Interpretation (`readUnsigned`/`readText`/
`readColor`/`readNumber`, `OpenPrintTag.cpp:101-176`), jede Abweichung
liefert `false` -> `Invalid`. Test
`test_opentag3d_new_major_version_is_rejected_without_crash` (verfaelschtes
Versionsfeld, erwartet `knownFormat=true, payloadValid=false`, kein
Absturz).

Kein Code geaendert -- reine Verifikation. Kein Build/Test/Flash noetig.

## 10.4 Netzwerk

* [x] WLAN weg
* [x] Spoolman weg
* [x] langsame Antwort
* [x] ungültige Antwort
* [x] Reconnect
* [x] MQTT

Bestandsaufnahme: hier war das Muster genau umgekehrt zu 10.3 -- die
*Erkennung* von Verbindungsverlust (WLAN/Spoolman/Bambu) war ueberall
bereits sauber verdrahtet (Events, Event-Group-Bits, Fehlermeldungen),
aber die *automatische Wiederherstellung* fehlte an drei von sechs
Stellen komplett: das System erkannte einen Ausfall korrekt, unternahm
danach aber nichts von selbst -- ein manueller Neustart oder (bei
Spoolman/Bambu) ein zufaelliger WLAN-Blip war bisher die einzige
Erholung. Alle drei Luecken behoben; die anderen drei Punkte waren
bereits robust (nur verifiziert, kein Code geaendert).

**WLAN weg** -- **Luecke geschlossen**: `WifiSignal::Disconnected`/
`LostIp` loeschten bereits `EVENT_WIFI_CONNECTED` und meldeten den
Verlust (`NetworkTask.cpp`), aber nichts versuchte danach, die
Verbindung wiederherzustellen. Ursache gefunden: WiFiManagers eigener
ESP32-Auto-Reconnect (`WiFi_autoReconnect()`) ist im vendorten Quelltext
(`.pio/libdeps/.../WiFiManager/WiFiManager.cpp`) hinter dem Build-Makro
`esp32autoreconnect` versteckt, das dieses Projekt nirgends definiert --
bestaetigt per Volltextsuche. Neu: `networkTask()`s Hauptschleife wartet
jetzt beschraenkt (`kWifiReconnectIntervalMs`, 15s) statt unbegrenzt,
sobald konfiguriert-aber-nicht-verbunden und kein Config-Portal aktiv
ist, und ruft periodisch `WiFi.reconnect()` auf.

**Spoolman weg** -- **Luecke geschlossen**: `EVENT_SPOOLMAN_READY` wurde
ausschliesslich bei einem WLAN-Verlust geloescht (`AppTask.cpp`) -- faellt
Spoolman selbst aus, waehrend WLAN durchgehend verbunden bleibt (Server-
Neustart, Netzwerkproblem auf Spoolman-Seite), blieb das Bit
faelschlich gesetzt und nichts pruefte je erneut nach. Neu: `appTask()`s
Hauptschleife wartet jetzt beschraenkt (`kAppTaskIdleTickMs`, 5s) statt
unbegrenzt auf `portMAX_DELAY` und ruft alle `kSpoolmanHealthCheckRetryIntervalMs`
(30s) das bereits vorhandene `retrySpoolmanHealthCheckIfNeeded()` auf
(bereits fuer den Boot-Race-Fix gebaut, idempotent, meldet Fehlschlaege
bewusst still -- exakt das richtige Verhalten fuer einen
Hintergrund-Retry).

**langsame Antwort** -- bereits vorhanden: `SpoolmanTask::getJson()`
setzt `HTTPClient::setConnectTimeout()`/`setTimeout()` auf den
nutzerkonfigurierten, validierten `timeoutMs` (1000-60000ms, siehe
`JsonStorage::validate()`); `BambuTask`s `mqttClient.connect()` ist durch
`kBambuConnectTimeoutMs` begrenzt. Keine unbegrenzte Blockade moeglich.

**ungültige Antwort** -- bereits vorhanden: `getJson()` prueft
`status != HTTP_CODE_OK` und `deserializeJson()`-Fehler getrennt und
liefert je einen spezifischen Fehlertext; `parseSpool()`/`parseFilament()`
haben defensive `.is<>()`-Pruefungen vor jedem Feldzugriff.
`BambuTask::handleReportPayload()` faengt `deserializeJson()`-Fehlschlaege
fuer MQTT-Payloads ebenso ab (loggt, kehrt zurueck, kein Crash) --
gegengeprueft per Read.

**Reconnect** -- siehe "WLAN weg"/"MQTT"/"Spoolman weg": alle drei
Wiederherstellungsluecken behoben, dies war derselbe zugrundeliegende
Fehler an drei Stellen.

**MQTT** -- **Luecke geschlossen**: eine abgebrochene MQTT-Sitzung
(Drucker-Neustart, LAN-Aussetzer) wurde nur ueber ein explizites
`BambuCommandType::Connect` neu verbunden (nur bei Boot und
`WifiGotIp` ausgeloest, `AppTask::connectAllEnabledPrinters()`) --
`serviceConnections()` meldete den Verbindungsverlust (`BambuDisconnected`)
korrekt, unternahm aber nichts. Bemerkenswert: die Konstante
`kBambuReconnectBackoffMs` existierte bereits in `BambuConfig.h`, wurde
aber nirgends benutzt -- derselbe Musterfund wie `CreateBackup` in 10.2
(als totes API fuer ein geplantes, nie fertiggestelltes Feature). Neu:
`serviceConnections()` ruft jetzt fuer jede weiterhin `enabled` markierte
Verbindung periodisch (`kBambuReconnectBackoffMs`, 5s) das bereits
vorhandene `doConnect()` erneut auf (dieselbe Funktion, die auch ein
explizites Connect-Kommando nutzt) -- `mqttClient.connect()`s eigener
Timeout (`kBambuConnectTimeoutMs`, 8s) verhindert dabei von selbst ein
Haemmern bei durchgehend langsam scheiternden Verbindungen.

Build (0 Warnungen), 52 native Tests gruen, geflasht.

## 10.5 Mehrdrucker

* [x] Wechsel während MQTT
* [x] Wechsel während Update
* [x] offline
* [x] mehrere offline
* [x] aktiven löschen
* [x] Default löschen
* [x] alte Antwort
* [x] AMS weg
* [x] Daten trennen

Bestandsaufnahme (Explore-Agent-Audit + eigene Verifikation der Luecke):
acht von neun Punkten waren bereits robust -- die Mehrdrucker-Architektur
(`PrinterConnections`-Array, `printerId`-getaggte Events, `requestId`-
Abgleich vor jeder Zustandsaenderung) ist konsequent durchgezogen. Eine
echte Luecke gefunden und behoben: eine physisch entfernte AMS-Einheit
blieb fuer immer als "angeschlossen" markiert.

**Wechsel während MQTT** -- bereits vorhanden: jedes Bambu-Event traegt
`event.printerId`; `printerEntry(event.printerId)` aktualisiert immer die
Daten des *betroffenen* Druckers in `printerCollection`, unabhaengig vom
gerade fokussierten (`AppTask.cpp` ~5406-5418). Nur die sichtbare Header/
AMS-Anzeige aktualisiert sich zusaetzlich live, wenn
`event.printerId == printerCollection.activePrinterId` gilt
(~5604-5621) -- ein Bericht eines Hintergrund-Druckers kann die Anzeige
des fokussierten Druckers nicht verfaelschen.

**Wechsel während Update** -- bereits vorhanden: `pendingSlotAssignment`
traegt ein eigenes `printerId`-Feld; jeder Stufen-Handler
(LoadingSpool/LoadingFilament/WritingSlot) prueft `requestId` vor jeder
Zustandsaenderung, das abschliessende AssignTray-Ergebnis wird bewusst
unabhaengig vom aktuellen Fokus verarbeitet (Kommentar: "shown regardless
of which printer is currently in focus") und schreibt in
`pendingSlotAssignment.printerId`-skalierte Daten (TraySpoolCache-Eintrag,
`findPrinter(printerCollection, pendingSlotAssignment.printerId)`) statt
in den gerade aktiven Drucker.

**offline / mehrere offline** -- bereits vorhanden per Architektur:
`PrinterConnections` ist ein Array mit vollstaendig unabhaengigem Zustand
je Drucker (`mqttClient`/`state`/`reportedConnected`/seit 10.4 auch
`lastReconnectAttemptAt`); `serviceConnections()` iteriert alle Eintraege
unabhaengig, kein gemeinsamer/globaler "gerade offline"-Zustand. Mehrere
gleichzeitig offline Drucker sind dadurch kein Sonderfall, sondern nur N
unabhaengige Einzelfaelle.

**aktiven löschen / Default löschen** -- bereits vorhanden: die
`DeletePrinter`-UI-Aktion loescht `activePrinterId`/`isActive` explizit,
falls der geloeschte Drucker aktiv war (`AppTask.cpp` 2938-2969);
`removePrinterConfig()` (700-725) loescht `selectedPrinterId`, falls es
auf den entfernten Drucker zeigte, und befoerdert automatisch
`printers[0]` zum neuen Default, falls der entfernte Drucker Default war.

**alte Antwort** -- bereits vorhanden: `pendingPrinterTestRequestId`
verhindert einen zweiten gleichzeitigen Verbindungstest und wird vor der
Anwendung eines `BambuTestResult` gegengeprueft; jeder
`pendingSlotAssignment`-Handler prueft ebenso `event.requestId ==
pendingSlotAssignment.requestId`, bevor er eine Antwort uebernimmt -- eine
veraltete/ueberholte Antwort wird still verworfen statt angewendet zu
werden.

**AMS weg -- echte Lücke, behoben:** `bambuApplyReport()` setzte
`AmsState::present`/`amsCount` bisher nur je (beim ersten Erscheinen
einer AMS-Einheit im Bericht), nie zurueck -- eine physisch entfernte
AMS-Einheit blieb fuer den Rest der Sitzung faelschlich als
angeschlossen markiert (per Volltextsuche bestaetigt: `present = false`
wird nirgends im gesamten `src`-Baum geschrieben, nur der
Struct-Default). Ursache/Fix siehe `docs/bambu-protocol.md`-Nachtrag:
`ams.ams[]` erscheint nur bei einem vollen `pushall`, gilt dort aber als
vollstaendige Momentaufnahme -- `bambuApplyReport()` merkt sich jetzt,
welche AMS-`id`s im aktuellen (vollen) Bericht vorkommen, und setzt fuer
jede zuvor bekannte, jetzt fehlende Einheit `present=false`/
`connectionState=Offline`, plus neu berechnetes `amsCount`. Ein Bericht
ohne `ams.ams[]` (regulaeres `push_status`) aendert bewusst nichts. Neue
Tests `testApplyReportClearsAmsRemovedFromFullReport` (AMS 1 fehlt im
zweiten vollen Bericht -> present=false, amsCount=1) und
`testApplyReportPartialUpdateKeepsAmsPresence` (Bericht ganz ohne
`ams`-Schluessel laesst vorhandene Praesenz unangetastet).

**Daten trennen** -- bereits vorhanden: der persistierte
`TraySpoolCache` ist per `printerId+amsId+trayId` verschluesselt
(`models/TraySpoolCache.h`), echte Drucker-Trennung bestaetigt. Der
RAM-only-Cache fuer Restgewicht/K-Faktor (`traySpoolDetails`,
`AppTask.cpp`) ist bewusst nur nach `spoolId` (nicht druckerspezifisch)
organisiert -- korrekt so: eine Spoolman-Spule hat dasselbe Gewicht/
K-Faktor unabhaengig davon, in welchem Drucker-AMS sie gerade steckt,
eine druckerspezifische Aufteilung waere hier keine Trennung, sondern
unnoetige Duplizierung.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 10.6 Workflow

* [x] Spoolman beim Import weg
* [x] NFC während Wizard weg
* [x] Waage instabil
* [x] Queue voll
* [x] Antwort spät
* [x] Abbruch
* [x] doppelte Messung

Bestandsaufnahme (Explore-Agent-Audit + eigene Verifikation der Luecken):
fuenf von sieben Punkten bereits robust; zwei zusammenhaengende Luecken im
Wiege-Assistenten (`weightUpdate`, QuickWeight/AdvancedWeight) gefunden
und behoben -- der Erfolgspfad hatte anders als der Fehlerpfad keinen
Abgleich gegen einen mittlerweile ueberholten/abgebrochenen Vorgang.

**Waage instabil** -- bereits vorhanden (vorab selbst verifiziert):
QuickWeight/AdvancedWeight pruefen `!scaleStable` sowohl beim Start als
auch nochmal unmittelbar vor dem Bestaetigen (`AppTask.cpp` ~3616/3737
bzw. ~2456/2496) -- ein Messwert, der zwischen Start und Bestaetigung
instabil wird, wird erkannt, nicht stillschweigend uebernommen.

**Spoolman beim Import weg** -- bereits vorhanden: der `SpoolmanError`-
Zweig fuer die Legacy-Migration deckt alle drei verwendeten requestIds ab
und ruft `finishLegacyMigrationEntry(ctx, false, ...)`, die den naechsten
Eintrag anstoesst statt haengen zu bleiben; die Schleife terminiert mit
einer Zusammenfassung (nur geloggt, siehe unten) selbst wenn jeder
verbleibende Eintrag so fehlschlaegt.

**NFC während Wizard weg** -- bereits vorhanden: QuickWeight/
AdvancedWeight lesen die Spoolman-ID einmalig aus `stagingState.spoolId`
(selbst per `requestStagingSpool()` geladen) beim Start in
`quickWeight.spoolId`/`advancedWeight.spoolId` und verwenden diese
gespeicherte Kopie beim Bestaetigen weiter -- unabhaengig vom physisch
noch aufgelegten Tag; ein Entfernen des Tags waehrend des Wiegens kann die
Spoolman-Zuordnung nicht verfaelschen.

**Queue voll** -- bereits vorhanden: `sendWeightUpdate()` sowie die
Legacy-Migrations-Sendehelfer pruefen `xQueueSend` explizit; schlaegt das
Senden fehl, wird der jeweilige Zustand zurueckgesetzt und (im
interaktiven Wiege-Fall) ein Fehlerdialog gezeigt statt eines
haengenbleibenden Fortschrittsbalkens.

**Antwort spät -- echte Lücke, behoben:** `SpoolmanError` fuer
`weightUpdate` prueft bereits `weightUpdate.active && event.requestId ==
weightUpdate.requestId`, der Erfolgspfad (`SpoolmanWeightUpdated`,
`AppTask.cpp` ~4503) tat das bisher nicht -- er uebernahm jede
eintreffende Antwort bedingungslos. Jetzt mit demselben Abgleich versehen;
eine veraltete/ueberholte Antwort wird geloggt und verworfen statt einen
unerwarteten Erfolgsdialog auszuloesen. Die feste Wiederverwendung der
Legacy-Migrations-requestIds ueber alle Eintraege hinweg ist dagegen
unproblematisch: die Migrationsschleife ist strikt seriell (der naechste
Eintrag wird erst aus `finishLegacyMigrationEntry()` heraus angestossen,
nachdem die Antwort des vorherigen vollstaendig verarbeitet ist), es kann
also nie mehr als eine Anfrage gleichzeitig offen sein.

**Abbruch -- echte Lücke, behoben:** `Cancel` setzte bereits
`quickWeight.pending`/`advancedWeight.pending` zurueck, ruehrte aber
`weightUpdate` nie an -- brach der Nutzer *nach* dem Bestaetigen ab
(waehrend die Spoolman-Anfrage noch lief), blieb `weightUpdate.active`
bestehen; die spaeter eintreffende Antwort loeste dann einen unerwarteten
Erfolgs-/Fehlerdialog fuer einen laengst verlassenen Vorgang aus. Jetzt
setzt `Cancel` `weightUpdate = {}` zurueck -- die eigentliche
HTTP-Anfrage laeuft serverseitig zwar weiter (kein Abbruchkommando an
SpoolmanTask, wie bei anderen Pending-Zustandsautomaten dieses Projekts
auch), ihre Antwort wird dank des neuen Abgleichs oben aber korrekt als
veraltet erkannt und verworfen.

**doppelte Messung** -- bereits vorhanden fuer den Fall selbst
(`ScaleStable`/`ScaleUnstable` loesen nie automatisch eine Messung aus;
ein echtes Doppel-Tippen auf "Bestaetigen" wird durch
`quickWeight.pending`/`advancedWeight.pending`, vor dem Senden auf
`false` gesetzt, dedupliziert). Der einzige Restfall -- ein zweiter
Wiegevorgang (anderer Spool/Bildschirm) waehrend der erste noch offen
ist -- ist derselbe Mechanismus wie "Antwort spät"; zusaetzlich zur
Antwort-Absicherung jetzt auch am Start blockiert:
QuickWeightConfirmation/AdvancedWeightConfirmation lehnen einen neuen
Wiegevorgang mit "Wiegevorgang läuft bereits" ab, solange
`weightUpdate.active` noch gesetzt ist.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 10.7 Langzeit

* [x] Tasks blockieren korrekt
* [x] Critical Sections
* [x] Mutex
* [ ] mehrstündiger Test
* [x] Speicher
* [x] UI
* [x] Dateien
* [x] Reconnect
* [x] Logger

Bestandsaufnahme (eigene Verifikation der Blockier-/Critical-Section-/
Mutex-Punkte per Volltextsuche ueber alle 8 Tasks + Explore-Agent-Audit
fuer die restlichen Punkte, kritischer Fund selbst gegengeprueft): acht
von neun Punkten sind Code-Eigenschaften, die sich per Review verifizieren
lassen und bereits erfuellt sind (eine echte Luecke bei "Logger"
gefunden und behoben). "mehrstuendiger Test" ist bewusst NICHT abgehakt --
das ist keine Code-Eigenschaft, sondern ein tatsaechlicher
Dauerlauf-Test auf echter Hardware, den nur der Nutzer durchfuehren kann.

**Tasks blockieren korrekt** -- verifiziert: alle 8 Tasks (App/Storage/
Scale/Nfc/Spoolman/Bambu/Network/Ui) warten in ihrer Hauptschleife
ausschliesslich per `xQueueReceive`/`xQueueSelectFromSet` mit `portMAX_DELAY`
oder einem begrenzten Timeout -- keine einzige Busy-Loop im gesamten
`src`-Baum. `UiTask.cpp` ist dabei besonders bemerkenswert: die Wartezeit
richtet sich nach `ui::runLvglTimers()`s eigener Empfehlung (LVGLs
Animations-/Timer-Planung), nicht nach einem festen Intervall.

**Critical Sections** -- verifiziert: die einzige Stelle im gesamten
Projekt ist das HX711-Bit-Banging in `ScaleTask.cpp` (~91-103,
`portENTER_CRITICAL`/`portEXIT_CRITICAL` um exakt die 24+1 GPIO-Takt-
Zyklen) -- kurz, keine blockierenden/Queue-Aufrufe darin, korrekt fuer
zeitkritisches Bit-Banging eingesetzt.

**Mutex** -- verifiziert: keine einzige `SemaphoreHandle_t`/
`xSemaphoreCreateMutex` im gesamten `src`-Baum. Die Architektur verzichtet
komplett auf geteilten Speicher zwischen Tasks -- jede Kommunikation laeuft
ueber Message-Queues mit wertkopierten Structs, wodurch eine ganze Klasse
von Mutex-Bugs (Deadlocks, Prioritaetsinversion, vergessenes Unlock) von
vornherein nicht existieren kann.

**mehrstündiger Test** -- **nicht abgehakt, erfordert echte Hardware:**
dies ist kein statisch pruefbarer Code-Aspekt, sondern ein tatsaechlicher
mehrstuendiger Dauerlauf auf dem Geraet (Speicherverlauf, Queue-Fuellstaende,
Reconnect-Zyklen ueber Zeit beobachten). Die dafuer noetige Diagnose-
Infrastruktur (Heap/PSRAM-Tiefstand seit Boot, jetzt auch verworfene
Logzeilen, siehe "Logger" unten) steht bereit; der eigentliche Test bleibt
dem Nutzer ueberlassen.

**Speicher** -- bereits vorhanden: `logTaskDiagnostics()` (Phase 10.1)
loggt `ESP.getMinFreeHeap()`/`getMinFreePsram()` (Tiefstand seit Boot --
der Standardweg, einen langsamen Leck ohne Live-Mehrstundentest zu
erkennen). Repo-weite Suche nach `new `/`malloc(`/`calloc(` findet nur
`heap_caps_malloc` fuer die beiden LVGL-Zeichenpuffer
(`UiBridge.cpp::initializeLvgl()`), einmalig beim Boot alloziert -- keine
einzige dynamische Allokation pro Schleifendurchlauf/Ereignis irgendwo im
Projekt (entspricht den in AGENTS.md/Codekommentaren wiederholt
referenzierten "static/stack/queue-value-type"-Konventionen).

**UI** -- bereits vorhanden: alle `_create()`-Aufrufe in `UiBridge.cpp`
ausserhalb der einmaligen Boot-Initialisierung sind entweder idempotent
(`ensureOverlay()` erstellt seine Overlay-Widgets nur beim ersten Aufruf,
schuetzt sich per Null-Check) oder korrekt gepaart (Spoolman-/Drucker-
Feld-Editoren loeschen ihr vorheriges Widget explizit vor dem Neuerstellen).
Der einzige echte Pro-Ereignis-Ersteller ist der Touch-Marker
(`showTouchMarker()`, bei jedem Touch-Down neu erzeugt), der sich aber
selbst per `lv_timer_create(deleteTouchMarker, 2000, ...)` nach 2s wieder
entfernt -- begrenzt, kein Aufstauen.

**Dateien** -- bereits vorhanden: jedes `.open()` in `JsonStorage.cpp`/
`StorageTask.cpp` schliesst auf jedem Ausstiegspfad, inklusive frueher
Fehler-Returns -- `isValidDocumentFile()` (von `atomicSave()`/
`recoverAtomicSave()` wiederholt aufgerufen) schliesst unbedingt vor jedem
Return; `atomicSave()`s eigene `temporaryFile` wird vor allen moeglichen
Fehlerpfaden geschlossen.

**Reconnect** -- bereits vorhanden (Ressourcen-Aspekt der in 10.4 neu
gebauten Logik, nicht die Logik selbst): `BambuTask.cpp`s
`PrinterConnection` haelt `tlsClient`/`mqttClient` als Wert-Member fuer
die gesamte Prozesslaufzeit -- `doConnect()` ruft nur Methoden auf den
bestehenden Objekten auf, kein `new WiFiClientSecure`/`new PubSubClient`
je Reconnect-Versuch. `WiFi.reconnect()` (NetworkTask) und
`retrySpoolmanHealthCheckIfNeeded()` (AppTask) allozieren nichts und
senden hoechstens einen einzelnen, groessenbegrenzten Command-Struct in
eine bereits begrenzte Queue.

**Logger -- echte Lücke, behoben:** `enqueueLogLine()`
(`RtosContext.cpp`) verwarf eine Zeile bei voller Queue bereits korrekt
begrenzt (10ms Wartezeit statt `portMAX_DELAY`, kein unbegrenztes
Wachstum) -- das Verwerfen selbst war aber vollstaendig unsichtbar: der
`xQueueSend`-Ruckgabewert wurde nie geprueft, keine Zaehlung, keine
Diagnose-Sichtbarkeit. Bei einem Logburst waehrend eines echten Fehlers
(genau der Moment, in dem Logzeilen am wichtigsten sind) haetten Zeilen
spurlos verschwinden koennen. Neu: `std::atomic<std::uint32_t>
droppedLogLines` in `RtosContext.cpp` (lock-frei, da aus jedem Task per
FS_LOG* erreichbar), neue Funktion `rtos::droppedLogLineCount()`;
`logTaskDiagnostics()` loggt den kumulierten Wert seit Boot direkt neben
der Heap/PSRAM-Diagnose.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 10.8 RAM-Optimierung (Nutzerwunsch 2026-08-25)

Untersucht, welche groesseren statischen RAM-Bloecke sich nach PSRAM
verschieben lassen. Groesster (und einziger relevanter) Befund: 19
funktionslokale `static rtos::AppEvent`-Puffer verteilt ueber
`NfcTask.cpp` (8x), `SpoolmanTask.cpp` (9x), `BambuTask.cpp` (1x) und
`AppTask.cpp` (1x, die Hauptschleife) -- `AppEvent` ist mit `PrinterState`,
`BambuConfigCollection`, `TraySpoolCache`, `TagReadResult` u. a. als
Werte eingebettet gross (~3KB je Instanz, durch bestehende Code-
Kommentare "(~3KB)" bereits dokumentiert und durch den finalen RAM-
Ruckgang exakt bestaetigt). Diese Puffer sind bewusst `static` (nicht
stack-lokal), um fruehere Stack-Overflow-Abstuerze zu vermeiden -- als
`static`/globales `.bss` landen sie aber IMMER im internen RAM, komplett
unabhaengig von PSRAM-Einstellungen.

Zwei moegliche Mechanismen geprueft:
- `EXT_RAM_ATTR` (verschiebt `.bss`-Variablen automatisch nach PSRAM) --
  **nicht wirksam in diesem Build**: das dafuer noetige
  `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` ist im mitgelieferten
  `sdkconfig` des Arduino-ESP32-S3-Frameworks nicht gesetzt (per
  Volltextsuche im Framework-Paket bestaetigt), und laesst sich mit der
  Arduino-Framework-Einbindung (kein reines ESP-IDF) nicht praktikabel
  nachtraeglich aktivieren.
- `heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` -- **der
  bereits bewaehrte Weg**, exakt das Muster, das `UiBridge.cpp::
  initializeLvgl()` schon fuer die (deutlich groesseren) LVGL-Zeichenpuffer
  nutzt. Automatisches Malloc-Routing (`CONFIG_SPIRAM_USE_MALLOC=y` ist
  gesetzt) greift hier nicht, da dessen Schwelle
  (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096`) ueber der AppEvent-Groesse
  liegt -- explizite Allokation war also so oder so noetig.

Umsetzung: neuer gemeinsamer Helfer `services/PsramAlloc.h`
(`allocatePsramInstance<T>()`), alloziert einmalig eine Null-
initialisierte PSRAM-Instanz per `heap_caps_malloc()` +
Placement-New; schlaegt die Allokation fehl, haelt der *aufrufende* Task
permanent an (`ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`-Schleife, gleiches
Fail-Fast-Muster wie bei anderen unwiederherstellbaren Init-Fehlern in
diesem Projekt) -- liefert nie einen Null-Zeiger. Alle 19 Stellen
umgebaut (`static T x{};` -> `static T* x = allocatePsramInstance<T>
("...");`, jeder Feldzugriff von `x.feld` auf `x->feld`, `xQueueSend(...,
&x, ...)` auf `xQueueSend(..., x, ...)`); fuer die 18 kurzen "einmal
aufbauen, einmal senden"-Funktionen von Hand, fuer `AppTask::appTask()`s
Hauptschleife (~2100 Zeilen, 480 Vorkommen von `event.`) gezielt per auf
den Funktionskoerper begrenztem `sed 's/\bevent\./event->/g'` --
selbstverifizierend, da jede vergessene Stelle ein Compilerfehler gewesen
waere (keine einzige aufgetreten).

Ergebnis (gemessener PlatformIO-RAM-Bericht, nicht geschaetzt): **RAM-
Nutzung sank von 39,3 % (128.780 Byte) auf 21,3 % (69.804 Byte) --
58.976 Byte (~57,6 KB) internes RAM freigemacht**, in zwei Bauschritten
gemessen (SpoolmanTask.cpp allein: 128.780 -> 100.844 Byte, danach die
restlichen drei Dateien: 100.844 -> 69.804 Byte). Deckt sich nahezu exakt
mit der manuellen Vorabschaetzung (19 x ~3,1KB ~= 59KB).

Kleinere, nicht mitgenommene Kandidaten fuer eine spaetere Runde (nicht
Teil der genehmigten 19 AppEvent-Stellen): `NfcTask.cpp`s `static
models::RawTagData raw{}` (~680 Byte, einmalig in `reportTag()`) und die
NDEF-Puffer `bytes`/`verify` (`std::array<std::uint8_t,
kNfcMaxNdefBytes=384>`, mehrfach in `handleWrite()`/`handleErase()`) sowie
`StorageTask.cpp`s `static rtos::StorageCommand command{}`
(~880 Byte, einmalig in der Hauptschleife) -- einzeln deutlich kleiner als
AppEvent, in Summe aber noch ein paar KB.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

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
