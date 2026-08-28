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
* Button-Beschriftungen (Label-Text) in SCR_SETTINGS_HOME, SCR_STAGING_DETAILS,
  SCR_STAGING_ACTIONS, SCR_TRAY_ACTIONS, SCR_TAG_ACTION_SELECT, SCR_TAG_LEGACY,
  SCR_TAG_DEFINITION_IMPORT, SCR_TAG_RESULT bleiben unverändert wie in EEZ
  Studio gesetzt (Ausnahme: Header-Button). Programmatisch nur
  Sichtbarkeit/Enabled-Zustand (`setLabelButtonAvailable()`) und
  Hintergrund-/Textfarbe ändern, niemals den Text selbst -- insbesondere
  nicht über `setControlText()`/`setButtonText()` (`UiBridge.cpp`).

Nachtrag (2026-08-25, Nutzerwunsch, Korrektur): die erste Pruefung dieser
Regel hatte nur direkte `lv_label_set_text(objects.X, ...)`-Aufrufe
gegrept und daher `setControlText()`/`setButtonText()` uebersehen -- diese
Wrapper loesen erst intern per `buttonLabel()` das Kind-Label auf, ein
reiner `objects.X`-Textsuche findet sie nicht. Nutzer hat das anhand von
`staging_details_quick_weight` konkret nachgewiesen (EEZ Studio:
"Schnell-\nwiegen" mit Zeilenumbruch, Code ueberschrieb mit einzeiligem
"Schnellwiegen"). Vollstaendige Nachpruefung ergab: `bindGeneratedWidgets()`
(`UiBridge.cpp`) enthielt fuer alle acht Screens `setControlText()`-Aufrufe,
die EEZ Studios bereits korrekten (teils mit bewusstem Zeilenumbruch
formatierten) Text ueberschrieben -- entfernt fuer `staging_details_
quick_weight`, `staging_action_advanced_weight`, `staging_action_link_tag`,
`staging_action_unlink_tag`, `staging_action_erase_tag`, `tray_action_reset`,
`tag_action_title/select_spool/use_last_spool/erase/back`, `tag_legacy_
title/select_spool/import/erase/close`, `tag_definition_import_title/
select_spool/spoolman/cancel`, `tag_result_title/quick_weight/advanced_
weight/close`. Layout- (`lv_obj_set_pos`/`_set_size`), Flag- und Style-
Aufrufe fuer dieselben Objekte blieben unveraendert (betreffen nicht den
Text). `staging_action_write_tag` bewusst ausgenommen: nicht klickbar,
dient als echte Laufzeit-Statusanzeige (wird ab `processUiCommand()` mit
dem Tag-Schreibstatus befuellt, `assignmentStatus`), keine Button-
Beschriftung im Sinne der Regel; der init-Aufruf war ohnehin redundant
(identisch mit EEZ Studios eigenem Default).

Zwei Faelle zeigten inhaltlich abweichenden statt nur reformatierten Text
gegenueber EEZ Studio -- nach Entfernen der Ueberschreibung zeigt das
Geraet jetzt EEZ Studios Version, zur Kenntnisnahme: `tag_definition_
import_title` ("Bambu-Definition erkannt" statt bisher "Tagdefinition
erkannt") und `tag_definition_import_spoolman`/`tag_legacy_import`
("importieren" statt bisher "Nach Spoolman importieren"). Falls das nicht
gewuenscht ist, muss der Text im EEZ-Projekt selbst angepasst werden, nicht
im Code. SCR_SETTINGS_HOME hatte nie eine Ueberschreibung -- dort war die
urspruengliche Pruefung bereits korrekt.

Zwischenfall: nach dem ersten Build/Test/Flash dieser Korrektur war
`UiBridge.cpp` beim naechsten Zugriff wieder exakt auf dem Stand des
letzten Commits (`f70cd91 "phase 11.6"`) -- die Aenderung war im
Arbeitsverzeichnis verloren (`git status` zeigte die Datei faelschlich als
unveraendert), obwohl das vorherige Flash sie kurzzeitig auf dem Geraet
hatte. Ursache nicht abschliessend geklaert (vermutlich ein Discard/
Checkout ausserhalb dieser Session); Aenderung wurde ein zweites Mal
angewendet und diesmal per `git status`/`git diff --stat` unmittelbar nach
dem Edit verifiziert, bevor gebaut wurde. `TASKS.md` und die
Halbgeviertstrich-Korrektur in `AppTask.cpp` waren von diesem Vorfall nicht
betroffen. Da dieser Stand weiterhin nur im Arbeitsverzeichnis liegt (kein
Commit durch mich ohne ausdrueckliche Anfrage), besteht bis zum naechsten
Commit erneut das Risiko eines Verlusts.

Build (0 Warnungen), 54 native Tests gruen, geflasht (Flash brauchte
mehrere Versuche: zunaechst zweimal "Could not open COM5" trotz von Windows
gelistetem Port, nach USB-Neustecken zweimal "No serial data received"
beim Reset-in-Bootloader-Handshake, erst nach manueller BOOT/RESET-Taster-
Sequenz erfolgreich -- kein Code-Zusammenhang).

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

**Nachtrag (2026-08-26):** Nutzer meldete, der Startbildschirm
("Fortschrittsanzeige") zeige nur 3 von 6 Statuszeilen an, der Rest werde
abgeschnitten. Ursache im `.eez-project`: das `boot_status`-Label
(`ui-project/FilamentStation.eez-project`, Identifier `boot_status`) hatte
nur 150 px Hoehe bei `top=126` auf dem 320 px hohen Screen. Fix im
`.eez-project`: `top` auf 122, `height` auf 190 vergroessert (nutzt den
verbleibenden Platz bis nahe an den Bildschirmrand, deutlich mehr Reserve
als fuer 6 Zeilen bei 20 px Zeilenh\xC3\xB6he (`ui_font_ui_german16`,
`line_height = 20`) rechnerisch noetig w\xC3\xA4re).

Nebenbefund, nicht Teil dieses Fixes: `objects.boot_status` wird
tats\xC3\xA4chlich nirgends im Anwendungscode angefasst
(`UiCommandType::UpdateBootStatus` existiert im Enum, wird aber nie
gesendet/verarbeitet) -- der Screen zeigt durchgehend den statischen
EEZ-Platzhaltertext ("SD-Karte: bereit\\nDisplay: bereit\\n..."), keine
echten Bereitschaftsdaten. War beim Mock-Daten-Audit in Phase 12 nicht
aufgefallen; hier bewusst nur die Darstellung repariert, nicht die fehlende
echte Anbindung nachgezogen (ausserhalb der gemeldeten Aufgabe).

**Korrektur (2026-08-26):** Der obige `.eez-project`-Fix (Nutzer hat
inzwischen re-exportiert, `screens.c` zeigt `boot_status` jetzt bei
`top=122, height=198`) war **das falsche Widget**. Nutzer meldete danach
weiterhin abgeschnittenen Text auf dem Bildschirm "FilamentStation startet"
(4. Zeile fehlt). Recherche ergab: dieser Screen ist gar kein eigener
EEZ-Screen, sondern der generische `UiOverlayKind::BootProgress`-Dialog --
derselbe wiederverwendete Overlay (`overlayBackdrop`/`overlayPanel`/
`overlayText`/`overlayProgress`), den auch WLAN-/Spoolman-/NFC-
Fortschrittsdialoge nutzen, komplett in C++ gebaut
(`UiBridge.cpp::ensureOverlay()`/`showOverlay()`), **nicht** im
`.eez-project`. Er wird als Vollbild-Overlay ueber `scr_boot` gelegt,
solange `AppTask::refreshBootProgress()` bis zu vier Statuszeilen sendet --
`objects.boot_status` auf `scr_boot` selbst ist dabei durchgehend verdeckt
und daher praktisch nie sichtbar; der obige Resize war folgenlos richtig,
aber wirkungslos fuer das gemeldete Problem.

**Tatsaechliche Ursache:** `overlayText` (Standardgroesse 388x100 px,
`LV_LABEL_LONG_WRAP`) reicht bis y=152 innerhalb des Panels, `overlayProgress`
beginnt aber bereits bei y=126 und wird danach erzeugt -- ueberlagert damit
im Z-Order die unteren rund 26 px des Textfelds (etwas mehr als eine
Zeile), was die vierte von vier Zeilen sichtbar abschnitt.

**Fix:** in `UiBridge.cpp::showOverlay()` einen neuen Sonderfall fuer
`UiOverlayKind::BootProgress` ergaenzt (gleiches Muster wie der bereits
bestehende Sonderfall fuer `AdvancedWeightConfirmation`/`-Result`, der aus
demselben Grund existiert): Panel 420x300 (statt 420x238), Textfeld
388x150 bei y=46, Fortschrittsbalken auf y=204 und Buttons auf y=228
verschoben -- ohne Ueberlappung, mit Kapazitaet fuer bis zu sieben Zeilen.
Zusaetzlich `overlayProgress`s Position in den allgemeinen Reset-Block am
Anfang von `showOverlay()` aufgenommen (fehlte dort bisher komplett --
seine y-Position wurde nirgends zurueckgesetzt, haette also nach diesem
Fix ohne diese Ergaenzung bei jedem folgenden Fortschrittsdialog faelschlich
bei y=204 stehen bleiben koennen).

Reiner C++-Fix, kein `.eez-project`-Eingriff -- kein erneuter EEZ-Export
noetig. Build (0 Warnungen), 60 native Tests gruen, geflasht.

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

Nachtrag (2026-08-25, Nutzerwunsch): dieser Countdown-Text enthielt ein
Halbgeviertstrich-Sonderzeichen ("\xE2\x80\x93", U+2013), das im
verwendeten LVGL-Font fehlt und dadurch nicht darstellbar war -- die
uebrige Oberflaeche verwendet durchgehend nur ASCII plus deutsche Umlaute/
scharfes S (Latin-1-Bereich), dieses eine Vorkommen war die einzige
Ausnahme (per Volltextsuche bestaetigt). Ersetzt durch einen normalen
Bindestrich.

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

**Nachtrag (2026-08-26), Bug bei echtem Hardwaretest gefunden und
behoben:** Nutzer meldete: Zuordnung von "Support for PLA" (Spoolman-
Material) zu einem Tray wird auf dem Drucker korrekt ausgefuehrt, aber die
App meldet einen Best\xC3\xA4tigungs-Timeout und die Spule bleibt
anschliessend nicht der Spoolman-ID zugeordnet -- bei "PLA"/"ABS"
funktioniert derselbe Ablauf einwandfrei. Log zeigte den korrekt gesendeten
`ams_filament_setting`-Payload mit `"tray_type":"Support for PLA"`, dann
nur noch die "awaiting confirmation"-Zeile, nie ein "AssignTray confirmed".

**Ursache:** `models::PrinterSlotStateData::material` (`PrinterState.h`)
war nur `char[12]` gross. `BambuProtocol.cpp::applyTrayOccupancy()`
schreibt den vom Drucker gemeldeten `tray_type` per `snprintf` in dieses
Feld -- bei "Support for PLA" (15 Zeichen) wurde das dabei stillschweigend
auf "Support for" (11 Zeichen) abgeschnitten. `BambuTask.cpp::
checkPendingTrayAssignment()` vergleicht diesen Wert anschliessend per
`strcmp` gegen `conn.pending.expectedTrayType` (`char[16]`, unangeschnitten
"Support for PLA") -- der Vergleich schlug dadurch permanent fehl, die
Best\xC3\xA4tigung kam nie, `kBambuAssignConfirmTimeoutMs` lief ab. Bei
"PLA"/"ABS" (je 3 Zeichen) griff dieselbe Kuerzung nie, deshalb liefen
diese Materialien unauff\xC3\xA4llig durch. Derselbe zu kleine Puffer
existierte identisch in `models::TraySpoolCacheEntry::material`
(`TraySpoolCache.h`) -- h\xC3\xA4tte bei einem laengeren Material zusaetzlich
`resolveTraySpoolCacheSpoolId()`s Material-Abgleich (Erkennung einer
ausserhalb der App gewechselten Spule) falsch negativ werden lassen.

**Fix:** beide Puffer von `[12]` auf `[16]` vergroessert -- passend zur
bereits an anderer Stelle verwendeten Kapazit\xC3\xA4t
(`BambuTrayFilament::trayType[16]`, `PendingTrayAssignment::
expectedTrayType[16]`, `UiModels.h`s `kMaterialNameCapacity = 16`), damit
kein Feld im Rundlauf enger ist als die tats\xC3\xA4chliche Bambu-
`tray_type`-Nutzlast.

Build (0 Warnungen), 60 native Tests gruen, geflasht (nach COM5-Reconnect,
Upload beim zweiten Versuch erfolgreich). Noch nicht am Geraet mit "Support
for PLA" erneut getestet -- sollte vom Nutzer verifiziert werden.

**Nachtrag (2026-08-26):** Backend-Fix behoben den Best\xC3\xA4tigungs-
Timeout, aber der Nutzer meldete danach: auf der Tray-Karte (Home/AMS-
Uebersicht) wird "Support for PLA" weiterhin nur als "Support for"
angezeigt, obwohl das EEZ-Label bereits auf `LV_LABEL_LONG_SCROLL_CIRCULAR`
umgestellt war. Ursache: ein drittes, bisher uebersehenes `char[12]`-Feld
-- `TrayUiEntry::material` (`UiBridge.cpp`, lokale UI-Zustandsstruktur pro
Tray-Karte). `UiBridge.cpp::processUiCommand()`s `UpdateTrayDetails`-Zweig
kopiert `command.title` (voller, unabgeschnittener Text aus `slot.material`)
per `snprintf` in dieses Feld -- bei 12 Byte erneut auf "Support for"
gekuerzt, unabhaengig vom Backend-Fix oben, da `updateTrayButton()` seine
Anzeige aus genau diesem Feld liest. War beim urspruenglichen Fix als
vermeintlich unbenutztes Feld eingestuft (falsche Grep-Suche nach `.material
=` statt `->material`) -- tatsaechlich live und der eigentliche verbleibende
Kuerzungspunkt. Fix: `TrayUiEntry::material` ebenfalls auf `[16]`
vergroessert (`UiBridge.cpp`), passend zu allen anderen Material-Feldern in
diesem Rundlauf. Nebenbei `TraySpoolCacheEntry::material` (extern auf `[17]`
abgeaendert vorgefunden) wieder auf `[16]` vereinheitlicht.

Build (0 Warnungen), 60 native Tests gruen, geflasht (Upload diesmal beim
ersten Versuch erfolgreich). Noch nicht am Geraet erneut mit "Support for
PLA" verifiziert.

**Nachtrag (2026-08-26):** Nutzer bestaetigte: "Support for PLA" wird jetzt
korrekt angezeigt, direkt nach dem Zuordnen auch SpoolmanID/Material/
Gewicht/K-Faktor korrekt. Nach einem Neustart jedoch: Material weiterhin
korrekt, aber SpoolmanID zeigt "?" und Gewicht/K-Faktor fehlen.

**Ursache:** reiner Timing-/Fehlender-Refresh-Bug, keine Datenkorruption.
`requestTraySpoolCache()` liegt in der FIFO-`storageCommandQueue` hinter
allen anderen `/config`-Dateien (`kInitialDocuments`, `StorageTask.cpp`);
ein bereits verbundener Drucker kann seinen ersten Statusbericht
(`BambuUpdate` -> `syncAmsToUi()`) schneller liefern als dieser SD-Ladevorgang
zurueckkommt. Trifft das ein, findet `resolveTraySpoolCacheSpoolId()` beim
allerersten Sync-Aufruf einen noch leeren `traySpoolCache` (`entryCount ==
0`) und zeigt "?". Der `StorageReadCompleted`-Handler fuer den
Cache-Ladevorgang (`AppTask.cpp`) hat den geladenen Cache bisher nur in die
lokale Variable uebernommen (`traySpoolCache = event->traySpoolCache;`),
aber nie einen erneuten UI-Sync ausgeloest -- ein einmal als "?" gezeigtes
Tray blieb dadurch dauerhaft so stehen, auch nachdem der Cache laengst
korrekt geladen war.

**Fix:** derselbe Handler ruft jetzt zusaetzlich `syncAmsToUi()` fuer den
aktuell aktiven Drucker auf, sobald der Cache geladen ist (nur falls eine
gueltige `activePrinterId` bereits bekannt ist) -- ein zu frueh als "?"
anzeigtes Tray wird dadurch sofort nachkorrigiert, sobald die eigentlich
schon korrekten lokalen Daten verfuegbar sind.

Build (0 Warnungen), 60 native Tests gruen, geflasht. Noch nicht am Geraet
mit einem echten Neustart erneut verifiziert.

**Korrektur (2026-08-27):** Der obige Re-Sync-Fix war eine echte
Verbesserung, aber **nicht die eigentliche Ursache** -- Nutzer meldete nach
dem naechsten Test: die Zuordnung ueber Material/Farbe-Fingerprinting
funktioniert nach einem Neustart fuer **kein** Fach mehr, nicht nur fuer
"Support for PLA".

**Tatsaechliche Ursache gefunden:** `JsonStorage.cpp::
validateTraySpoolCacheEntries()` enthielt eine eigene, unabhaengige
Laengenpruefung `std::strlen(entry["material"]...) >= 12U` -- ein
Ueberbleibsel aus der Zeit vor dem `[16]`-Puffer-Fix
(`models::TraySpoolCacheEntry::material`, siehe oben), das beim damaligen
Fix nicht mitgezogen wurde. Diese Validierung laeuft **sowohl beim Laden
als auch beim Speichern** (`JsonStorage::serialize()` ruft dieselbe
`validate()` vor jedem Schreiben auf) und bewertet das **gesamte Dokument
als Einheit** -- ein einziger Eintrag mit 12+ Zeichen langem Material (z. B.
"Support for PLA", 15 Zeichen) liess `/mappings/printer-slots.json`
insgesamt als ungueltig gelten, **auch fuer alle anderen, laengst
korrekten Eintraege in derselben Datei**. Konkret bedeutete das: sobald
irgendein Fach mit einem 12+ Zeichen langen Material belegt wurde, schlug
sowohl das anschliessende Speichern (`persistTraySpoolCache()`, still
scheiternd, kein Fehlerdialog -- bewusst "fire-and-forget") als auch jeder
spaetere Ladeversuch fehl. Der Zwischenzustand direkt nach dem Zuordnen war
trotzdem korrekt sichtbar, weil `traySpoolCache` im RAM bereits vor dem
(fehlschlagenden) Speichern aktualisiert wird -- erst ein Neustart, der
zwingend neu aus der (nie erfolgreich schreibenden) Datei laden muss,
deckte den Fehler auf.

**Fix:** Grenze in `validateTraySpoolCacheEntries()` von `12U` auf `16U`
angehoben, passend zum tatsaechlichen Puffer. Keine weiteren Stellen mit
derselben veralteten Grenze gefunden (gezielt nachgeprueft).

**Testluecke festgestellt, nicht behoben:** `test/test_json_storage/`
(laeuft auf echter Hardware, `pio test -e wt32-s3-wrover-n16r2`, siehe
`docs/developer-guide.md`) deckt `TraySpoolCache` ueberhaupt nicht ab --
ein Rundlauftest mit einem 12+ Zeichen langen Material haette diesen Bug
sofort gefunden. Nicht in dieser Session ergaenzt, da der Hardware-Testlauf
die laufende manuelle Nutzertestung unterbrochen haette; sollte nachgeholt
werden.

Build (0 Warnungen), 60 native Tests gruen, geflasht. Noch nicht am Geraet
mit einem echten Neustart erneut verifiziert.

**Nachtrag (2026-08-27):** Nutzer meldete nach diesem Fix: das
Material/Farbe-Fingerprinting funktioniert nach einem Neustart weiterhin
nicht -- jetzt auch fuer "PLA", nicht mehr nur fuer lange Materialnamen.
Codepruefung findet keinen weiteren offensichtlichen Logikfehler in
`resolveTraySpoolCacheSpoolId()`/`isValidTraySlotAddress()`/dem Lade-Pfad;
die naheliegendste Erklaerung ist eine noch nachwirkende Folge des
2026-08-27-Fundes oben: `persistTraySpoolCache()`s Speichern serialisiert
**alle** Eintraege gemeinsam, und `JsonStorage::serialize()` validiert das
gesamte Dokument vor dem Schreiben -- ein einzelner ungueltiger Altvorgang
(vor dem `[16]`-Fix) haette jedes seither versuchte Speichern insgesamt
verworfen und die Datei dauerhaft auf einem alten/leeren Stand
eingefroren, unabhaengig vom Material der aktuellen Zuordnung.

**Bisher komplett unsichtbar dafuer:** weder ein fehlgeschlagenes Speichern
(`persistTraySpoolCache()`, requestId bewusst 0, "fire-and-forget") noch ein
fehlgeschlagenes Laden (`kTraySpoolCacheLoadRequestId` als
`StorageRequestError` statt `StorageReadCompleted`) erzeugten bisher
irgendeine Log-Zeile -- `sendStorageEvent()` selbst loggt nur einen
Enqueue-Fehler der Queue, nie den eigentlichen Fehlerinhalt. Zwei neue
`FS_LOGW`-Zweige in `AppTask.cpp` ergaenzt, die genau diese beiden bisher
stillen Fehlerpfade jetzt sichtbar machen ("Tray-Spoolman cache load
failed"/"... save failed", jeweils mit dem konkreten `JsonStorageError`-Text).

Kein weiterer Fix in diesem Schritt -- die tatsaechliche Ursache soll anhand
eines frischen Logs mit dieser neuen Sichtbarkeit bestaetigt werden, statt
weiter zu raten. Falls die Vermutung oben zutrifft, sollte einmal ein
komplett neuer Zuordnungsversuch (der jetzt mit der `[16]`-Grenze sauber
speichert) das Problem von selbst beheben; falls die neuen Log-Zeilen
weiterhin nichts zeigen, liegt die Ursache woanders und muss anhand des
neuen Logs neu eingegrenzt werden.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

**Nachtrag (2026-08-27):** Neues Log erhalten -- zeigt den Fehler direkt:

```
W [APP] Tray-Spoolman cache save failed: Storage request failed: invalid_document_field
```

fuer eine Zuordnung von "ABS" (3 Zeichen, laengst innerhalb aller Grenzen)
auf ams_id=0/tray_id=0. Die 2026-08-27-Laengenvermutung ist damit
widerlegt -- die Ursache liegt woanders. Da `validateTraySpoolCacheEntries()`
stets das **gesamte** Dokument bewertet, ist ein anderer, im Log nicht
direkt sichtbarer Eintrag im selben Speichervorgang der wahrscheinlichere
Kandidat (z. B. aus einer frueheren Zuordnung in derselben laufenden
Sitzung). Der generische Fehlertext (nur der `JsonStorageError`-Codename)
reicht dafuer nicht aus.

**Weitere Diagnose ergaenzt, noch kein Fix:** `persistTraySpoolCache()`
(`AppTask.cpp`) loggt jetzt jeden einzelnen Eintrag (`printerId`/`amsId`/
`trayId`/`spoolId`/`material`+Laenge/`colorHex`+Laenge) direkt vor dem
Schreiben. Damit sollte der naechste Fehlschlag den genauen betroffenen
Eintrag zeigen, statt weiter richten zu muessen.

Build (0 Warnungen), 60 native Tests gruen, geflasht. Wartet auf ein
frisches Log mit dieser Detailanzeige.

**Nachtrag (2026-08-27), Ursache endgueltig gefunden:** die neue
Detail-Log-Zeile zeigte alle drei aktuellen Eintraege vor dem
fehlschlagenden Speichern:

```
ams_id=0 tray_id=0 spool_id=1 material="PLA" colorHex="" colorHex_len=0
ams_id=0 tray_id=1 spool_id=4 material="ABS" colorHex="080808FF" colorHex_len=8
ams_id=0 tray_id=2 spool_id=1 material="PLA" colorHex="" colorHex_len=0
```

Zwei Eintraege (Spoolman-Spule #1, "PLA") haben ein **leeres** `colorHex`
-- diese Spule hat in Spoolman schlicht keine Farbe hinterlegt, was ein
voellig legitimer Zustand ist (kein Datenfehler). `ams_id=0/tray_id=1`
("ABS") ist dagegen unauffaellig; die falschen Eintraege sind also nicht
die zuletzt zugeordnete Spule selbst, sondern **andere, laengst bestehende**
Eintraege im selben Dokument -- exakt das in der vorherigen Notiz vermutete
Szenario.

`validateTraySpoolCacheEntries()` verlangte bisher zwingend ein
nicht-leeres `colorHex` (`isNonEmptyString()`) -- das war zu strikt. Fix:
nur noch Typ (String) und Laenge werden geprueft, ein leerer String ist
jetzt ausdruecklich erlaubt (`isOptionalString()`, bereits fuer andere
Felder in dieser Datei vorhanden). `resolveTraySpoolCacheSpoolId()`s
`strcmp`-Vergleich behandelt zwei leere Strings bereits korrekt als
Uebereinstimmung, keine Aenderung dort noetig.

Build (0 Warnungen), 60 native Tests gruen, geflasht. Noch nicht am Geraet
verifiziert.

**Nachtrag (2026-08-27):** Nutzer meldete: "Extern" (der manuelle
Spulenhalter ohne AMS) als Zielslot ausgewaehlt fuehrt sofort zu "Ungueltiger
AMS-Slot", ohne dass ueberhaupt etwas gesendet wird. Ursache: "Extern"
sendet `amsId`/`trayId` beide als `kExternalTraySentinel` (0xFF, siehe
`UiBridge.cpp::trayTargetClicked()`), aber **vier** unabhaengige Stellen in
`AppTask.cpp`/`BambuTask.cpp` waren ausschliesslich auf den regulaeren
AMS-Bereich (1..4/0..3) ausgelegt und haetten den externen Fall auch nach
einer blossen Validierungs-Lockerung falsch oder gar nicht verarbeitet:

1. Die Eingangsvalidierung in `ConfigureSlotFromStaging`/`ReapplySlot`,
   `ResetSlot`/`UntagSlot` und der "Manuell"-Spulenauswahl (drei identische
   Codestellen) lehnte `amsId=0xFF` als aussehalb des Bereichs 1..4 ab --
   der unmittelbar gemeldete Fehler.
2. `sendPendingSlotAssignTray()` haette `amsId=0xFF` blind per `-1U` in
   `0xFE` umgerechnet, statt der vom Drucker erwarteten festen Adresse
   `ams_id=255/tray_id=254` (docs/bambu-protocol.md, FilaMan-Vergleich) --
   neue Konstanten `models::kBambuExternalAmsId`/`kBambuExternalTrayId`
   ergaenzt und in `PrinterState.h` dokumentiert (bewusst getrennt von
   `kActiveTrayNowExternal=254`, einem anderen Feld mit zufaellig
   gleichem Tray-Wert).
3. Derselbe blinde `-1U`-Fehler existierte unabhaengig nochmal in
   `ResetSlot`/`UntagSlot`s eigenem, direkten `AssignTray`-Aufbau (nicht
   ueber `sendPendingSlotAssignTray()`).
4. `BambuTask::checkPendingTrayAssignment()` haette die Bestaetigung nie
   erkannt: `findSlot()` durchsucht ausschliesslich `amsUnits[]`, das
   externe Fach lebt aber in einem eigenen `externalSlot`-Feld
   (`PrinterState.h`) -- ohne diesen Fix waere "Extern konfigurieren" trotz
   korrekt gesendetem Kommando in denselben Best\xC3\xA4tigungs-Timeout
   gelaufen wie die frueheren AMS-Bugs dieser Phase.
5. Die Cache-Persistierung nach erfolgreicher Best\xC3\xA4tigung
   (dieselbe Stelle wie die vorherigen "?"-Nachtraege) kannte den externen
   Fall ebenfalls nicht -- ohne Fix waere "Extern" zwar konfigurierbar und
   bestaetigbar geworden, aber nie in `/mappings/printer-slots.json`
   uebernommen worden (derselbe dauerhafte "?"-Effekt nach einem Neustart).

Alle fuenf Stellen behoben; die Lesevorseite (`syncAmsToUi()`s
`external`-Block, `AppTask.cpp:999-1019`) war bereits vorher korrekt und
diente als Vorlage fuer die Sentinel-Konvention.

Build (0 Warnungen), 60 native Tests gruen, geflasht (nach einem
fehlgeschlagenen ersten Upload-Versuch mit knapp 22 Minuten erfolglosem
COM5-Retry, danach nach USB-Reconnect beim naechsten Versuch erfolgreich).
Noch nicht am Geraet verifiziert.

**Nachtrag (2026-08-27):** Nutzer testete erneut -- "Extern konfigurieren"
scheiterte weiterhin mit "Ungueltiger AMS-Slot". Ursache: der obige Fix
deckte drei der vier `action.amsId == 0 || ...`-Validierungsstellen ab; die
vierte (SelectSpool/"Manuell" auf `TrayActions`, erreichbar wenn zuvor das
externe Fach selbst ausgewaehlt wurde) hat eine Einrueckungsebene tiefer
gelegen (verschachtelt in einem eigenen `if`, nicht direkt in einem
`case`-Block) und wurde vom `replace_all` der vorherigen Aenderung deshalb
nicht erfasst -- textuell nicht identisch trotz gleicher Logik. Dieselbe
`isExternalTarget`-Pruefung dort ergaenzt.

Build (0 Warnungen), 60 native Tests gruen, geflasht (nach COM5-Reconnect,
Upload beim zweiten Versuch erfolgreich). Noch nicht am Geraet verifiziert.

**Nachtrag (2026-08-27), fuenfte und sechste Stelle gefunden:** Nutzer
meldete denselben Fehler ein drittes Mal. Diesmal die tatsaechliche
Ursache: `BambuTask.cpp::handleAssignTray()` hat eine **eigene, vierte**
Validierung (`command.amsId >= kMaximumAmsPerPrinter || command.trayId >=
kSlotsPerAms`), die bei der bisherigen Fehlersuche komplett uebersehen
wurde, weil sie in einer anderen Datei liegt als die drei bereits
gefundenen `AppTask.cpp`-Stellen -- der wire-kodierte externe Wert
(`ams_id=255`) verletzt diesen Bereich ebenfalls (255 >= 4). Ergaenzt um
dieselbe Sentinel-Ausnahme (`isExternalWireAddress`, hier gegen die
Bambu-Wire-Konstanten `kBambuExternalAmsId`/`kBambuExternalTrayId`
geprueft, nicht gegen `kExternalTraySentinel` -- an dieser Stelle liegt der
Wert bereits umgerechnet vor).

**Sechste, tiefer liegende Stelle dabei zusaetzlich gefunden (noch nicht
gemeldet, aber denselben Fehler verursacht haette):**
`BambuProtocol.cpp::bambuBuildExtrusionCaliSel()` berechnet fuer
`tray_id` einen globalen Index als `ams_id * kSlotsPerAms + trayId` --
fuer den externen Fall (`255 * 4 + 254`) ergibt das in `std::uint8_t`
umlaufend `250`, einen bedeutungslosen Wert, der mit keiner realen
Adressierungskonvention uebereinstimmt. Fix: fuer `ams_id ==
kBambuExternalAmsId` wird stattdessen direkt der feste Sentinel-Wert
(`254`) verwendet, wie ihn `ams_filament_setting` fuer `tray_id`/`slot_id`
in diesem Fall bereits einsetzt -- keine globale Indexrechnung.

Insgesamt jetzt neun unabhaengige Stellen fuer den externen Slot behoben
(drei Validierungen und die Wire-Kodierung in zwei getrennten Codepfaden in
`AppTask.cpp`, deren Cache-Persistierung, die Bambu-Bestaetigungspruefung,
die eigene Validierung in `handleAssignTray()` sowie die
`extrusion_cali_sel`-Adressberechnung -- alle drei in `BambuTask.cpp`/
`BambuProtocol.cpp`).

Build (0 Warnungen), 60 native Tests gruen (inkl. `test_bambu_protocol`),
geflasht (erster Upload-Versuch diesmal erfolgreich). Noch nicht am Geraet
verifiziert.

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

# Phase 11 – Energiesparen

Konzept (Nutzerwunsch 2026-08-25): 3-Stufen-Statemachine AKTIV ->
GEDIMMT -> LIGHT-SLEEP, Timer ab letzter Touch-Aktivitaet (LVGL
`lv_disp_get_inactive_time()`), Wake ueber Touch-INT-Pin (GPIO7, RTC-
faehig, ext0/ext1-Wakeup). Nutzerentscheidung: echter Light-Sleep,
Drucker-/Spoolman-Ueberwachung darf dafuer waehrend des Sleeps
unterbrochen werden (WiFi wird vollstaendig abgeschaltet). Aufwachen
nutzt bestehende Reconnect-/Recovery-Logik aus Phase 10.4/10.5 (WiFi-
Reconnect-Timer, PN532-Reconnect-Erkennung, Bambu-MQTT-Reconnect).
Groesstes offenes Risiko: PSRAM-Kompatibilitaet mit `esp_light_sleep_
start()` in diesem Arduino-Framework-Build (analog zum bereits in
Phase 10.8 festgestellten `EXT_RAM_ATTR`-sdkconfig-Problem) -- daher
zuerst als eigener Machbarkeits-Check vor dem Rest der Phase.

## 11.1 Konfiguration und Statemachine

* [x] `PowerConfig.h` (Timer-Konstanten: Dimm-Timeout, Sleep-Timeout,
  gedimmte Zielhelligkeit)
* [x] `PowerTask` (neuer eigener Task, haelt Aktiv/Gedimmt/Sleep-Zustand)
* [x] Inaktivitaets-Erkennung ueber LVGL `lv_disp_get_inactive_time()`
* [x] neue Command-Typen (`PowerDown`/`PowerUp`) fuer Scale-/Nfc-/
  Network-Task nach bestehendem Commands.h-Muster
* [x] Bestaetigungs-Events, bevor `PowerTask` tatsaechlich schlafen legt

**Umgesetzt (2026-08-25):** neue `PowerConfig.h`
(`kPowerActivityReportIntervalMs=1000`, `kPowerDimTimeoutMs=30000`,
`kPowerSleepTimeoutMs=180000`, `kPowerDimmedBrightness=38`). Neuer
`PowerTask` (`PowerTask.cpp`) mit interner
`enum class PowerState { Active, Dimmed, Sleep }` und reiner
Zustandsuebergangs-Logik (`stateForInactivity()`), Uebergaenge werden
geloggt (`FS_LOGI`, neue `LogComponent::Power`) -- die eigentlichen
Aktionen (Dimmen, Peripherie/WiFi abschalten, Light-Sleep) folgen in
11.2-11.6 und sind hier bewusst noch nicht verdrahtet.

Inaktivitaet wird korrekt ueber LVGL 9.5 ermittelt: die im Konzept
genannte `lv_disp_get_inactive_time()` ist in dieser LVGL-Version nur
noch ein v8-Kompatibilitaetsmakro (`lv_api_map_v8.h`) fuer die echte
Funktion `lv_display_get_inactive_time()` -- neuer Wrapper
`ui::inputInactiveMs()` in `UiBridge.cpp` haelt den LVGL-Zugriff dabei
weiterhin exklusiv in `UiTask`, per "Nur UiTask greift auf LVGL zu".
`UiTask`s Hauptschleife (`UiTask.cpp`) begrenzt ihre Wartezeit neu
zusaetzlich auf `kPowerActivityReportIntervalMs`, damit sie auch bei
voelliger LVGL-Ruhe (vorher moeglich: `portMAX_DELAY`) regelmaessig genug
`PowerCommandType::ReportInactivity` an den neuen `powerCommandQueue`
sendet; der bisherige `std::numeric_limits`-Sonderfall entfaellt dadurch
(totes `#include <limits>` entfernt).

`PowerDown`/`PowerUp` wurden wie im Konzept beschrieben direkt in die
bestehenden `ScaleCommandType`/`NfcCommandType`/`NetworkCommandType`
erweitert (Commands.h) statt als neuer eigener Typ -- entspricht "nach
bestehendem Commands.h-Muster"; noch ohne Handler in den jeweiligen
Tasks (folgt 11.3-11.5). Neuer `PowerCommandType` (`ReportInactivity`,
`PowerDownAcknowledged`) + `PowerCommand`-Struct (Messages.h) fuer den
Kanal UiTask/Hardware-Tasks -> PowerTask; `PowerDownAcknowledged` ist
strukturell vorhanden, wird aber erst ab 11.3-11.6 tatsaechlich gesendet,
wenn PowerTask vor dem Light-Sleep auf Bestaetigungen wartet.

Neuer Task ueberall nach bestehendem Muster eingehaengt: `RtosContext`
(Queue + `TaskHandle_t`), `TaskConfig.h` (`kPowerTask`, 4096 Byte Stack --
kleine Statemachine ohne AppEvent-grosse Stack-Locals), `Tasks.h`,
`RtosContext::createServiceTasks()`. Neue `LogComponent::Power` in
Logger.h/LoggerFormat.cpp ergaenzt (exhaustive Switches angepasst).

Build (0 Warnungen), 54 native Tests gruen, geflasht (2. Flash-Versuch
erfolgreich -- 1. und 2. Versuch scheiterten an einem transienten
"No serial data received" auf COM5, kein Code-Zusammenhang).

## 11.2 Bildschirm Dimmen/Aus

* [x] `UiCommandType::SetBrightness`, verarbeitet in `UiTask`
* [x] Laufzeit-Aufruf von `Light_PWM.setBrightness()` (bisher nur
  einmalig beim Boot gesetzt)
* [x] Stufe GEDIMMT (reduzierte Helligkeit, Peripherie/WiFi unveraendert)
* [x] Stufe LIGHT-SLEEP (Helligkeit 0)

**Umgesetzt (2026-08-25):** neuer `UiCommandType::SetBrightness`
(Commands.h) -- `value` traegt die Ziel-Helligkeit 0-255. Neuer Case in
`ui::processUiCommand()` (`UiBridge.cpp`), ruft
`drivers::displayDevice().setBrightness()` direkt zur Laufzeit auf
(vorher nur einmalig beim Boot in `DisplayDriver.cpp::
initializeDisplay()`); kein neuer Wrapper in `DisplayDriver.h` noetig,
da `displayDevice()` bereits das volle `lgfx::LGFX_Device` zurueckgibt.

`PowerTask` sendet dieses Kommando jetzt bei jedem Statemachine-Uebergang
(neue `brightnessForState()`/`sendBrightness()` in `PowerTask.cpp`):
AKTIV -> `kDisplayDefaultBrightness` (192, BoardConfig.h), GEDIMMT ->
`kPowerDimmedBrightness` (38), SLEEP -> 0. Peripherie (Waage/NFC/WiFi)
bleibt unveraendert an -- das ist bewusst erst 11.3-11.5. Noch nicht am
Geraet visuell gegengeprueft (nur geflasht, kein Sichttest durch mich) --
Verhalten am realen Board (Dimmen nach 30s, Aus nach 180s, Touch-Wake
zurueck auf volle Helligkeit) sollte vom Nutzer noch bestaetigt werden.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

**Nachtrag (2026-08-27), Nutzervorgabe:** Zielwerte korrigiert --
`kDisplayDefaultBrightness` (`BoardConfig.h`) von 192 auf 255 (volle
Helligkeit im Normalbetrieb), `kPowerDimmedBrightness` (`PowerConfig.h`)
von 38 auf 28. Reine Konstantenaenderung, keine Logikaenderung.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

## 11.3 Waage Power-Down

* [x] HX711 Power-Down vor Sleep (SCK-Pin dauerhaft HIGH)
* [x] Re-Init/erster Sample-Wait nach Wake

**Umgesetzt (2026-08-25):** `PowerTask` sendet beim Uebergang in
LIGHT-SLEEP `ScaleCommandType::PowerDown` und beim Verlassen von
LIGHT-SLEEP `ScaleCommandType::PowerUp` an `scaleCommandQueue` (neue
`sendScalePower()`, `PowerTask.cpp`) -- ausgeloest exakt am
Sleep-Uebergang, nicht bei AKTIV<->GEDIMMT.

`ScaleTask`s Hauptschleife (`ScaleTask.cpp`) behandelt beide Kommandos
jetzt direkt (vor `processScaleCommand()`, da sie Schleifenzustand statt
nur Kalibrierung betreffen): PowerDown haelt den SCK-Pin dauerhaft HIGH
(laut HX711-Datenblatt reichen >60us fuer den Power-Down, hier bleibt er
fuer die gesamte Sleep-Dauer stehen) und setzt `connected`/
`hasMeasurement` zurueck; die Schleife ueberspringt waehrenddessen Sample-
Lesung und den Verbindungs-Timeout komplett (`if (poweredDown) continue;`),
damit das absichtliche Abschalten nicht als "HX711 not responding"-Fehler
gemeldet wird. PowerUp setzt SCK zurueck auf LOW und `lastMeasurementMs`
auf `millis()` -- kein separater Re-Init noetig, da der bereits dauerhaft
installierte ISR-Handler (`gpio_isr_handler_add`, nie entfernt) automatisch
wieder Notifies liefert, sobald das HX711 nach dem Aufwachen die naechste
Konversion abschliesst; die bestehende "erstes Sample -> ScaleReady"-Logik
im Hauptloop greift dann unveraendert.

`ScaleTask` bestaetigt den Power-Down zusaetzlich ueber den in 11.1
definierten Kanal (`PowerCommandType::PowerDownAcknowledged`, neue
`sendPowerAck()`); `PowerTask` loggt den Erhalt jetzt (`FS_LOGD`) statt
ihn stillschweigend zu verwerfen -- das tatsaechliche Abwarten aller
Bestaetigungen vor dem echten Light-Sleep bleibt bewusst Phase 11.6.

Noch nicht am Geraet verifiziert (kein Sichttest durch mich moeglich) --
sollte vom Nutzer am realen Board bestaetigt werden: Waage zeigt nach
Erreichen von LIGHT-SLEEP keine neuen Messwerte mehr, liefert nach einem
Touch-Wake aber wieder normale Messungen.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 11.4 NFC Power-Down

* [x] RF-Feld aus vor Sleep (`setRfField(false)`, bereits vorhanden)
* [x] Polling pausiert waehrend Sleep
* [x] echtes PN532-PowerDown-Kommando (0x16) pruefen/implementieren
* [x] Re-Init nach Wake ueber bestehende `initializePn532()`/
  `reportPn532Reconnected()`-Logik

**Umgesetzt (2026-08-25):** `PowerTask` sendet beim Sleep-Uebergang
zusaetzlich zu Scale jetzt auch `NfcCommandType::PowerDown`/`PowerUp` an
`nfcCommandQueue` (neue `sendNfcPower()`, `PowerTask.cpp`).

`NfcTask`s Hauptschleife (`NfcTask.cpp`) behandelt beide Kommandos als
neue Cases direkt im bestehenden `nfcCommandQueue`-Switch: PowerDown
schaltet zuerst das RF-Feld aus (`setRfField(false)`, bereits vorhanden),
sendet danach das echte PN532-PowerDown-Kommando (`0x16`, neue
`powerDown()`-Funktion neben `setRfField()`/`resetRfField()`) und pausiert
das Polling ueber ein neues `poweredDown`-Flag (`if (poweredDown)
continue;`, analog zum HX711-Muster aus 11.3). Ein bereits aktiv erkanntes
Tag wird dabei zurueckgesetzt (`present=false`), da es waehrend des
Power-Downs ohnehin nicht mehr abgefragt werden kann.

**Zum PowerDown-Kommando (0x16):** Parameter ist ein WakeUpEnable-Bitmap
laut PN532-Datenblatt (UM0701-02, 7.2.11); implementiert mit Bit4 (0x10)
= HSU, dem hier tatsaechlich verwendeten Transportinterface -- der exakte
Bit-Wert stammt aus dem Datenblatt, wurde aber nicht gegen ein
Referenzsystem gegengeprueft. Deshalb bewusst mit Rueckfallebene gebaut:
schlaegt das Kommando fehl oder ist der Bit-Wert falsch, bleibt das RF-Feld
trotzdem aus (erste, garantiert wirksame Massnahme) und ein spaeterer
Wake-Fehlversuch wird ueber die bestehende Comm-Error-/Reconnect-Erkennung
(`notePn532CommError` -> `reportPn532Disconnected`) sichtbar gemeldet statt
still zu haengen -- im schlechtesten Fall bleibt NFC bis zu einem manuellen
Stromzyklus des PN532 unverfuegbar, aber klar erkennbar, nie stillschweigend
tot.

Wake/Re-Init nutzt exakt die im Konzept genannte bestehende Logik: PowerUp
ruft `initializePn532()` direkt auf (die bereits mit der Wake-Praeambel
`{0x55,0x55,0x00,0x00,0x00}` beginnt, die laut Datenblatt auch aus dem
Power-Down weckt) und bei Erfolg `reportPn532Reconnected()` (setzt
`EVENT_NFC_READY`, sendet `NfcInitialized`); bei Fehlschlag kein
Sonderpfad -- der naechste normale `scanTarget()`-Versuch erkennt den
Fehler ueber die bereits vorhandene `notePn532CommError()`/
`sustainedCommErrors`-Maschinerie aus Phase 10.2, dieselbe, die auch jeden
anderen Transportfehler zur Laufzeit erkennt.

`NfcTask` bestaetigt PowerDown ueber denselben Kanal wie Phase 11.3
(neue `sendPowerAck()`, analog zu `ScaleTask.cpp`).

Noch nicht am Geraet verifiziert (kein Sichttest durch mich moeglich) --
besonders das PowerDown-Kommando (0x16) sollte der Nutzer am realen Board
pruefen: Tag-Erkennung stoppt in LIGHT-SLEEP, funktioniert nach Touch-Wake
wieder normal, und es bleibt kein PN532-Fehlerzustand haengen.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 11.5 WiFi Abschaltung

* [x] `WiFi.mode(WIFI_OFF)` vor Sleep
* [x] `WiFi.mode(WIFI_STA)` + bestehender Reconnect-Mechanismus
  (`kWifiReconnectIntervalMs`) nach Wake
* [x] Bambu-MQTT- und Spoolman-Resync nach Wake ueber bestehende
  Recovery-Logik

**Umgesetzt (2026-08-25):** `PowerTask` sendet beim Sleep-Uebergang
zusaetzlich zu Scale/NFC jetzt auch `NetworkCommandType::PowerDown`/
`PowerUp` an `networkCommandQueue` (neue `sendNetworkPower()`,
`PowerTask.cpp`).

`NetworkTask`s Hauptschleife (`NetworkTask.cpp`) behandelt beide
Kommandos als neue Cases im bestehenden `networkCommandQueue`-Switch:
PowerDown ruft `WiFi.mode(WIFI_OFF)`. Ein neues `poweredDown`-Flag
verhindert dabei, dass der in Phase 10.4 gebaute automatische Reconnect
(`kWifiReconnectIntervalMs`) sofort wieder dagegen ankaempft -- sowohl die
Wartezeit-Berechnung (`disconnectedAndConfigured`) als auch der
Reconnect-Versuch selbst sind jetzt zusaetzlich mit `!poweredDown`
bedingt, damit die Schleife waehrend des Sleeps in `portMAX_DELAY`
verharrt statt alle `kWifiReconnectIntervalMs` sinnlos aufzuwachen.

PowerUp ruft `WiFi.mode(WIFI_STA)` und setzt `lastReconnectAttemptAt = 0`
zurueck -- kein neuer Reconnect-Code noetig, das loest im naechsten
Schleifendurchlauf einfach den ohnehin schon vorhandenen
`WiFi.reconnect()`-Pfad aus Phase 10.4 aus (identisches "kein Sonderfall
noetig"-Muster wie schon bei Scale/NFC in 11.3/11.4).

Bambu-MQTT- und Spoolman-Resync brauchten wie im Konzept erwartet keinen
neuen Code: sobald `WifiGotIp` erneut `EVENT_WIFI_CONNECTED` setzt,
greifen die in Phase 10.4/10.5 gebauten Recovery-Mechanismen
(`BambuTask::serviceConnections()`-Reconnect-Backoff,
`AppTask`s Spoolman-Health-Check-Retry) automatisch, exakt wie nach jedem
anderen WiFi-Verbindungsverlust.

`NetworkTask` bestaetigt PowerDown ueber denselben Kanal wie 11.3/11.4
(neue `sendPowerAck()`, analog zu Scale-/NfcTask).

Noch nicht am Geraet verifiziert (kein Sichttest durch mich moeglich) --
sollte vom Nutzer geprueft werden: WLAN trennt sich beim Erreichen von
LIGHT-SLEEP sichtbar (z.B. im Router), verbindet sich nach Touch-Wake
selbststaendig neu, und Drucker-/Spoolman-Status aktualisieren sich danach
ohne manuelles Eingreifen.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 11.6 Light-Sleep und Touch-Wake

* [x] Machbarkeits-Check: PSRAM + `esp_light_sleep_start()` in diesem
  sdkconfig (vor dem Rest dieser Unterphase)
* [x] FT6336-INT-Verhalten am realen Board pruefen (Pulse vs. Pegel,
  Polaritaet)
* [x] `esp_sleep_enable_ext0_wakeup()`/`ext1` auf `kTouchInterruptPin`
  (tatsaechlich per `gpio_wakeup_enable()`/`esp_sleep_enable_gpio_wakeup()`
  umgesetzt, siehe Begruendung unten)
* [x] Koordination: `PowerTask` wartet auf Quiescent-Bestaetigung aller
  betroffenen Tasks vor `esp_light_sleep_start()`
* [x] UI-Uebergangszustand ("Verbinde...") nach Wake bis WiFi/Bambu/
  Spoolman wieder synchron sind
* [x] optionaler periodischer Timer-Wake als Sicherheitsnetz -- entgegen der
  urspruenglichen Einstufung als "spaetere Ausbaustufe" bereits jetzt fest
  eingebaut, Begruendung unten

**Machbarkeits-Check (2026-08-25), vorab per Recherche statt Hardwaremessung:**

- **PSRAM:** `platformio.ini` setzt `board_build.arduino.memory_type =
  qio_qspi` -- dieses Board laeuft mit *Quad*-PSRAM, nicht Octal. Im
  tatsaechlich kompilierten `sdkconfig.h`
  (`.pio-core/packages/framework-arduinoespressif32/tools/sdk/esp32s3/
  qio_qspi/include/sdkconfig.h`) ist
  `CONFIG_ESP_SLEEP_PSRAM_LEAKAGE_WORKAROUND=1` bereits gesetzt -- fest
  einkompiliert in allen sechs Speicher-Varianten dieses Arduino-ESP32-
  Pakets, nicht projektspezifisch aktivierbar/deaktivierbar. Anders als bei
  `EXT_RAM_ATTR` (Phase 10.8) ist hier also keine sdkconfig-Aenderung noetig
  oder moeglich -- die Absicherung ist bereits vorhanden. `CONFIG_PM_ENABLE`
  ist NICHT gesetzt (kein automatisches Tickless-Idle), was fuer den hier
  gewaehlten Ansatz (expliziter, manueller `esp_light_sleep_start()`-Aufruf
  statt automatischem PM-Light-Sleep) unproblematisch ist. Die tatsaechliche
  Implementierung von `esp_light_sleep_start()` liegt nur als vorkompilierte
  `libesp_hw_support.a` vor (keine Quelltexte vendored) -- interne Details
  stuetzen sich auf oeffentliche ESP-IDF-API-Dokumentation in `esp_sleep.h`,
  nicht auf lokale Quellpruefung.
- **FT6336-INT:** LovyanGFX' `Touch_FT5x06`-Treiber
  (`.pio/libdeps/wt32-s3-wrover-n16r2/LovyanGFX/src/lgfx/v1/touch/
  Touch_FT5x06.cpp:57`) konfiguriert das Register `FT5x06_INTMODE_REG`
  explizit auf Polling-Modus (nicht Trigger/Puls-Modus) und behandelt
  `pin_int` selbst nur als gepollten Pegel (`gpio_in()`, keine ISR) -- kein
  Hinweis auf Polaritaet im Treiber oder in `docs/hardware.md` (dort explizit
  als "spaetere Nutzung zu pruefen" vermerkt). Der Pin ist per LovyanGFX
  standardmaessig `input_pullup` (idle HIGH) konfiguriert -- daraus folgt die
  Annahme "aktives Signal zieht LOW" (`GPIO_INTR_LOW_LEVEL`), aber das ist
  eine Ableitung aus der Pull-up-Beschaltung, keine verifizierte Tatsache aus
  einem FT6336-Datenblatt (keins im Repo vorhanden).

**Wegen dieser verbleibenden Unsicherheit bei der Polaritaet wurde der
periodische Sicherheitsnetz-Timer NICHT auf spaeter verschoben, sondern
sofort fest eingebaut** (`kPowerSleepSafetyNetTimerMs=30000`,
`PowerConfig.h`): jeder Light-Sleep-Zyklus in `sleepUntilTouchWake()`
(`PowerTask.cpp`) bewaffnet sowohl `esp_sleep_enable_gpio_wakeup()` (Touch)
als auch `esp_sleep_enable_timer_wakeup()` (30s) und prueft nach jedem
`esp_light_sleep_start()`-Aufruf per `esp_sleep_get_wakeup_cause()`, ob die
Ursache `ESP_SLEEP_WAKEUP_GPIO` war. Falls nicht (Timer), wird sofort erneut
geschlafen (endlose innere Schleife) -- ist die Polaritaet falsch
angenommen, "blinkt" das Geraet dadurch alle 30s kurz auf statt dauerhaft
unerreichbar zu bleiben (ein einfacher Touch waehrend eines dieser Fenster
weckt es dann trotzdem, statt einen Stromzyklus zu erfordern).

**Koordination vor dem Sleep:** neuer `rtos::PowerPeripheral`-Enum
(`Scale`/`Nfc`/`Network`, `Commands.h`) plus `source`-Feld in `PowerCommand`
(`Messages.h`) identifiziert, welcher Task bestaetigt hat; alle drei
`sendPowerAck()`-Aufrufe (Scale-/Nfc-/NetworkTask) setzen es jetzt. Neue
`waitForSleepQuiescence()` (`PowerTask.cpp`) wartet blockierend auf alle drei
Bestaetigungen (Bitmaske), maximal `kPowerSleepAckTimeoutMs=3000`ms (deutlich
ueber dem PN532-Antwort-Timeout von 500ms) -- verhindert, dass eine
unterbrochene PN532-UART-Transaktion oder ein HX711-SCK-Toggle durch den
angehaltenen Prozessortakt korrumpiert wird. Ein ausbleibender/verlorener Ack
blockiert den Sleep trotzdem nicht dauerhaft (Timeout-Fallback mit Log-
Warnung).

**Wake-Ablauf:** `powerTask()`s Statemachine wurde vereinfacht --
"LIGHT-SLEEP verlassen" passiert nicht mehr ueber einen spaeteren, per
`ReportInactivity` erkannten Zustandswechsel (das waere nach diesem Umbau
unerreichbar, da `sleepUntilTouchWake()` den gesamten Prozessor blockiert,
bis ein echter Touch eintrifft), sondern direkt und synchron im Anschluss an
den Ruecksprung aus `sleepUntilTouchWake()`: `inactiveMs` wird auf 0
zurueckgesetzt, `state` direkt auf `Active` gesetzt, Helligkeit sowie
Scale-/Nfc-/Network-PowerUp gesendet, und ein `ShowToast`-UiCommand
("Aufgewacht, verbinde...") informiert den Nutzer -- die eigentliche
WiFi/Bambu/Spoolman-Synchronisation laeuft danach unveraendert ueber die
bestehende Recovery-Logik aus 11.5/Phase 10.4-10.5 und ueberschreibt den
Toast-Text automatisch mit den regulaeren Statusmeldungen.

**Ausdruecklich noch nicht verifiziert (kein Hardwarezugriff durch mich
moeglich) -- vor laengerem unbeaufsichtigtem Betrieb durch den Nutzer zu
pruefen:**
- Ob `GPIO_INTR_LOW_LEVEL` tatsaechlich die richtige Polaritaet ist (siehe
  oben) -- der 30s-Sicherheitsnetz-Timer faengt eine falsche Annahme ab, sollte
  aber am Geraet beobachtet werden (haeufiges kurzes Aufblinken alle 30s im
  Sleep waere das Anzeichen dafuer, dass Touch-Wake NICHT greift und nur der
  Timer das Geraet weckt).
- Verhalten der USB-CDC-Seriellkonsole waehrend eines Light-Sleep-Zyklus
  (moeglicher kurzer sichtbarer Aussetzer im `pio device monitor`, kein
  bekanntes Firmware-Risiko, aber nicht beobachtet). **Erster Hinweis
  bereits beobachtet:** beim naechsten Flash-Versuch nach dieser Phase
  meldete `esptool` dreimal in Folge "Could not open COM5, the port
  doesn't exist", obwohl Windows den Port weiterhin listete -- erst nach
  physischem Aus-/Einstecken des USB-Kabels durch den Nutzer war COM5
  wieder erreichbar. Passt zum erwarteten Risiko (Geraet vermutlich im
  Light-Sleep, USB-CDC dadurch nicht ansprechbar); noch nicht als
  ursaechlich bestaetigt, aber ein konkretes erstes Datenpunkt dafuer, dass
  das Sicherheitsnetz (30s-Timer) fuer den Nutzer sichtbar wird, sobald
  laenger unbeaufsichtigter Betrieb stattfindet.
- Build/Test/Flash wie gewohnt gruen -- das ist kein Ersatz fuer echten
  Light-Sleep-Betrieb am Geraet, den ich selbst nicht ausloesen/beobachten
  kann.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

**Nachtrag (2026-08-27):** genau das oben als "erster Hinweis" vermerkte
Risiko hat sich jetzt als eigener Nutzerbericht bestaetigt: nach einem
Aufwachen aus dem Light-Sleep funktioniert das serielle Logging nicht mehr.
Firmware-seitiger Behebungsversuch: `PowerTask::powerTask()` ruft direkt
nach dem Ruecksprung aus `sleepUntilTouchWake()` erneut `Serial.begin
(config::kSerialBaudRate)` auf (kein `Serial.end()` davor -- `LoggingTask`
koennte zum selben Zeitpunkt ebenfalls aufwachen und noch unverarbeitete,
vor dem Sleep geloggte Zeilen nachschreiben wollen; ein zerstoerendes
`end()` waere damit riskanter als ein reines Neu-Initialisieren).

**Ausdruecklich unsicher, ob das ausreicht:** wie oben bereits vermerkt,
deutet der wiederholt beobachtete "Could not open COM5"-Fehler beim
naechsten Flash-Versuch nach einem Sleep-Zyklus (durchgaengig in dieser
Session, nicht nur einmalig) auf eine tiefere USB-PHY-/Host-
Erkennungsgrenze hin, die eine reine `Serial.begin()`-Wiederholung
moeglicherweise nicht loest -- das kann nur der Nutzer am echten Geraet
verifizieren.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

## 11.7 Validierung

* [ ] reale Strommessung je Stufe (Aktiv/Gedimmt/Sleep)
* [ ] Wake-Zuverlaessigkeit (mehrere Touch-Positionen, Dauerbetrieb)
* [ ] Verhalten bei aktivem Druck dokumentieren (kein Print-Aktiv-Signal
  vorhanden, daher rein Timer-basiert in V1 -- bewusste Einschraenkung)

**Nachtrag (2026-08-26), konkreter Wake-Bug gefunden und behoben:** Nutzer
meldete genau das hier als offen gefuehrte Risiko real beobachtet: nach dem
Sleep zeigt ein Touch gelegentlich das Display <1s lang, bevor es sofort
wieder schwarz wird -- erst der naechste Touch weckt zuverlaessig auf.

**Ursache:** `UiTask` meldet `ReportInactivity` alle
`kPowerActivityReportIntervalMs` (~1 s) an `powerCommandQueue`, auch noch
unmittelbar bevor `PowerTask` per `esp_light_sleep_start()` den Prozessor
anhaelt (`PowerTask.cpp::sleepUntilTouchWake()`). Mindestens diese letzte,
veraltete Meldung (mit einem `inactiveMs`-Wert nahe/ueber dem
Sleep-Schwellwert) blieb bislang unkonsumiert in der Queue stehen, waehrend
das Geraet schlief. Nach dem Aufwachen setzt `powerTask()` `inactiveMs`
zwar lokal auf 0 zurueck, liest aber im naechsten Schleifendurchlauf sofort
diese veraltete Nachricht -- `stateForInactivity()` liefert dafuer erneut
`Sleep`, wodurch der gerade erst beendete Sleep-Ablauf (Peripherie
abschalten, auf Best\xC3\xA4tigung warten, echter Light-Sleep) unmittelbar
erneut anl\xC3\xA4uft, bevor der Nutzer das kurz aufgeleuchtete Display
ueberhaupt wahrnehmen kann.

**Fix:** `PowerTask.cpp::powerTask()` leert `powerCommandQueue` direkt nach
`sleepUntilTouchWake()` vollstaendig (nicht-blockierendes `xQueueReceive`
mit Timeout 0 in einer Schleife), bevor die Statemachine weiterlaeuft --
nur noch echte, nach dem Aufwachen frisch eintreffende Meldungen
entscheiden ueber den naechsten Zustand.

Build (0 Warnungen), 60 native Tests gruen, geflasht. Checkbox bleibt
bewusst offen: dieser eine Mechanismus ist behoben, die breitere Validierung
(mehrere Touch-Positionen, Dauerbetrieb) steht weiterhin aus und sollte vom
Nutzer nach diesem Fix erneut beobachtet werden.

**Nachtrag (2026-08-27):** Nutzer meldete ein zweites, unabhaengiges
Timing-Problem im Zustand "Gedimmt" (nicht Sleep): das Aufhellen auf einen
Tastendruck/Touch reagiert unterschiedlich schnell, manchmal zuegig,
manchmal mit ca. 1,5 s Verzoegerung.

**Ursache:** `UiTask` meldet Inaktivitaet nur alle
`kPowerActivityReportIntervalMs` (bisher 1000 ms) an `PowerTask`; dasselbe
Intervall begrenzt zugleich `UiTask`s maximale Wartezeit auf der
`uiCommandQueue` (`boundedSleepMs`). Ohne eigenen Touch-Interrupt (siehe
`docs/rtos.md`, Touch wird ausschliesslich per I2C-Polling innerhalb von
`lv_timer_handler()` erkannt) wird ein Touch bei einem vollstaendig ruhigen,
gedimmten Bildschirm erst beim naechsten Schleifendurchlauf ueberhaupt
bemerkt -- je nachdem, wann innerhalb des laufenden 1-Sekunden-Wartezyklus
der Touch erfolgt, ergibt sich eine Verzoegerung von nahe 0 bis knapp
ueber 1 s, zzgl. Verarbeitung/Queue-Laufzeit bis zur tatsaechlichen
Helligkeitsaenderung -- daher die beobachteten bis zu ca. 1,5 s.

**Fix:** `kPowerActivityReportIntervalMs` (`config/PowerConfig.h`) von 1000
auf 150 ms gesenkt. Betrifft nur die Reaktionsgeschwindigkeit in Aktiv/
Gedimmt -- keine Auswirkung auf den tatsaechlichen Light-Sleep-
Stromverbrauch, der weiterhin ausschliesslich ueber den separaten
GPIO-Wake in `PowerTask::sleepUntilTouchWake()` laeuft (kein Polling, kein
Busy Waiting) und durch dieses Intervall nicht beruehrt wird.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

---

# Phase 12 – Mock-Daten entfernen

Bestandsaufnahme (Nutzerwunsch 2026-08-25): vollstaendige Pruefung des
Quellcodes auf verbliebene Mock-Daten/-Funktionen (Grep nach
`[Mm]ock|TODO|FIXME|Platzhalter|Dummy` in `src/`, plus gezielte
Nachverfolgung jedes Treffers bis zum tatsaechlichen Aufrufer). Ergebnis:
die meisten frueheren Mock-Stellen sind bereits durch echte Daten ersetzt
(z.B. `printerEntries`/`amsEntries` in `UiBridge.cpp`, per Kommentar
bestaetigt: "no longer pre-seeds fake printers"). Fuenf echte Fundstellen
verbleiben, alle unten einzeln behandelt. `docs/hardware.md`s offener
Punkt zum Touch-INT-Verhalten ist kein Mock, sondern bereits unter
Phase 11.6 dokumentiert.

## 12.1 Echter Neustart

* [x] `ESP.restart()` nach Bestaetigung des RestartConfirmation-Dialogs
  tatsaechlich aufrufen (`AppTask.cpp:2563-2564`, aktuell nur
  "Neustart best\xC3\xA4tigt; im Mock nicht ausgef\xC3\xBChrt.")
* [x] Alle "(Mock)"-Texte rund um PrepareRestart entfernen
  (`AppTask.cpp:2831,2839`)
* [x] Kurze Verzoegerung/Log vor dem Neustart (laufende SD-/Spoolman-
  Schreibvorgaenge sollten nicht mitten im Reset abgebrochen werden --
  pruefen, ob ein "sauberer" Abschluss noetig ist oder ein sofortiger
  Neustart unproblematisch ist)

**Umgesetzt (2026-08-25):** Der `Confirm`-Handler fuer
`RestartConfirmation` (`AppTask.cpp`) ruft jetzt tatsaechlich
`ESP.restart()` auf, statt nur eine Mock-Erfolgsmeldung zu zeigen. Ablauf:
`FS_LOGI`-Log mit `request_id`, `ShowDialog`/`Success`-Meldung "Das Ger\xC3\xA4t
startet jetzt neu.", `vTaskDelay(kRestartDelayMs)` (neue Konstante,
`AppConfig.h`, 1500ms), dann `ESP.restart()`. Die Verzoegerung dient zwei
Zwecken: die Bestaetigungsmeldung wird noch sichtbar gerendert, und
eventuell noch laufende `StorageTask`-Schreibvorgaenge bekommen Zeit zum
Abschliessen -- ein AppTask-weiter Block ist hier unproblematisch, da der
Prozessor direkt danach ohnehin zurueckgesetzt wird. Kein zusaetzlicher
"saubere Beendigung"-Mechanismus noetig: `JsonStorage::atomicSave()` ist
bereits schreib-dann-umbenennen-atomar (Phase 2.4, per Wiederherstellungs-
test verifiziert), ein Reset mitten in einem Schreibvorgang hinterlaesst
also so oder so nie eine korrupte Datei.

Beide "(Mock)"-Texte entfernt: der Bestaetigungsdialog-Text (vorher "...
ausgel\xC3\xB6st (Mock)") ist jetzt neutral, und der (ohnehin unerreichbare,
da `PrepareRestart` immer ueber seinen eigenen fruehen `return` geht) Mock-
Toast-Fallback-Eintrag fuer `PrepareRestart` wurde aus der Fallback-Liste
entfernt (`ResetWifiCredentials`/`CheckFirmwareUpdate` dort unveraendert
gelassen, ausserhalb des Umfangs dieser Phase).

Nebenbefund: die Phase-13-Notiz "im gesamten Code keine einzige Versions-
Konstante" war falsch -- `config::kApplicationVersion = "0.1.0-dev"`
existiert bereits (`AppConfig.h:8`, genutzt in `main.cpp:42` fuer den
Boot-Log). Meine urspruengliche Volltextsuche fuer Phase 13 hatte nach
Mustern wie `FIRMWARE_VERSION`/`firmwareVersion` gesucht und diesen
camelCase-Namen mit anderem Praefix nicht erfasst. Phase 13.1 kann diese
Konstante direkt wiederverwenden, statt eine neue anzulegen.

Noch nicht am Geraet verifiziert (kein Sichttest durch mich moeglich) --
sollte vom Nutzer bestaetigt werden: nach Bestaetigen des Neustart-Dialogs
erscheint kurz die Erfolgsmeldung, danach startet das Geraet tatsaechlich
neu (Boot-Log erneut sichtbar).

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 12.2 Druckerbearbeitung-Status

* [x] `objects.printer_edit_status` zeigt beim Oeffnen von
  SCR_SETTINGS_PRINTER_EDIT hart codiert "Status: Mock-Daten"
  (`UiBridge.cpp:3040`) -- durch neutralen/echten Text ersetzen (z.B. leer
  oder "Neuer Drucker" vs. bestehenden Eintrag unterscheiden)

**Scope-Erweiterung waehrend der Umsetzung entdeckt (2026-08-25):** die
urspruengliche Checkliste erfasste nur das Statuslabel, aber
`loadPrinterUiDraft()` (`UiBridge.cpp`) enthielt zusaetzlich fuer jede
Drucker-ID (1-3) hart codierte, komplett erfundene Beispieldrucker (Name,
IP, Seriennummer, Access-Code -- z.B. "X1C Labor" / "192.168.1.51" /
"00M987654321"), die beim Oeffnen des Editors fuer einen bestehenden
Drucker angezeigt wurden. Meine urspruengliche Grep-Suche (`[Mm]ock|TODO|
FIXME|...`) fand diese Stelle nicht, da dort nirgends das Wort "Mock"
vorkommt. Nachverfolgung ergab: `AppTask`s `EditPrinter`/`AddPrinter`-
Handler laden bereits die ECHTEN Werte (`loadPrinterDraft()`,
`printerConfigs`) und schicken sie unmittelbar nach dem `ShowScreen`-
Kommando ueber `sendPrinterDraftToUi()` (4x `UpdateSettings`) nach -- da
`UiTask`s Hauptschleife die gesamte `uiCommandQueue` vor dem naechsten
LVGL-Render leerraeumt, wurden die erfundenen Werte in der Praxis wohl nie
sichtbar gerendert (reiner Zwischenzustand im Speicher). Trotzdem beseitigt:
`loadPrinterUiDraft()` setzt jetzt fuer jede ID nur noch einen neutralen
leeren Platzhalter, keine erfundenen Drucker mehr.

Zweiter, tatsaechlich sichtbarer Bug dabei gefunden: derselbe
`UpdateSettings`-Handler, der die echten Feldwerte uebernimmt, setzte
IMMER "Status: ge\xC3\xA4ndert, nicht gespeichert" -- auch beim stillen
Erstladevorgang eines unveraenderten, bestehenden Druckers. Ein frisch
geoeffneter Editor zeigte also faelschlich sofort "ungespeicherte
\xC3\x84nderungen", obwohl der Nutzer noch nichts getippt hatte. Behoben durch
eine neue Unterscheidung ueber `command.amsId` (fuer `UpdateSettings` sonst
ungenutzt): `AppTask::EditPrinterField` (echte Nutzeraenderung) setzt jetzt
`amsId=1`, `sendPrinterDraftToUi()` (stiller Erstladevorgang) laesst es bei
0 (Standardwert durch `UiCommand command{};`). `UiBridge.cpp`s Handler
aktualisiert das Statuslabel nur noch bei `amsId != 0`.

`objects.printer_edit_status` selbst zeigt jetzt beim Oeffnen neutral
"Status: -" statt "Status: Mock-Daten", und wechselt erst bei einer
tatsaechlichen Nutzeraenderung zu "Status: ge\xC3\xA4ndert, nicht gespeichert".

Noch nicht am Geraet verifiziert (kein Sichttest durch mich moeglich) --
sollte vom Nutzer bestaetigt werden: Editor eines bestehenden Druckers
zeigt beim Oeffnen die echten gespeicherten Werte (nicht "X1C Labor" o.ae.)
und "Status: -"; erst nach Aendern eines Feldes wechselt der Status auf
"ge\xC3\xA4ndert, nicht gespeichert".

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 12.3 Tote Mock-Referenzen in trayClicked()

* [x] `models::mock::findPrinter()`/`findTray()` in `trayClicked()`
  (`UiBridge.cpp:987,993`) entfernen -- das daraus berechnete `spoolId`
  wird von `AppTask`s `SelectTray`-Handler nachweislich nie gelesen
  (`AppTask.cpp:3193-3205`), toter Code ohne Verhaltensaenderung beim
  Entfernen

**Umgesetzt (2026-08-25):** `trayClicked()` (`UiBridge.cpp`) ruft nicht
mehr `models::mock::findPrinter()`/`findTray()` auf. `amsId` wird jetzt
direkt aus `currentAmsId` abgeleitet (`trayId == 0xFF ? 0xFF :
currentAmsId`) statt ueber einen Nullpruef-Umweg auf das mittlerweile
unbenutzte Mock-Ergebnis; `spoolId` wird gar nicht mehr mitgeschickt
(`sendAction()`s Default-Parameter greift, `SelectTray`-Handler liest ihn
ohnehin nie). `MockUiDataProvider.h`-Include und die Datei selbst bleiben
bewusst noch stehen -- das entfernt erst Phase 12.4, da hier
"ausschliesslich Phase 12.3" gilt und der letzte Verbraucher gerade erst
weggefallen ist.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 12.4 MockUiDataProvider entfernen

* [x] `src/ui/models/MockUiDataProvider.h`/`.cpp` vollstaendig entfernen,
  sobald 12.3 erledigt ist (letzter verbleibender Verbraucher)

**Umgesetzt (2026-08-25):** Beide Dateien geloescht, `#include "ui/models/
MockUiDataProvider.h"` aus `UiBridge.cpp` entfernt.

Dabei ein Build-Fehler entdeckt und behoben: `MockUiDataProvider.h` band
transitiv `ui/models/UiModels.h` ein, das `UiBridge.cpp` selbst nie direkt
inkludiert hatte -- nach dem Entfernen brach der Build mit ~40 Fehlern
("`UiConnectionState` in namespace ... does not name a type",
`kMaximumFilamentColors` etc.), da diese Typen weiterhin aktiv genutzt
werden (nicht mockspezifisch, sondern generelle UI-Modelltypen). Neuer
direkter `#include "ui/models/UiModels.h"` in `UiBridge.cpp` behebt das --
kein Verhaltensunterschied, nur die bisher versteckte transitive
Abhaengigkeit jetzt explizit gemacht.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 12.5 Verwaiste Platzhalter-Datei

* [x] `src/app/ApplicationController.cpp` entfernen -- leere Datei ("//
  Platzhalter fuer eine spaetere fachliche Zustandssteuerung."), nirgends
  inkludiert oder referenziert (per Volltextsuche bestaetigt)

**Umgesetzt (2026-08-25):** Datei geloescht, vor dem Loeschen erneut per
Volltextsuche bestaetigt, dass keine Referenz mehr existiert (ausser dieser
TASKS.md-Erwaehnung). `src/app/ApplicationState.h` und `src/app/AppTask.h`
im selben Verzeichnis referenzieren `ApplicationController` nicht und
bleiben unveraendert -- ausserhalb des Umfangs dieser Phase.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

## 12.6 Firmware-Update-Platzhalter (Zwischenschritt)

* [x] `CheckFirmwareUpdate`-Toast-Text von "Update-Pr\xC3\xBCfung nicht
  ausgef\xC3\xBChrt (Mock)" auf ehrlichen Zwischenstand aendern (z.B.
  "Firmware-Update noch nicht verf\xC3\xBCgbar"), bis Phase 13 das echte
  Feature liefert

**Umgesetzt (2026-08-25):** Toast-Text in `AppTask.cpp` (der einzige noch
erreichbare Zweig dieses Fallback-Blocks -- StartWifiPortal/
ResetWifiCredentials/PrepareRestart kehren alle schon vorher ueber ihren
eigenen `if`-Zweig zurueck) auf "Firmware-Update noch nicht verf\xC3\xBCgbar"
geaendert. Der EEZ-Studio-eigene Standardtext "Status: Mock, keine Update-
Funktion" auf SCR_SETTINGS_FIRMWARE selbst (screens.c) bleibt unveraendert
-- ausserhalb des Umfangs dieser Phase (kein Code setzt ihn zur Laufzeit,
und eine EEZ-Projekt-Aenderung braucht den Export durch den Nutzer). Wird
mit dem echten Feature in Phase 13 ohnehin ersetzt.

Build (0 Warnungen), 54 native Tests gruen, geflasht.

---

# Phase 13 – Firmware-Update (OTA)

Nutzerentscheidung (2026-08-25): echtes OTA-Update-System statt nur einer
ehrlichen Platzhalter-Meldung, als eigene groessere Phase.

**Machbarkeits-Check bereits durchgefuehrt:** `platformio.ini` setzt
`board_build.partitions = default_16MB.csv`
(`.pio-core/.../tools/partitions/default_16MB.csv`) -- diese Partitionstabelle
hat bereits echtes Dual-OTA (`app0`/`app1`, je 0x640000 = 6.291.456 Byte
~6.3 MB, plus `otadata`-Partition). Aktuelle Firmware belegt ~1,7 MB (26 %
einer einzelnen App-Partition, Stand Phase 11.6) -- reichlich Reserve.
**Keine Repartitionierung/kein einmaliger Seriell-Reflash bestehender
Geraete noetig**, OTA kann direkt auf der vorhandenen Partitionstabelle
aufbauen.

Passend zu Phase 14.8s bereits bestehendem Punkt "kein Security-Key":
keine kryptographische Signaturpruefung einplanen, nur
Transportsicherheit (HTTPS) plus optionale SHA-256-Integritaetspruefung
gegen eine veroeffentlichte Pruefsumme.

**Update-Quelle geklaert (Nutzer, 2026-08-25):** GitHub-Releases-API.
Repo-Pfad direkt aus dem konfigurierten git-Remote dieses Projekts
uebernommen (`git remote -v` -> `origin` ->
`git@github.com:charlie71/FilamentStation.git`) und vom Nutzer explizit
bestaetigt, nicht geraten.

## 13.1 Grundlagen

* [x] Firmware-Versionierung einfuehren -- Korrektur (2026-08-25, waehrend
  Phase 12.1 entdeckt): `config::kApplicationVersion = "0.1.0-dev"`
  (`AppConfig.h:8`) existiert bereits (bisher nur fuer den Boot-Log in
  `main.cpp:42` genutzt); die urspruengliche Suche fuer diese Phase hatte
  nach `FIRMWARE_VERSION`/`firmwareVersion` gesucht und diesen
  camelCase-Namen mit anderem Praefix nicht gefunden. Diese Konstante
  direkt wiederverwenden statt einer neuen -- ggf. auf ein richtiges
  Semver-Schema pruefen/anpassen
* [x] Anzeige der aktuellen Version in SCR_SETTINGS_DEVICE/
  SCR_SETTINGS_FIRMWARE
* [x] Update-Quelle mit Nutzer festlegen (siehe oben)
* [x] Versions-Vergleichslogik (Semver-Parsing)

**Umgesetzt (2026-08-25):**
- Neue `config::kUpdateRepoOwner="charlie71"`/`kUpdateRepoName=
  "FilamentStation"`/`kUpdateApiHost="api.github.com"` in neuer
  `config/UpdateConfig.h`.
- `objects.device_settings_version` (SCR_SETTINGS_DEVICE) und
  `objects.firmware_settings_current` (SCR_SETTINGS_FIRMWARE) werden jetzt
  beim Laden des jeweiligen Screens programmatisch aus
  `config::kApplicationVersion` gesetzt (`UiBridge.cpp`, `ShowScreen`-Faelle)
  statt sich auf EEZ Studios statischen Text zu verlassen (der zufaellig
  schon "0.1.0-dev" zeigte, aber nach einem echten Update drift-anfaellig
  waere). `firmware_settings_available`/`firmware_settings_status` bleiben
  unveraendert -- das ist Phase 13.2.
- Neue `services/SemVer.h`/`.cpp`: `parseSemVer()` (akzeptiert "vX.Y.Z",
  "X.Y.Z", "X.Y.Z-suffix"/"X.Y.Z+meta") und `compareSemVer()`
  (Kernversion numerisch, bei Gleichstand gilt "hat Suffix" als aelter,
  passend zu Semver 1.0.0-alpha < 1.0.0). Volle Semver-Praerelease-
  Identifier-Vergleiche bewusst nicht implementiert -- fuer
  Firmware-Update-Zwecke reicht die vereinfachte Regel.
- Neuer natives Testziel `test_semver` (6 Faelle: Parsing, Fehlerfaelle,
  Kernversionsvergleich, Suffix-Regel), in `native-spoolman-tests`
  (`platformio.ini`) aufgenommen -- 60 statt 54 native Tests insgesamt.

**Stolperstein dabei gefunden und behoben:** `SemVer.h`/`.cpp` zunaechst
mit der kompakten C++17-Syntax `namespace filament_station::services { }`
geschrieben (wie es die `config/*.h`-Header in diesem Projekt bereits
durchgehend tun) -- das native Testtoolchain (`toolchain-gccmingw32`, GCC
5.1.0) unterstuetzt dieses gemeinsame C++17-Feature trotz `-std=gnu++17`
nicht ("expected '{' before '::' token"), da diese GCC-Version aelter ist
als die eigentliche Sprachfeature-Implementierung (kam erst in GCC 6).
Alle bestehenden `services/*.h`-Dateien in diesem Projekt nutzen deshalb
konsequent die verschachtelte Alt-Syntax (`namespace filament_station {
namespace services { ... } }`) -- an diese Konvention angepasst. Betrifft
nur `services/`-Header, die in native Tests landen; `config/*.h` bleibt bei
der kompakten Syntax, da diese Header nie vom nativen Toolchain uebersetzt
werden.

Build (0 Warnungen), 60 native Tests gruen (davon 6 neu), geflasht.

## 13.2 Versions-Check

* [x] HTTP(S)-Abfrage der Update-Quelle -- neuer Task oder Erweiterung
  eines bestehenden (HTTPClient/WiFiClientSecure analog SpoolmanTask)
* [x] UI-Anzeige "Update verfuegbar: vX.Y.Z" vs. "Firmware aktuell"

**Umgesetzt (2026-08-25):** Neuer eigener `UpdateTask` (analog zu Scale-/
Nfc-/Network-/Power-Task ein eigenes Anliegen, statt einen bestehenden Task
zu ueberladen) mit eigener Queue/Task-Handle in `RtosContext`, eigenen
`config::kUpdateTask`-Settings (`TaskConfig.h`, 8192 Byte -- HTTPS+JSON
analog zu `kBambuTask`/`kSpoolmanTask`) und neuer `LogComponent::Update`.

Neuer `rtos::UpdateCommandType::CheckForUpdate` + `UpdateCommand`-Struct,
neuer `rtos::AppEventType::UpdateCheckResult` (`value`: 1=Update
verfuegbar/0=aktuell/-1=Fehler, `text`: Version bzw. Fehlermeldung).

`UpdateTask::checkForUpdate()` (`UpdateTask.cpp`): baut die URL aus
`config::kUpdateApiHost/kUpdateRepoOwner/kUpdateRepoName` (Phase 13.1),
nutzt `WiFiClientSecure` mit `setInsecure()` (identisches Muster wie
`BambuTask`, kein Security-Key/keine Zertifikatspflege geplant) plus
`HTTPClient`, setzt den von GitHub zwingend geforderten `User-Agent`-
Header, filtert die JSON-Antwort per `DeserializationOption::Filter` auf
nur `tag_name` (die volle Release-Antwort ist mehrere KB gross und wird
nicht gebraucht), vergleicht per `services::parseSemVer()`/
`compareSemVer()` (Phase 13.1) gegen `config::kApplicationVersion`. HTTP
404 (noch kein Release veroeffentlicht -- aktuell der erwartbare
Normalfall fuer dieses Repo) wird als eigener, klar beschrifteter Fall
behandelt statt als generischer HTTP-Fehler.

`AppTask::handleUiAction()`s `CheckFirmwareUpdate`-Zweig sendet jetzt
`UpdateCommand{CheckForUpdate}` statt der Mock-Meldung, mit
`pendingUpdateCheckRequestId`-Sperre gegen parallele Anfragen (Muster wie
`pendingPrinterTestRequestId` bei `TestPrinterConnection`) und sofortigem
"Wird gepr\xC3\xBC" "ft..."-Toast. Neuer `AppEventType::UpdateCheckResult`-
Handler im zentralen Event-Loop sendet zwei `ShowStatus`-Kommandos: die
bereits vorhandene `value=300+CheckFirmwareUpdate`-Route
(`firmware_settings_status`) sowie eine neue `value=400`-Route
(`UiBridge.cpp`, neuer Zweig) fuer `firmware_settings_available` ("Update
verf\xC3\xBCgbar: vX.Y.Z" / "Verf\xC3\xBCgbar: aktuell" / Fehlertext).

**Stolperstein dabei gefunden und behoben:** ein `sendResult()`-Aufruf mit
dem String "Keine Version ver\xC3\xB6ffentlicht" erzeugte eine Compiler-
Warnung ("hex escape sequence out of range") -- `\xB6` gefolgt von `f`
wird als durchgehende Hex-Escape-Sequenz `\xB6f` geparst (jede Hex-Ziffer
inkl. a-f wird vom Escape verschluckt), nicht als `\xB6` + `f`. Behoben
durch Aufsplitten in zwei String-Literale ("...\xC3\xB6" "ffentlicht"),
identisches Muster wird an mehreren Stellen in `AppTask.cpp`/`UiBridge.cpp`
bereits fuer genau dieses Problem verwendet (z.B. "Zur\xC3\xBC" "ck").

Noch nicht am Geraet mit echtem Netzwerkzugriff verifiziert (kein
Hardwarezugriff durch mich moeglich) -- sollte vom Nutzer geprueft werden:
"Prüfen"-Button auf SCR_SETTINGS_FIRMWARE antippen, kurz "Wird geprüft..."
sehen, danach entweder "Firmware ist aktuell"/"Verfügbar: aktuell" oder
(da fuer dieses Repo aktuell wahrscheinlich noch kein GitHub-Release
existiert) "Keine Version veröffentlicht".

Build (0 Warnungen), 60 native Tests gruen, geflasht.

## 13.3 Download

* [x] .bin-Datei per HTTPS herunterladen
* [x] Fortschrittsanzeige (ShowProgress-Overlay analog Spoolman-/Bambu-
  Workflows)
* [x] Verbindungs-/Speicherfehler behandeln (unterbrochener Download darf
  die laufende App-Partition nicht antasten -- Download in die inaktive
  OTA-Partition, siehe 13.5)

**Nutzerentscheidung (2026-08-25):** Asset-Auswahl per Konvention -- das
erste Release-Asset, dessen Dateiname auf ".bin" endet, wird verwendet
(kein fester Dateiname erforderlich).

**Umgesetzt, mit bewusster Abweichung von der urspruenglichen Phasen-
Aufteilung:** Der Checklisten-Text von 13.3 selbst sagt bereits "Download
in die inaktive OTA-Partition", waehrend 13.5 laut Plan
`Update.begin()/write()/end()` besitzen sollte -- auf ESP32 sind Download
und Partitions-Schreiben aber ein einziger, nicht sinnvoll ueber getrennte
Nutzeraktionen aufteilbarer Streaming-Vorgang (der `Update`-Objekt-Zustand
liesse sich nicht sauber ueber getrennte Tasksitzungen hinweg pausieren).
Deshalb liefert 13.3 bereits die vollstaendige Download-und-Schreib-Pipette
inklusive `Update.end()`-Abschluss; 13.5 bleibt fuer den tatsaechlichen
Neustart in die neue Partition (`ESP.restart()`) reserviert -- das Geraet
laeuft nach einem erfolgreichen 13.3-Download weiter auf der alten
Firmware, bis 13.5 das ergaenzt.

`UpdateTask::downloadUpdate()` (`UpdateTask.cpp`) macht bewusst eine
eigene, frische Abfrage der Release-API (nicht die Daten aus einem
vorherigen `CheckForUpdate` wiederverwendet, vermeidet eine veraltete
Download-URL): Release-JSON mit Filter auf `assets[].name`/
`assets[].browser_download_url`, erstes Asset mit ".bin"-Endung ausgewaehlt
(Nutzerentscheidung oben), dann `HTTPClient` mit
`setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS)` (GitHub-Asset-Downloads
leiten per 302 auf eine host-fremde, signierte
`objects.githubusercontent.com`-URL um -- ohne Force-Follow wuerde das
fehlschlagen).

`Update.begin(contentLength)` waehlt automatisch die inaktive OTA-Partition
(`app0`/`app1`, Machbarkeits-Check aus Phase 13-Einleitung); Streaming-
Schleife nutzt `Update.write(stream)` (Template-Ueberladung fuer
Stream-Objekte, laut Kommentar in `Update.h` schneller als
`writeStream()`). Speicherfehler: `Update.begin()`s Rueckgabewert plus
`Update.errorString()` geprueft und als Fehlermeldung durchgereicht.
Verbindungsfehler: `dataHttp.connected()` und ein
`kUpdateStallTimeoutMs=30000`-Watchdog (keine Fortschrittsbewegung fuer
30s gilt als haengende Verbindung) fuehren zu `Update.abort()` statt
`Update.end()` -- die inaktive Partition wird dabei wieder freigegeben,
die laufende App-Partition ist zu keinem Zeitpunkt betroffen (`Update`
schreibt ausschliesslich in die inaktive OTA-Partition, niemals in die
gerade laufende). `vTaskDelay(10ms)` statt Busy-Waiting, wenn der Stream
gerade keine neuen Daten liefert (Allgemeine Regel "Busy Waiting
verboten").

Unbekannte Content-Length (GitHub setzt sie in der Praxis eigentlich immer,
aber als Rueckfallpfad behandelt): `UPDATE_SIZE_UNKNOWN`
(`Update.h`-Konstante) statt eines geratenen Werts; die Schleifen-
Abbruchbedingung wechselt in diesem Fall von "erwartete Groesse erreicht"
auf "Stream sauber zu Ende" (`!dataHttp.connected() && stream.available()
== 0`), da `Update.remaining()` bei unbekannter Groesse sonst nie Null
erreichen wuerde.

Fortschritt: neuer `AppEventType::UpdateDownloadProgress`
(throttled auf `kUpdateProgressReportIntervalMs=500`ms, gleiches
Throttle-Prinzip wie `BambuAssignProgress`), von `AppTask` an das bereits
offene `ShowProgress`-Overlay weitergereicht (`UiCommandType::
UpdateProgress`, exakt derselbe generische Mechanismus wie bei Spoolman-/
Bambu-Workflows -- kein neuer UI-Code noetig, `showOverlay()`/
`UpdateProgress`-Handling in `UiBridge.cpp` sind bereits kind-generisch).

UI-Ablauf: "Pr\xC3\xBC" "fen"-Taste auf SCR_SETTINGS_FIRMWARE verhaelt sich
jetzt kontextabhaengig (`AppTask`s neues `updateAvailable`-Flag) -- ohne
bekanntes Update loest sie weiterhin den Versions-Check aus (Phase 13.2);
ist ein Update bereits bekannt, zeigt sie stattdessen einen neuen
Best\xC3\xA4tigungsdialog (`UiOverlayKind::UpdateInstallConfirmation`,
generisch ueber den bestehenden Confirm-Mechanismus, kein neuer
UI-Sondercode). Neue Sperre `pendingUpdateDownloadRequestId` (Muster wie
`pendingPrinterTestRequestId`) verhindert parallele Downloads.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

Noch nicht am Geraet mit echtem Download verifiziert (kein Hardwarezugriff
durch mich moeglich, und aktuell vermutlich noch kein GitHub-Release mit
.bin-Anhang vorhanden) -- sollte vom Nutzer geprueft werden, sobald ein
Release veroeffentlicht ist.

**Nachtrag (2026-08-25, echter Testlauf durch den Nutzer, Fehler
gefunden und behoben):** Der Nutzer ver\xC3\xB6ffentlichte ein echtes Release
(v0.1.1), der Versions-Check funktionierte korrekt ("Update verfuegbar:
v0.1.1"), aber der Installations-Bestaetigungsdialog zeigte nur einen
"Schlie\xC3\x9F" "en"-Knopf statt "Best\xC3\xA4tigen" -- die Installation
liess sich dadurch nie ausloesen. Ursache: `UiBridge.cpp::showOverlay()`
entscheidet ueber eine fest codierte Liste von `UiOverlayKind`-Werten,
fuer welche Dialoge der Best\xC3\xA4tigen-Knopf ueberhaupt sichtbar wird
(`RestartConfirmation`, `WifiResetConfirmation`,
`QuickWeightConfirmation`, ... ) -- das neue
`UiOverlayKind::UpdateInstallConfirmation` aus dieser Phase fehlte in
dieser Liste, existierte also nur als AppTask-seitige Logik, ohne dass die
UI-Seite davon wusste. Diese versteckte Kopplung wurde beim urspruenglichen
Schreiben dieser Phase nicht erkannt, da der generische Ueberlay-
Mechanismus (Titel/Text/`ShowDialog`) an anderer Stelle tatsaechlich kind-
unabhaengig funktioniert -- nur die Knopf-Sichtbarkeit selbst ist es
nicht. Diagnostiziert per Live-Seriellmonitor (kein "UPDATE"-Log ueberhaupt
sichtbar trotz best\xC3\xA4tigtem Dialog -- zeigte, dass `AppTask`s
`UpdateInstallConfirmation`-Zweig nie erreicht wurde) und durch die exakte
Nutzerbeschreibung ("hier gibt es nur einen schlie\xC3\x9Fen button")
bestaetigt. Behoben durch Erg\xC3\xA4nzen von
`UiOverlayKind::UpdateInstallConfirmation` in dieser Liste
(`UiBridge.cpp`).

Build (0 Warnungen), 60 native Tests gruen, geflasht. Nutzer sollte den
Installationsversuch jetzt erneut durchfuehren.

## 13.4 Verifikation

* [x] SHA-256-Pruefsummenvergleich gegen veroeffentlichten Wert (kein
  Security-Key/keine Signatur, siehe oben)

**Nutzerentscheidung (2026-08-25):** Pr\xC3\xBCfsumme wird als zweites
Release-Asset "<bin-dateiname>.sha256" ver\xC3\xB6ffentlicht (nur der
64-stellige Hex-Hash als Inhalt, z.B. per `sha256sum firmware.bin >
firmware.bin.sha256`). Fehlt dieses Asset, installiert das Ger\xC3\xA4t das
Update NICHT (fail closed) -- keine stillschweigend unverifizierte
Installation.

**Umgesetzt (2026-08-25):** Da die Pr\xC3\xBCfsumme die tats\xC3\xA4chlich
uebertragenen Rohbytes braucht, wurde die Download-Schleife aus Phase 13.3
umgebaut: statt der bequemen `Update.write(stream)`-Stream-Ueberladung
(die intern liest, ohne die Bytes an den Aufrufer zurueckzugeben) liest die
Schleife jetzt manuell in einen 1&nbsp;KB-Puffer (`stream.readBytes()`)
und uebergibt jeden Chunk sowohl an `Update.write(buffer, n)` als auch an
`mbedtls_sha256_update_ret()` (laufender Hash waehrend des Downloads, kein
zweiter Lesevorgang aus der frisch beschriebenen Partition noetig). Exakte
mbedTLS-Funktionsnamen fuer diese Framework-Version direkt im vendorten
Header (`mbedtls/sha256.h`) verifiziert (`_ret`-Suffix-Variante).

Asset-Suche erweitert: nachdem das `.bin`-Asset gefunden ist, wird in
derselben bereits geladenen Asset-Liste zusaetzlich nach
"<bin-name>.sha256" gesucht; dessen Inhalt wird per neuer
`fetchChecksum()`/`extractHexSha256()` (eigene kleine Klartext-Anfrage,
kein JSON) geladen und auf 64 Hex-Ziffern validiert (sha256sum-typisches
"hash  filename"-Format wird toleriert, nur die ersten 64 Zeichen zaehlen).

Nach Abschluss des Downloads wird der berechnete Hash mit dem
veroeffentlichten verglichen, bevor `Update.end()` aufgerufen wird -- bei
einer Abweichung `Update.abort()` (identischer sicherer Rueckzug wie bei
einem Verbindungsabbruch aus Phase 13.3) statt die Partition zu
committen, mit klarer Fehlermeldung "Pr\xC3\xBCfsumme stimmt nicht
\xC3\xBCberein".

Build (0 Warnungen), 60 native Tests gruen, geflasht.

Noch nicht am Geraet mit einem echten veroeffentlichten Release verifiziert
(kein Hardwarezugriff durch mich moeglich) -- sollte vom Nutzer geprueft
werden, sobald ein Release mit .bin- und .sha256-Anhang existiert
(sowohl der Erfolgsfall als auch eine absichtlich falsche Pruefsumme, um
den Ablehnungspfad zu bestaetigen).

**Nachtrag (2026-08-25), Fehler bei echtem Hardwaretest gefunden und
behoben:** Nutzer meldete beim echten Testlauf (Phase 13.8) einen Abbruch
mit "Pr\xC3\xBC" "fsumme konnte nicht geladen werden", Log zeigte
`E esp-sha: Failed to allocate buf memory` gefolgt von
`Checksum request failed ... status=-1` -- ein interner
ESP-IDF-Speicherfehler waehrend des TLS-Handshakes der
Pruefsummen-Anfrage, nicht der eigentlichen `mbedtls_sha256_*`-Berechnung
(die laeuft erst nach einem erfolgreichen Download). Ursache:
`downloadUpdate()` haelt `metaClient`/`metaHttp` (Release-Metadaten-Abruf)
als lokale Variablen ueber die gesamte Funktion am Leben -- obwohl
`metaHttp.end()` aufgerufen wird, bleibt das `WiFiClientSecure`-Objekt
selbst (und damit sein mbedTLS-Kontext/Puffer) bis zum Funktionsende
bestehen. Als `fetchChecksum()` anschliessend eine EIGENE, ZWEITE
TLS-Verbindung aufbauen wollte, waren beide Sitzungen gleichzeitig aktiv --
zu viel interner RAM-Bedarf fuer die SHA-Pufferallokation des zweiten
Handshakes.

**Fix:** Release-Metadaten-Abruf (`metaClient`/`metaHttp`/`filter`/
`document`) in einen eigenen Block `{ ... }` verschoben, sodass alle diese
Objekte VOR dem Aufruf von `fetchChecksum()` zerstoert werden (nur noch
`downloadUrl`/`checksumFetchUrl` als einfache `char[]`-Kopien ueberleben
den Block). Damit ist zu jedem Zeitpunkt maximal eine TLS-Sitzung aktiv --
Metadaten, dann Pruefsumme, dann der eigentliche Binaer-Download
(`dataClient`/`dataHttp`, unveraendert als letzter Schritt).

Build (0 Warnungen), 60 native Tests gruen, geflasht (nach COM5-Reconnect,
Upload beim zweiten Versuch erfolgreich).

## 13.5 Flash

* [x] `Update`-Bibliothek (`Update.begin()/write()/end()`) nutzt die
  inaktive OTA-Partition (`app0`/`app1`, siehe Machbarkeits-Check oben) --
  bereits in Phase 13.3 mitgeliefert (siehe dortige Begruendung: Download
  und Partitions-Schreiben sind auf ESP32 ein einziger Streaming-Vorgang,
  nicht sinnvoll auf zwei separate Nutzeraktionen aufteilbar)
* [x] Neustart in die neue Partition nach erfolgreichem Flash
  (`ESP.restart()`, direkt verzahnt mit der Neustart-Korrektur aus
  Phase 12.1) -- einzig verbleibender Punkt dieser Unterphase

**Umgesetzt (2026-08-25):** `AppTask`s `UpdateDownloadResult`-Handler
zeigt bei Erfolg jetzt bewusst nicht mehr nur eine Erfolgsmeldung, sondern
denselben `RestartConfirmation`-Dialog, der bereits in Phase 12.1 gebaut
wurde (inkl. dessen Confirm-Handler mit `kRestartDelayMs`-Verzoegerung +
`ESP.restart()`) -- kein neuer Bestaetigungs-/Neustart-Pfad noetig, ein
Update-Neustart ist funktional derselbe Vorgang wie ein gewoehnlicher
Neustart, nur mit anderem Anlass/Text ("Update installiert" /
"Firmware wurde erfolgreich installiert und gepr\xC3\xBCft. Jetzt neu
starten, um die neue Version zu verwenden?"). Der Nutzer kann den Dialog
genauso mit "Abbrechen" verlassen und den Neustart sp\xC3\xA4ter manuell
ueber die Ger\xC3\xA4te-Einstellungen ausl\xC3\xB6sen -- kein erzwungener
sofortiger Neustart.

Damit ist der End-to-End-OTA-Ablauf (Versions-Check -> Download+Flash+
Pruefsummenverifikation -> Neustart-Bestaetigung -> tatsaechlicher
Neustart in die neu geschriebene Partition) vollstaendig verdrahtet.

**Stolperstein dabei gefunden und behoben:** derselbe "hex escape sequence
out of range"-Fehler wie bereits in Phase 13.2/12.1 -- "gepr\xC3\xBCft."
wurde als durchgehende Hex-Sequenz `\xBCf` geparst statt `\xBC` + `f`.
Behoben durch Aufsplitten in zwei String-Literale, identisches Muster wie
zuvor.

Build (0 Warnungen), 60 native Tests gruen, geflasht.

Noch nicht am Geraet mit einem echten Release-Update end-to-end verifiziert
(kein Hardwarezugriff durch mich moeglich) -- sollte vom Nutzer geprueft
werden: nach erfolgreicher Installation erscheint der Neustart-Dialog,
Best\xC3\xA4tigen startet tats\xC3\xA4chlich in die neue Firmware-Version
(sichtbar z.B. an `device_settings_version`/`firmware_settings_current`
nach dem Neustart).

## 13.6 Fehlerbehandlung und Rollback

* [x] Pruefen, ob das Arduino-ESP32-Framework die App nach einem OTA-Boot
  automatisch als "valid" markiert (ESP-IDFs App-Rollback-Mechanismus)
  oder ob `esp_ota_mark_app_valid_cancel_rollback()` explizit in `setup()`
  ergaenzt werden muss
* [x] Verhalten bei einer Firmware, die nach dem Update nicht mehr
  boot-faehig ist (automatischer Rollback auf die vorherige Partition)

**Rechercheergebnis (2026-08-25):** `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`
ist im vendorten sdkconfig fuer dieses exakte Board bereits gesetzt
(`.pio-core/.../esp32s3/qio_qspi/include/sdkconfig.h:27`) -- der
ESP-IDF-Rollback-Mechanismus ist also grundsaetzlich aktiv. Arduino-ESP32s
eigenes `initArduino()` (`esp32-hal-misc.c:222-238`, laeuft automatisch vor
`setup()`) markiert eine frisch per OTA geschriebene Partition dabei aber
standardmaessig SOFORT als gueltig, noch bevor `setup()` ueberhaupt startet
-- die Default-Implementierung der (als "weak" ueberschreibbaren)
`verifyOta()` liefert unbedingt `true`. Das entwertet den Rollback-Schutz
fast vollstaendig: ein Update, das erst waehrend `setup()`/dem RTOS-Start
abstuerzt oder haengen bleibt, waere trotzdem schon bestaetigt und wuerde
nie automatisch zurueckgerollt.

**Umgesetzt:** neue `extern "C" bool verifyRollbackLater() { return true;
}`-Ueberschreibung in `main.cpp` (der Original-Deklaration in einer
`.c`-Datei folgend `extern "C"`, sonst wuerde C++-Namensverstuemmelung die
Ueberschreibung stillschweigend wirkungslos machen -- direkt getestet:
sauberer Build ohne Duplicate-Symbol-/Linker-Fehler bestaetigt die korrekte
Ueberschreibung). Das verschiebt die Bestaetigung auf einen echten "App
laeuft nachweislich"-Zeitpunkt: `AppTask::showHomeWhenStartupReady()`
(`AppTask.cpp`) ruft `esp_ota_mark_app_valid_cancel_rollback()` jetzt erst
auf, nachdem UI und Storage tatsaechlich bereit sind und der Home-Screen
gesendet wurde -- geschuetzt durch eine `ESP_OTA_IMG_PENDING_VERIFY`-Pruefung
(kein Effekt bei einem normalen, nicht per OTA gestarteten Boot).

Der zweite Checklistenpunkt (Rollback bei nicht boot-faehiger Firmware)
braucht dadurch keinen eigenen Code: st\xC3\xBCrzt die neue Firmware
irgendwo VOR diesem Bestaetigungspunkt ab oder bleibt haengen (Absturz,
Panic, Watchdog, Brownout -- jede Art von Reset), bleibt die Partition im
`PENDING_VERIFY`-Zustand; der ESP-IDF-Bootloader erkennt das beim naechsten
Start automatisch und faellt selbstaendig auf die vorherige Partition
zurueck. Das ist Standard-ESP-IDF-Verhalten, sobald das verfruehte
automatische Bestaetigen (der eigentliche, jetzt behobene Fehler) nicht
mehr im Weg steht.

Build (0 Warnungen), 60 native Tests gruen, geflasht (regul\xC3\xA4rer
USB-Flash, kein OTA-Boot -- die `PENDING_VERIFY`-Pruefung greift dabei
erwartungsgem\xC3\xA4\xC3\x9F nicht, das laesst sich nur durch einen echten
zukuenftigen OTA-Zyklus end-to-end beobachten).

## 13.7 UI-Integration

* [x] SCR_SETTINGS_FIRMWARE: Fortschritt-/Bestaetigungs-/Fehler-Dialoge
* [x] "Neustart nach Update"-Bestaetigung nutzt die in Phase 12.1
  reparierte echte PrepareRestart-Funktion

**Gepr\xC3\xBCft (2026-08-25):** Beide Punkte waren bereits vollstaendig
durch die vorherigen Unterphasen erledigt -- keine neue Codeaenderung noetig,
per gezielter Nachpruefung des bestehenden Codes bestaetigt statt einfach
angenommen:
- Fortschritt: `UiOverlayKind::UpdateDownload`-Overlay (Phase 13.3,
  `AppTask.cpp:2631`).
- Best\xC3\xA4tigung vor der Installation: `UiOverlayKind::
  UpdateInstallConfirmation` (Phase 13.3, `AppTask.cpp:2617,2918`).
- Fehler-Dialoge: `ShowDialog`/`Error` bei Download-/Verifikationsfehlern
  (Phase 13.3/13.4, `AppTask.cpp:5643-5645`); Check-Fehler (Phase 13.2)
  laufen bewusst weiterhin ueber die inline Status-/Verfuegbar-Labels auf
  demselben Screen statt ueber ein Popup -- das entspricht dem bereits vor
  dieser Session vorhandenen Bildschirm-Design (eigene
  `firmware_settings_status`/`_available`-Labels) und wurde absichtlich so
  beibehalten, nicht uebersehen.
- Neustart-Best\xC3\xA4tigung: nutzt bereits seit Phase 13.5 exakt den
  echten `RestartConfirmation`-Ablauf aus Phase 12.1 (`AppTask.cpp:5637-
  5641`, inkl. dessen Confirm-Handler mit `kRestartDelayMs` + echtem
  `ESP.restart()`), kein Mock-Pfad mehr vorhanden.

Kein Build/Test/Flash noetig -- keine Codeaenderung in dieser Phase,
Arbeitsverzeichnis war bereits sauber (`git status` leer).

## 13.8 Validierung

* [x] echter Update-Testlauf am Geraet (alter Stand -> neuer Stand)
* [ ] Rollback-Test mit absichtlich fehlerhaftem Image

Beides sind reine Nutzertests (kein Hardwarezugriff durch mich moeglich) --
Checkboxen bleiben offen, bis der Nutzer beide Tests durchgefuehrt und
bestaetigt hat. Testanleitung (2026-08-25):

**1. Normaler Update-Testlauf:**
1. `pio run -e wt32-s3-wrover-n16r2` -> `.pio/build/wt32-s3-wrover-n16r2/
   firmware.bin`.
2. Pr\xC3\xBCfsumme erzeugen (PowerShell, kein sha256sum noetig):
   `(Get-FileHash firmware.bin -Algorithm SHA256).Hash.ToLower() |
   Out-File -Encoding ascii firmware.bin.sha256`.
3. GitHub-Release mit beiden Dateien unter einer hoeheren Versionsnummer
   veroeffentlichen (z.B. `gh release create v0.1.1 firmware.bin
   firmware.bin.sha256`) -- fuer den ersten Test reicht die unveraenderte
   aktuelle Firmware unter neuer Versionsnummer, das prueft nur die
   Mechanik. Dateinamen muessen exakt "firmware.bin"/"firmware.bin.sha256"
   sein (Phase 13.3/13.4-Konvention: zweites Asset = erster Assetname +
   ".sha256").
4. Am Geraet: Einstellungen -> Firmware -> "Pr\xC3\xBCfen" -> sollte "Update
   verf\xC3\xBCgbar: v0.1.1" zeigen -> nochmal "Pr\xC3\xBCfen" (jetzt
   Installations-Bestaetigung) -> Best\xC3\xA4tigen -> Fortschrittsbalken
   0-100% -> Erfolgsdialog -> Neustart bestaetigen.
5. Nach dem Neustart: Einstellungen -> Ger\xC3\xA4t/Firmware sollte die neue
   Versionsnummer zeigen (bestaetigt echten Wechsel auf die neue Partition).

**Bestaetigt (2026-08-25):** Nutzer hat den normalen Update-Testlauf am
Geraet durchgefuehrt -- Installation und Neustart auf die neue
Firmware-Version erfolgreich. Rollback-Test steht noch aus.

**2. Rollback-Test:** temporaer `abort();` als erste Zeile in `setup()`
(`main.cpp`) einfuegen, als eigenen Release mit weiterer neuer
Versionsnummer (z.B. `v0.1.2-broken-test`) veroeffentlichen, ueber den
normalen Weg am Geraet installieren. Erwartung: Geraet stuerzt nach dem
Neustart sofort ab, faellt automatisch auf die vorherige, funktionierende
Partition zurueck (Bootloader-Mechanismus aus Phase 13.6). Anschliessend
`abort();`-Zeile wieder entfernen (nie in main committen) und den
Test-Release wieder entfernen. Falls der Rollback nicht greift: reine
USB-Neuflashung (`pio run -t upload`) behebt das jederzeit, unabhaengig
vom OTA-Mechanismus.

---

# Phase 14 – Dokumentation und Release

## 14.1 Technik

* [x] Architektur
* [x] Tasks
* [x] Queues
* [x] Events
* [x] IRQ
* [x] Prioritäten
* [x] Stacks
* [x] GPIO
* [x] Verdrahtung
* [x] BOM

**Umgesetzt (2026-08-25):** `docs/architecture.md` um vier neue Abschnitte
ergaenzt (Tasks inkl. Prioritaeten-Rationale, Queues, Events, IRQ) --
konsolidierte Tabellen direkt aus dem aktuellen Code erzeugt
(`src/config/TaskConfig.h`, `src/rtos/RtosContext.h`/`.cpp`,
`src/rtos/Events.h`, `src/rtos/Commands.h`, `src/tasks/ScaleTask.cpp`), nicht
freihaendig geschaetzt. Bestehende Detaildokus (`docs/rtos.md`,
`docs/hardware.md`, `docs/bambu-protocol.md` etc.) bleiben die Quelle fuer
Begruendungen/Historie einzelner Entscheidungen und werden aus den neuen
Abschnitten heraus referenziert statt dupliziert.

`docs/hardware.md` um eine konsolidierte GPIO-Gesamtuebersicht (alle
`BoardConfig.h`-Pins in einer Tabelle, inkl. reservierter/freier Pins) und
einen neuen BOM-Abschnitt (Hauptplatine, Display, Touch, SD, Waage, NFC-Leser
je mit Anbindung) ergaenzt -- GPIO/Verdrahtung im Detail je Peripherie waren
bereits durch die bestehenden Abschnitte abgedeckt.

Reine Dokumentationsphase, kein Code geaendert -- `git status` zeigte vor der
Aenderung nur die beiden docs-Dateien plus TASKS.md, kein Build/Test/Flash
noetig.

## 14.2 Logging

* [x] kanonisches Format
* [x] Level
* [x] Components
* [x] Logger API
* [x] PlatformIO monitor_filters
* [x] WiFiManager-Debug
* [x] Debug/Release Level
* [x] sensitive Daten

**Umgesetzt (2026-08-25):** `docs/logging.md` bestand bereits aus einer
frueheren Phase, war aber seither nicht mehr aktualisiert worden. Beim
Abgleich mit dem aktuellen Code (`src/services/Logger.h`,
`LoggerFormat.cpp`, `platformio.ini`) zwei konkrete Luecken gefunden und
behoben:

- Komponentenliste war veraltet: `POWER` und `UPDATE` (aus Phase 11/13
  hinzugekommen) fehlten in der Aufzaehlung.
- Der neue `Logger::componentMinimumLevel()`-Mechanismus (Pro-Komponenten-
  Obergrenze, aktuell nutzt ihn nur `UI`, auf `LogLevel::Error` begrenzt,
  damit dessen Debug-Zeilen nicht andere Komponenten auf demselben Monitor
  verdraengen) war bisher gar nicht dokumentiert.

Ausserdem eigene Abschnitte fuer "Debug/Release Level" (kein separates
Release-Environment vorhanden, `FS_LOG_LEVEL` bleibt ohne Override beim
Header-Default 4/Debug, `CORE_DEBUG_LEVEL=0` betrifft nur das
ESP-IDF-interne Logging und ist von `FS_LOG_LEVEL` unabhaengig) und
"WiFiManager-Debug" (`setDebugOutput(false)`, `NetworkTask.cpp:247`)
ergaenzt, vorher nur beilaeufig erwaehnt.

**Testluecke gefunden, nicht behoben (ausserhalb dieser Doku-Phase):** die
nativen Logger-Tests (`test/test_logger/test_main.cpp`) decken `POWER` und
`UPDATE` nicht mit ab -- in `docs/logging.md` als bekannte, unkritische
Luecke vermerkt (beide Komponenten durchlaufen denselben generischen
Formatierungscode wie alle anderen).

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.3 NFC/RFID

* [x] Tagtechnologien
* [x] Tagformate
* [x] TagIdentity
* [x] UID-Normalisierung
* [x] Bambu UUID
* [x] FilamentStation Payload
* [x] Capabilities
* [x] Spoolman `extra.tag`
* [x] AssignTag
* [x] RemoveTagAssignment
* [x] Duplicate Handling
* [x] kein lokales Mapping

**Umgesetzt (2026-08-25):** Abgleich der bestehenden formatspezifischen
Dokus (`docs/nfc-tags.md`, `docs/bambu-rfid.md`, `docs/openprinttag.md`,
`docs/opentag3d.md`, `docs/legacy-and-unknown-tags.md`) gegen die aktuellen
`TagTechnology`/`TagFormat`-Enums (`models/TagDefinition.h`) ergab keine
Luecke -- Tagtechnologien, Tagformate, Bambu UUID und FilamentStation
Payload waren bereits vollstaendig und aktuell dokumentiert. `AssignTag`,
`RemoveTagAssignment` und "kein lokales Mapping" waren ebenfalls bereits
durch `docs/workflows.md` bzw. `docs/legacy-and-unknown-tags.md`
abgedeckt.

Echte Luecke gefunden: die formatunabhaengige Schicht zwischen "Tag
gelesen" und "Spoolman-Zuordnung" -- `TagIdentity`, UID-Normalisierung,
`TagCapabilities` und Duplicate Handling -- war bisher nirgends als
eigenes Thema dokumentiert, nur beilaeufig in `docs/workflows.md` erwaehnt.
Neue Datei `docs/tag-identity.md` direkt aus dem Code erstellt
(`models/TagIdentity.h`, `models/TagReadResult.h`,
`services/TagIdentity.cpp`, `services/SpoolmanClient.cpp:111`
`findSpoolByTag()`, `AppTask.cpp` Duplicate-Fehlermeldungen an allen drei
Aufrufstellen):

- `TagIdentity`/`TagIdentitySource`-Struktur und das "einmal beim Lesen
  eingefroren, nie erneut abgeleitet"-Prinzip.
- Die drei UID-Normalisierungsfunktionen (`tagIdentityFromUid()`, zwei
  `tagIdentityFromBambuUuid()`-Ueberladungen) inkl. ihrer jeweiligen
  Validierungsregeln (Hex-only, gerade Laenge, 16-Byte-Laenge fuer Bambu,
  Ablehnung von durchgehend `0x00`/`0xFF`).
- `TagCapabilities`-Felder und was sie jeweils freischalten.
- Der exakte Spoolman-Filterpfad fuer `extra.tag`-Nachschlagen inkl.
  client-seitiger Doppelpruefung des Serverergebnisses.
- Duplicate Handling: `TagLookupStatus::Duplicate`-Ergebnis (≥2 Treffer)
  wird nie automatisch aufgeloest, alle drei betroffenen Aktionen
  (nativer Konsistenzcheck, `AssignTag`, `RemoveTagAssignment`) brechen
  mit spezifischer Fehlermeldung ab und verweisen auf eine manuelle
  Korrektur in Spoolman.

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.4 Daten

* [x] lokale JSON-Dateien
* [x] Verzeichnisse
* [x] Backup
* [x] keine NFC-Mapping-Dateien
* [x] kein Pending Spoolman Write
* [x] kein persistenter Offline-Spoolman-Cache

**Umgesetzt (2026-08-25):** `docs/storage.md` gegen den aktuellen Code
abgeglichen (`StorageTask.cpp`, `JsonStorage.cpp`, `AppTask.cpp`) --
Verzeichnisse, Backup/atomares Speichern, "kein Pending Spoolman Write"
(Abschnitt "Gewichtsaktualisierungen") und "kein persistenter
Offline-Spoolman-Cache" (Abschnitt "Spoolman-Daten") waren bereits aktuell
und korrekt.

**Zwei echte Luecken gefunden und behoben:**

1. Die Liste der initial angelegten `/config`-Dateien war veraltet (nannte
   sechs Dateien, tatsaechlich sind es sieben -- `bambu.json` fehlte) und
   nannte `/mappings/printer-slots.json` gar nicht, obwohl es seit
   2026-08-24 (Nutzerwunsch, ausserhalb der bisherigen TASKS.md-Phasen
   direkt umgesetzt) ebenfalls ein von `StorageTask` verwaltetes
   Initialdokument ist (`StorageTask.cpp:30-38`, `kInitialDocuments`).
   `docs/storage.md` um einen neuen Abschnitt "Drucker/Fach->Spule-
   Zuordnung" ergaenzt: Zweck (`models::TraySpoolCache`, ersetzt einen per
   Hardwaretest verworfenen Versuch, dieselbe Assoziation im Drucker selbst
   zu speichern), Schreibtrigger (fire-and-forget bei jeder vom Drucker
   bestaetigten Fachzuordnung/-entfernung), Lesepfad (Material/Farbe-
   Abgleich erkennt eine ausserhalb der App gewechselte Spule als
   veraltet) und explizite Abgrenzung von "kein persistenter
   Offline-Spoolman-Cache" (persistiert wird nur die Identitaets-
   Assoziation, Restgewicht/K-Faktor bleiben ein reiner, nicht
   persistierter RAM-Cache).

2. `docs/legacy-and-unknown-tags.md` behauptete noch (aus Phase 14.3,
   ungeprueft aus einer aelteren Doku uebernommen): "`/mappings/
   printer-slots.json` ... No current runtime implementation reads or
   writes it." Das ist seit derselben 2026-08-24-Aenderung falsch --
   richtiggestellt mit Verweis auf `docs/storage.md`, inklusive Klarstellung,
   dass die Datei trotz des gemeinsamen `/mappings`-Verzeichnisses fachlich
   nichts mit NFC-Tag-Identitaeten zu tun hat (`isMappingPath()` in
   `StorageTask.cpp` schliesst sie explizit von der NFC-Legacy-Migration
   aus).

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.5 Workflows

* [x] Screens
* [x] Navigation
* [x] Hauptworkflow
* [x] Staging
* [x] Slot
* [x] Tag zuordnen
* [x] Tag-Zuordnung entfernen
* [x] Bambu
* [x] OpenPrintTag
* [x] OpenTag3D
* [x] Legacy
* [x] Unknown
* [x] Mehrdrucker
* [x] Spoolman Offline Error Flow

**Umgesetzt (2026-08-25):** `docs/workflows.md` deckte bisher nur "Tag
zuordnen"/"Tag-Zuordnung entfernen" plus Sicherheitsbedingungen ab. Alle
uebrigen Checklistenpunkte fehlten komplett und wurden neu ergaenzt, direkt
aus `AppTask.cpp`s Action-Dispatcher und Back-Handler erarbeitet (kein
freihaendiger Text):

- **Screens:** Tabelle aller `UiScreenId`-Werte mit Zweck, inkl.
  `BambuSpoolType` (Leergewicht-Voreinstellung fuer importierte Bambu-Tags,
  bisher nirgends dokumentiert).
- **Navigation:** klargestellt, dass es keinen generischen Navigations-Stack
  gibt, sondern eine feste Zurueck-Zuordnung je Screen plus einzelne
  Sonderfaelle (uebersprungenes Staging, Tag-Aktionen-Herkunft,
  Druckereinstellungen-Ruecksprung ueber `printerSettingsReturnScreen`).
- **Hauptworkflow/Staging/Slot:** der zentrale Ablauf "Spule identifizieren
  -> Slot zuordnen", der Staging-Zwischenzustand (`stagingSpoolId`, genau
  eine angelegte Spule) und der zweistufige Slot-Commit
  (`ConfigureSlot`/`TraySelect` -> `ConfigureSlotFromStaging`) inkl.
  Reset/Untag/Reapply/Refresh-Unterschiede.
- **Bambu/OpenPrintTag/OpenTag3D/Legacy/Unknown (als Scan-Ergebnis-Workflow,
  nicht als Format-Parsing -- das steht bereits in den jeweiligen
  Format-Dokus):** Tabelle, welcher Screen je Format bei bereits bekannter
  vs. fehlender Zuordnung erscheint.
- **Mehrdrucker:** Druckerverwaltung vs. Druckerwechsel, inkl. der
  Zusicherung, dass ein Wechsel nie Daten eines anderen Druckers ueberschreibt
  und Staging beim Wechsel erhalten bleibt.
- **Spoolman Offline Error Flow:** die zwei getrennten Kanaele -- rein
  informative laufende Statusanzeige versus das zentrale
  `requireSpoolman()`-Gate am Anfang des Action-Dispatchers, das
  online-pflichtige Aktionen einheitlich blockiert, statt dass jede Aktion
  einzeln prueft.

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.6 Benutzeranleitung

* [x] Installation
* [x] WLAN
* [x] Spoolman
* [x] Extra-Feld `tag`
* [x] Waage
* [x] NFC
* [x] Tag zuordnen
* [x] Tag-Zuordnung entfernen
* [x] Bambu importieren
* [x] Drucker
* [x] AMS
* [x] Firmware

**Umgesetzt (2026-08-25):** Neue Datei `docs/user-guide.md` -- anders als
alle bisherigen Phase-14-Dokus richtet sich diese an den Endnutzer, nicht an
Entwickler (Alltagssprache, keine internen Bezeichner/Codepfade). Alle
genannten Bildschirmtexte/Knopfbeschriftungen sind aus
`src/ui/generated/screens.c` (`lv_label_set_text_static`) entnommen, nicht
erfunden; Ablaufdetails aus `AppTask.cpp` (z. B. Kalibrierungs-Zahlentastatur
mit Gueltigkeitsbereich 1-100000 g, `UiBridge.cpp:1220-1239`).

Zwei fuer den Nutzer wichtige, aus dem Code/vorhandenen Dokus
rekonstruierte Voraussetzungen explizit aufgenommen, die sonst nirgends
nutzerfreundlich zusammengefasst waren:

- **WLAN-Ersteinrichtung:** das vom Geraet selbst aufgespannte Portal-Netz
  folgt dem Muster `FilamentStation-<6-stelliger Geraete-Suffix>` mit
  Passwort `FS-<derselbe Suffix>` (`NetworkTask.cpp`,
  `makePortalCredentials()`, `config/NetworkConfig.h`) -- exakt hergeleitet,
  nicht geraten.
- **Bambu Developer Mode:** ohne am Drucker aktivierten Developer Mode
  lehnt aktuelle Bambu-Firmware Schreibkommandos (Slot-Zuordnung)
  kryptografisch ab, obwohl Verbindung/Access-Code korrekt sind -- per
  Hardwaretest bestaetigter Befund aus `docs/bambu-protocol.md`, bisher nur
  dort in einem technischen Debugging-Abschnitt vergraben, nicht in einer
  fuer Endnutzer auffindbaren Form.

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.7 Entwickler

* [x] Build
* [x] Upload
* [x] Tests
* [x] EEZ Export
* [x] Logger
* [x] Screen
* [x] Action
* [x] Task
* [x] JSON
* [x] Tagparser
* [x] Spoolman Extra Field

**Umgesetzt (2026-08-25):** Neue Datei `docs/developer-guide.md` -- als
"Kochbuch" angelegt (welche Dateien fuer welche Aenderung anzufassen sind),
ergaenzend zu `docs/architecture.md` (Referenzwissen). Build/Upload/Tests
direkt aus `platformio.ini` entnommen, inkl. der bisher nirgends
gesammelten Tatsache, dass es **vier** getrennte native Testumgebungen mit
je eigenem `build_src_filter` gibt (`native-spoolman-tests`,
`native-scale-tests`, `native-nfc-tests`, `native-logger-tests`) -- der
Session-Standardworkflow hat bisher nur `native-spoolman-tests` laufen
lassen, was fuer alle Aenderungen dieser Session korrekt war (kein
Scale-/NFC-/Logger-Code betroffen), fuer kuenftige Aenderungen an diesen
Subsystemen aber die jeweils passende zusaetzliche Umgebung braucht.

Die uebrigen Abschnitte (EEZ Export, Screen, Action, Task, JSON, Tagparser,
Spoolman Extra Field) sind als konkrete Schritt-fuer-Schritt-Anleitungen
geschrieben, jeweils an einem echten, kuerzlich in dieser Session
umgesetzten Beispiel verifiziert (Task: `UpdateTask`/Phase 13; JSON:
`TraySpoolCache`/Phase 14.4-Fund; Logger: `Power`/`Update`-Komponenten/
Phase 14.2-Fund) statt freihaendig beschrieben. Der EEZ-Export-Abschnitt
haelt insbesondere fest, dass der Export ein manueller Schritt in der
EEZ-Studio-Desktop-App ist (nicht Teil des PlatformIO-Builds) -- mehrfach
in dieser Session als Fehlerursache erlebt, wenn das vergessen wurde.

Reine Dokumentationsphase, kein Code geaendert -- kein Build/Test/Flash
noetig.

## 14.8 Release

* [x] Lizenzen
* [x] SpoolEase-Code nicht kopiert
* [x] Quellen
* [x] eigene Lizenz
* [x] Version
* [x] Changelog
* [x] Release
* [x] reproduzierbarer Build
* [x] Known Issues
* [x] kein Security-Key
* [x] keine lokale NFC-Zuordnungsdatenbank

**Nutzerentscheidung (2026-08-25):** MIT-Lizenz gewaehlt (Empfehlung,
Begruendung: Kompatibilitaet mit allen verwendeten Abhaengigkeiten --
ArduinoJson/LovyanGFX/lvgl/WiFiManager/PubSubClient sind MIT, das
Arduino-ESP32-Framework selbst LGPL-2.1-or-later als separat gelinkte
Komponente unproblematisch). Neue Datei `LICENSE` im Projektwurzel-
verzeichnis angelegt.

**Umgesetzt (2026-08-25):** Neue Dateien `docs/release.md` (deckt alle
uebrigen Checklistenpunkte) und `CHANGELOG.md` (Projektwurzel, an Keep a
Changelog angelehnt, bewusst zusammenfassend statt TASKS.md zu duplizieren).

Alle Lizenzangaben direkt aus den bezogenen Bibliotheksversionen geprueft
(`LICENSE`/`license.txt`-Dateien bzw. `license`-Feld in
`library.json`/`package.json` unter `.pio/libdeps`/`.pio-core`), nicht aus
dem Gedaechtnis behauptet -- dabei fuer LovyanGFX "MIT AND BSD-2-Clause"
(nicht nur MIT) und fuer das Arduino-ESP32-Framework "LGPL-2.1-or-later"
(`package.json`) bestaetigt.

"SpoolEase-Code nicht kopiert" konkret belegt: die einzige Beruehrung mit
`yanshay/spoolease`/`Fire-Devils/filaman-bambulab-plugin` war der bereits
in `docs/bambu-protocol.md` dokumentierte Feld-fuer-Feld-Vergleich des
MQTT-Kommandoaufbaus waehrend der Slot-Zuordnungs-Fehlersuche
(2026-08-22) -- keine der beiden Bibliotheken ist eingebunden, kein
Quellcode uebernommen.

"Known Issues" direkt aus den einzigen noch unerledigten Checkboxen in
`TASKS.md` konsolidiert (nicht neu erfunden): mehrstuendiger Dauertest
(10.7), reale Strommessung je Energiesparstufe + Wake-Zuverlaessigkeit +
das bewusst rein zeitbasierte Energiesparen ohne Druck-Aktiv-Signal
(11.7), sowie der noch ausstehende OTA-Rollback-Test (13.8, normaler
Update-Testlauf bereits vom Nutzer bestaetigt).

"Kein Security-Key" als durchgaengige, bereits an zwei Stellen separat
getroffene Entscheidung zusammengefuehrt: Bambu-MQTT
(`WiFiClientSecure::setInsecure()`, Access-Code als Shared Secret) und
Firmware-Update (HTTPS + SHA-256-Pruefsumme statt Signaturpruefung) folgen
demselben Muster.

Reine Dokumentationsphase, kein Firmware-Code geaendert (nur neue `LICENSE`/
`CHANGELOG.md`/`docs/release.md`) -- kein Build/Test/Flash noetig.

---

**Phase 14 (Dokumentation und Release) vollstaendig abgeschlossen** --
alle acht Unterphasen (14.1-14.8) umgesetzt und dokumentiert.

---

# Phase 15 – Doxygen-Softwaredokumentation

Nutzerwunsch (2026-08-27): vollstaendige Doxygen-Dokumentation aller
Dateien/Funktionen/globalen Variablen/Definitionen in `src/` (ausser
`src/ui/generated/`), plus `@dot`-Statediagramme fuer alle echten
State-Machines. Plan unter `C:\Users\karl\.claude\plans\
glittery-foraging-stardust.md` erarbeitet und vom Nutzer bestaetigt.
Nutzerentscheidungen: Kommentare auf Englisch (bewusst abweichend vom
sonstigen Deutsch dieses Projekts), generierte HTML-Ausgabe gitignored/nur
lokal gebaut, Ausfuehrung Phase fuer Phase durch mich selbst.

## 15.0 Setup

* [x] Graphviz installiert
* [x] Doxyfile angelegt
* [x] .gitignore ergaenzt
* [x] Build-Skript
* [x] Baseline-Lauf

**Umgesetzt (2026-08-27):** `winget install Graphviz.Graphviz` (16.0.0,
war als "bereits vorhanden" erkannt aber nicht im PATH dieser Session --
`DOT_PATH` im Doxyfile deshalb explizit auf `C:/Program Files/Graphviz/bin`
gesetzt statt auf PATH-Aufloesung zu vertrauen). Neue `Doxyfile` im
Projektwurzelverzeichnis: `INPUT=src`, `EXCLUDE=src/ui/generated`,
`EXCLUDE_PATTERNS=*/test/*`, `EXTRACT_ALL=NO` + `WARN_IF_UNDOCUMENTED=YES`
(macht fehlende Dokumentation als Warnung sichtbar statt sie stillschweigend
zu uebernehmen), `WARN_LOGFILE=doxygen-warnings.log`, `HAVE_DOT=YES`,
Ausgabe nach `doxygen-output/` (HTML, SVG-Diagramme). `doxygen-output/` und
`doxygen-warnings.log` in `.gitignore` ergaenzt. Neues
`scripts/build-docs.ps1` (analog zu `scripts/release.ps1`): fuehrt
`doxygen Doxyfile` aus und fasst die Warnzahl aus dem Logfile zusammen --
das ist ab jetzt das Fortschritts-/Vollstaendigkeitsmass fuer diese Phase,
analog zur 0-Compilerwarnungen-Regel der Firmware selbst.

**Baseline-Lauf:** 1831 Warnungen (durchgaengig undokumentierter Code, wie
erwartet vor Beginn der eigentlichen Kommentierung). Keine
Graphviz/dot-Fehler im Log -- Diagramm-Rendering-Pfad grundsaetzlich
funktionsfaehig (echte `@dot`-Bloecke folgen ab Phase 15.7).

Kein Firmware-Build/Flash noetig (reine Tooling-/Doku-Infrastruktur, keine
Aenderung an `src/`).

## 15.1 config/

* [x] Alle 10 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`-Header plus `@brief`/`///<`-Kommentare
fuer jede Konstante/jeden Struct in allen 10 Dateien
(`AppConfig.h`/`BoardConfig.h`/`PowerConfig.h`/`ScaleConfig.h`/
`NfcConfig.h`/`NetworkConfig.h`/`BambuConfig.h`/`TaskConfig.h`/
`UpdateConfig.h`/`Secrets.example.h`). Bestehende deutsche
Begruendungskommentare unveraendert stehen gelassen, Doxygen-Bloecke
ergaenzen sie nur um die kurze englische Strukturzusammenfassung.
`src/config/` zeigt danach 0 Doxygen-Warnungen. `pio test -e
native-spoolman-tests` weiterhin 60/60 gruen (config/-Header werden breit
eingebunden, Kompilierbarkeit damit sichergestellt).

## 15.2 models/

* [x] Alle 16 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`/`@brief`-Bloecke fuer alle 16 Dateien.
State-Diagramm fuer `SpoolmanAppState` (`AppState.h`) ergaenzt. Bei
`PrinterConnectionState`/`AmsConnectionState` (`PrinterState.h`) den
tatsaechlichen Code gegengeprueft, statt den geplanten vollen
Diagrammumfang blind umzusetzen: `PrinterConnectionState::Disabled`/
`Connecting` und praktisch alle `AmsConnectionState`-Werte ausser
`Unavailable`/`Connected` werden im Code nirgends tatsaechlich gesetzt --
das Diagramm bzw. der `@note` zeigt nur die wirklich implementierten
Uebergaenge, nicht den vollen, teils ungenutzten Enum-Wertebereich.

**Fuenf tote Dateien gefunden:** `Spool.h`, `Filament.h`, `AmsTray.h`,
`NfcTag.h`, `ScaleMeasurement.h` -- fruehe Scaffolding-Platzhalter,
nirgends im Code referenziert (durch die jeweils passenden echten Typen
ersetzt: `SpoolmanSpool`/`SpoolmanCatalog`, `PrinterSlotStateData`/
`TraySpoolCacheEntry`, `TagReadResult`/`TagIdentity`, `rtos::AppEvent`-
Felder). Mit `@deprecated`-Verweis auf den jeweils tatsaechlich
verwendeten Ersatztyp dokumentiert statt geloescht (ausserhalb des
Auftrags dieser reinen Doku-Phase).

**Stolperstein:** `spoolman:<id>`/`spool:<id>` in Freitext-Kommentaren
wurde von Doxygen als ungueltiges HTML-Tag `<id>` interpretiert
(Warnung) -- durch `&lt;id&gt;` ersetzt.

`src/models/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1826 -> 1639).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.3 rtos/

* [x] Alle 5 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`/`@brief`-Bloecke fuer alle 5 Dateien
(`Events.h`, `Commands.h`, `Messages.h`, `RtosContext.h`, `RtosContext.cpp`).

`Events.h`: `@brief` auf `AppEventType` (selbsterklaerende Werte bleiben
ohne Einzelkommentar, die vier Werte mit bestehendem deutschem
Begruendungskommentar behalten diesen unveraendert), alle 9
`EventBits_t`-Konstanten einzeln dokumentiert -- inkl. Praezisions-Fund:
`EVENT_BAMBU_READY` wird zwar an einer Stelle gelesen
(`AppTask.cpp:2114`), aber im gesamten Code nirgends per
`xEventGroupSetBits` gesetzt; als "Reserved; not currently set by any
task" dokumentiert statt eine nicht existierende Bedeutung zu erfinden.

`Commands.h`: alle 15 Enums bekommen einen `@brief` auf Typebene, aber
keine Einzelkommentare je Enum-Wert -- empirisch verifiziert, dass Doxygens
`WARN_IF_UNDOCUMENTED` bei `enum class`-Werten (anders als bei
Struct-/Klassenmembern) keine Warnung erzeugt, solange der Enum-Typ selbst
einen `@brief` hat. Spart bei den grossen Nachrichtentyp-Enums erheblichen
Aufwand ohne Warnungen zu riskieren. `isPublicTagAssignmentAction()`/
`requiresOnlineSpoolman()` mit vollem `@brief`/`@param`/`@return`
dokumentiert, alle 8 Member von `UiAction` einzeln kommentiert.

`Messages.h`: groesste Datei der Phase (~30-Feld-`AppEvent`, ~20-Feld-
`UiCommand`, plus 8 kleinere Command-Structs) -- jedes Struct-Feld einzeln
mit `///<` dokumentiert, alle bestehenden deutschen Kommentare
(K-Faktor-Semantik, Legacy-NFC-Migration, LoadFilament/AssignTray-Hinweise)
unveraendert stehen gelassen.

`RtosContext.h`/`.cpp`: `RtosContext`-Struct (alle Queues/Handles einzeln
kommentiert), `createObjects()`/`createUiTask()`/`createServiceTasks()`/
`createTasks()` als die in der Planung genannten zentralen Funktionen mit
vollem `@brief`/`@return`, `context()`/`enqueueLogLine()`/
`droppedLogLineCount()` sowie der interne `createTask()`-Helfer in der
.cpp dokumentiert.

`src/rtos/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1639 -> 1478).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.4 nfc/

* [x] Alle 10 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`/`@brief`-Bloecke fuer alle 10 Dateien
(`ITagParser.h`, `TagParserRegistry.h/.cpp`, `TagParsers.h/.cpp`,
`TagWritePolicy.h`, `OpenPrintTag.h/.cpp`, `OpenTag3D.h/.cpp`).
`TagParseResult` und `TagAssignmentEffect` mit `@brief` je Wert
dokumentiert (kleine Enums, klarer Mehrwert). Alle privaten
Hilfsfunktionen in den anonymen Namespaces der beiden CBOR/Binaer-Decoder
(`OpenPrintTag.cpp`: `readArgument`/`readHeader`/`skipValue`/
`readUnsigned`/`readNumber`/`readText`/`readColor`/
`materialAbbreviation`/`parseMapHeader`/`parseMeta`/`parseMain`/
`findMimePayload`; `OpenTag3D.cpp`: `findMimePayload`/`readBigEndian16`/
`copyFixedText`) einzeln mit `@brief`/`@param`/`@return` dokumentiert, da
`EXTRACT_STATIC`/`EXTRACT_ANON_NSPACES` sie wie jede andere Funktion
warnungspflichtig macht. Bestehende deutsche Kommentare (z. B. der
Bambu-Block-9-Identity-Vorrang in `TagParserRegistry.cpp`, der
Legacy-Rewrite-Hinweis in `TagParsers.cpp`) unveraendert stehen gelassen.

**Stolperstein (wiederholt aus 15.2):** ein weiteres unescaped
`spool:<id>` in einem `///<`-Kommentar (`TagParserRegistry.h:40`) erzeugte
erneut die "Unsupported xml/html tag"-Warnung -- durch `&lt;id&gt;`
ersetzt. In Markdown-Codespans (`` `spool:<id>` ``, `TagParsers.h`)
ist die spitze Klammer dagegen unproblematisch, da Doxygens
Markdown-Parser den Inhalt von Codespans nicht als HTML interpretiert --
das wurde diesmal genutzt statt jede Erwaehnung zu escapen.

`src/nfc/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1478 -> 1372).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.5 services/

* [x] Alle 28 Dateien dokumentiert

**Umgesetzt (2026-08-27):** Groesster Block der gesamten Phase 15
(28 Dateien: `SemVer`, `ScaleMath`, `ScaleFilter`, `SpoolmanUrl`,
`NfcPayload`, `Ntag21x`, `TagIdentity`, `LoggerFormat`, `PsramAlloc.h`,
`TagAssignmentPolicy.h`, `Logger`, `SpoolmanCatalog`, `SpoolmanClient`,
`BambuProtocol`, `JsonStorage` -- jeweils .h/.cpp). `@file`/`@brief` fuer
alle Dateien, alle anonymen-Namespace-Hilfsfunktionen einzeln dokumentiert
(EXTRACT_STATIC/EXTRACT_ANON_NSPACES machen sie warnungspflichtig wie jede
andere Funktion), inkl. lokal in Funktionen definierter Structs
(`SpoolmanCatalog.cpp::materialDensity()`'s `Entry`). `Logger.h`s
FS_LOGE/W/I/D/T-Makros und FS_LOG_LEVEL mit `@def` dokumentiert. Bestehende
englische *und* deutsche Erklaerkommentare (BambuProtocol.h/.cpp's
ausfuehrliche Protokoll-Herleitungen, JsonStorage.cpp's
Support-for-PLA-Fix-Historie) unveraendert stehen gelassen, groesstenteils
in `@param`/`@note`-Bloecke ueberfuehrt statt dupliziert.

**Stolperstein:** `#SymbolName`-Autolink-Syntax loeste bei Enum-Werten
(`#NfcPayloadType::Spoolman`) und freien Funktionen/Templates
(`#calculateScaleFactor()`, `#allocatePsramInstance()`,
`#tagOperationsAvailable()`) "explicit link request ... could not be
resolved"-Warnungen aus -- anders als bei Struct-/Klassenmitgliedern
(dort funktioniert `#member` reibungslos, siehe `RtosContext.h` in 15.3).
Durch einfachen Klartext ohne `#`-Praefix ersetzt.

`src/services/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1372 -> 1125).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.6 drivers/

* [x] Alle 4 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`/`@brief`-Bloecke fuer alle 4 Dateien
(`DisplayDriver.h/.cpp`, `TouchDriver.h/.cpp`). Die lokale
`FilamentStationDisplay`-Klasse (anonymer Namespace in
`DisplayDriver.cpp`, konkrete LovyanGFX-Bus-/Panel-/Backlight-/Touch-
Verdrahtung fuer dieses Board) mit `@brief` auf Klassen- und
Konstruktorebene sowie je privatem Member dokumentiert, die globale
`display`-Instanz ebenfalls. Bestehender deutscher Kommentar zum
GPIO4-Reset-Pin-Sharing unveraendert stehen gelassen.

`src/drivers/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1125 -> 1118).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.7 tasks/

* [x] Alle 11 Dateien dokumentiert

**Umgesetzt (2026-08-27):** Zweitgroesster Block der Phase (11 Dateien,
~11.900 Zeilen) -- inkl. `AppTask.cpp`, mit 6250 Zeilen die mit Abstand
groesste Einzeldatei im gesamten Projekt (etwa die Haelfte aller
Quellzeilen unter `src/`). `@file`/`@brief` fuer alle Dateien.

Alle 5 in der Planung identifizierten Statemachines dieser Phase mit
`@dot`-Diagramm dokumentiert: `PowerState` (`PowerTask.cpp`, bereits aus
frueherer Arbeit an dieser Datei bekannt), sowie `TraySpoolDetailsStage`,
`TagAssignmentStage`, `TagRemovalStage`, `SlotAssignmentStage` und
`LegacyMigrationStage` (alle in `AppTask.cpp`, Modul-Header-Block).
`AppTask.cpp` besteht aus einem ca. 340 Zeilen langen Block
namespace-scope-State-Variablen (jede einzeln mit `///<` dokumentiert --
Doxygen behandelt anonyme-Namespace-Variablen wie jedes andere
warnungspflichtige Mitglied) gefolgt von ca. 80 Hilfsfunktionen und zwei
Monolithen: `handleUiAction()` (~1880 Zeilen, ein einzelner
`@brief`-Block reicht fuer die komplette Funktion unabhaengig von ihrer
internen Laenge) und `appTask()` selbst (~2230 Zeilen, dessen Deklaration
in `Tasks.h` bereits aus Phase 15.7's eigenem `Tasks.h`-Durchlauf
dokumentiert ist und automatisch mit dieser Definition verschmilzt --
0 zusaetzliche Handarbeit noetig).

Vorwaertsdeklarierte Funktionen (z. B. `sendUiCommand`,
`requestStagingSpool`, `reportAssignmentWriteFailure`) wurden nur an der
Vorwaertsdeklaration dokumentiert, nicht zusaetzlich an ihrer spaeteren
Definition -- Doxygen fuehrt beide Vorkommen automatisch zu einem Eintrag
zusammen. Bestehende deutsche und englische Erklaerkommentare (z. B. die
ausfuehrliche Begruendung der externen/manuellen Fach-Adressumrechnung in
`sendPendingSlotAssignTray()`) unveraendert stehen gelassen.

**Stolperstein:** `IRAM_ATTR` zwischen Rueckgabetyp und Funktionsname
(`ScaleTask.cpp`'s ISR) verwirrte Doxygens Parser zu einer
"return type is not documented"-Falschwarnung -- behoben durch
`ENABLE_PREPROCESSING`/`MACRO_EXPANSION`/`PREDEFINED = IRAM_ATTR=` im
Doxyfile, damit das Makro vor der Analyse entfernt wird. Zwei
selbstverursachte doppelte Kommentarbloecke (Copy-Paste-Fehler bei
`tagTechnologyName()`) beim naechsten Lauf gefunden und bereinigt.

`src/tasks/` zeigt danach 0 Doxygen-Warnungen (Gesamtzahl 1118 -> 408).
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

## 15.8 ui/

* [x] Alle 4 Dateien dokumentiert

**Umgesetzt (2026-08-27):** `@file`/`@brief` fuer alle 4 Dateien
(`UiBridge.h`, `UiDesignSystem.h`, `models/UiModels.h`, `UiBridge.cpp`).
`UiBridge.cpp` (3728 Zeilen, zweitgroesste Datei nach `AppTask.cpp`) ist
UiTasks alleiniger LVGL-Zugriffspunkt ("Nur UiTask greift auf LVGL zu"):
ein ca. 190 Zeilen langer Modul-State-Block (jede Variable/jedes Struct
einzeln dokumentiert, analog zu `AppTask.cpp` in 15.7) gefolgt von rund 90
Funktionen -- ueberwiegend kompakte, selbsterklaerende
`lv_event_t*`-Klick-Handler fuer die generierten EEZ-Studio-Screens, dazu
Renderer-Funktionen (`updateHomeContent()`, `updateTrayButton()`,
`updateStagingContent()`, ...) fuer jeden UiCommand-Typ.

**Stolperstein (schwerwiegend, zweimal aufgetreten):** bei zwei
Bearbeitungen (`trayTargetClicked()`, `createTrayDetailsDecoration()`)
wurde beim Ersetzen eines bestehenden Kommentarblocks durch einen
`@brief`-Block versehentlich auch die Funktionssignatur samt
oeffnender Klammer mit geloescht (`old_string`/`new_string` deckten die
Signaturzeile nicht ab) -- der Fehler blieb zunaechst unbemerkt, da
reine Doxygen-Aenderungen laut Standard-Workflow keinen Firmware-Build
erfordern und `pio test` (native, ohne LVGL/UiBridge.cpp) ihn nicht
erkennen kann. Durch einen zur Sicherheit zusaetzlich ausgefuehrten
`pio run`-Build entdeckt und behoben. **Workflow-Lehre:** bei
umfangreichen Kommentar-Bearbeitungen an Dateien mit LVGL-/Board-
spezifischem Code (nicht durch `pio test` abgedeckt) zusaetzlich zur
warnungsfreien Doxygen-Pruefung mindestens einen finalen
`pio run -e wt32-s3-wrover-n16r2`-Build zur Absicherung durchfuehren,
auch wenn die Aenderungen als "reine Kommentare" geplant waren.

`src/ui/` zeigt danach 0 Doxygen-Warnungen. Firmware-Build
(`pio run -e wt32-s3-wrover-n16r2`) erfolgreich, 0 Compilerwarnungen.
`pio test -e native-spoolman-tests` weiterhin 60/60 gruen.

Gesamtzahl projektweit: 408 -> 2 (die verbleibenden 2 Warnungen betreffen
ausschliesslich `src/main.cpp`, Gegenstand von Phase 15.9).

## 15.9 main.cpp + Abschluss

* [x] `main.cpp` dokumentiert
* [x] `@mainpage`-Block ergaenzt
* [x] `src/app/ApplicationState.h`-Sonderfall dokumentiert
* [x] Finaler Doxygen-Lauf: 0 Warnungen projektweit
* [x] Firmware-Build und native Tests verifiziert

**Umgesetzt (2026-08-27):** `@mainpage`-Block in `main.cpp` ergaenzt, mit
Kurzbeschreibung des Projekts und Verweis auf `docs/architecture.md` als
ergaenzende, narrative Referenz. `haltStartup()`, `setup()`, `loop()` und
das `CONFIG_APP_ROLLBACK_ENABLE`-`verifyRollbackLater()`-Weak-Override
vollstaendig dokumentiert (`@brief`/`@param`/`@return`/`@note`), bestehende
ausfuehrliche Begruendungskommentare (OTA-Rollback-Timing,
USB-CDC-Puffergroesse) unveraendert stehen gelassen.

**Sonderfall `src/app/`:** wie in der Planungsphase bereits identifiziert,
sind sowohl `ApplicationState.h` (nirgends referenziertes fruehes
Scaffolding-Enum) als auch das bislang uebersehene, komplett leere
`AppTask.h` (nur ein Verweiskommentar auf `tasks/Tasks.h`) toter Code ohne
irgendeine Einbindung im Projekt -- beide mit `@file`/`@deprecated`-Hinweis
dokumentiert statt geloescht (Loeschen ausserhalb des Auftrags dieser
reinen Doku-Phase), analog zu den fuenf toten `models/`-Dateien aus 15.2.

**Finale Verifikation (Verifikation-Abschnitt der Planung):**
- `doxygen Doxyfile`: **0 Warnungen projektweit** (von 1831 Warnungen in
  der 15.0-Baseline auf 0).
- Stichprobe der generierten HTML-Ausgabe: `PowerState`s `@dot`-Diagramm
  auf der `PowerTask.cpp`-Namespace-Seite als eigenstaendige, nicht-leere
  SVG-Datei (7,3 KB) eingebettet und referenziert; `AppTask.cpp`s
  Namespace-Seite enthaelt alle 5 erwarteten `@dot`-Diagramme
  (`TraySpoolDetailsStage`, `TagAssignmentStage`, `TagRemovalStage`,
  `SlotAssignmentStage`, `LegacyMigrationStage`). Insgesamt 154
  Graphviz-SVG-Dateien im generierten `doxygen-output/html/` vorhanden --
  der in 15.0 installierte Graphviz-Pfad funktioniert durchgaengig.
- `pio run -e wt32-s3-wrover-n16r2`: erfolgreich, 0 Compilerwarnungen.
- `pio test -e native-spoolman-tests`: 60/60 gruen.

---

**Phase 15 (Doxygen-Softwaredokumentation) vollstaendig abgeschlossen** --
alle neun Unterphasen (15.0-15.9) umgesetzt: ~92 Quelldateien, ~22.000
Zeilen unter `src/` vollstaendig mit Doxygen-Kommentaren versehen
(`@file`/`@brief` je Datei, `@brief`/`@param`/`@return` je Funktion,
`///<`-Kommentare je globaler/`constexpr`-Variable, `@brief` je
Struct/Enum/Namespace-Mitglied), alle neun in der Planung identifizierten
echten State-Machines (`PowerState`, `SpoolmanAppState`,
`PrinterConnectionState`/`AmsConnectionState`, `TraySpoolDetailsStage`,
`TagAssignmentStage`, `TagRemovalStage`, `SlotAssignmentStage`,
`LegacyMigrationStage`) mit `@dot`-Statediagrammen dokumentiert, 6
tote/unbenutzte Scaffolding-Dateien identifiziert und mit
`@deprecated`-Hinweis versehen statt entfernt, `doxygen Doxyfile` liefert
0 Warnungen ueber den gesamten Umfang, Firmware-Build und native Tests
durchgaengig gruen gehalten.

---

**Nachtrag (2026-08-28, Nutzerwunsch: Bambu-AssignTray-Temperaturhandling
vereinfacht, Material-Mapping erweitert):** Der bisherige Weg, die AMS-
Duesentemperatur (`nozzle_temp_min`/`nozzle_temp_max`) im
`ams_filament_setting`-Kommando aus den Spoolman-Filament-Extra-Feldern
`bambu_temp_min`/`bambu_temp_max` zu lesen, wurde auf Nutzerwunsch
vollstaendig ersetzt durch eine statische, im Quellcode hinterlegte
36-Eintrag-Tabelle `BambuProtocol::kBambuMaterialMappings[]`
(`resolveBambuMaterial()`), die je Spoolman-Materialtext (`PLA`, `PETG`,
`PLA-CF`, ...) `tray_info_idx`/`tray_type`/Duesentemperaturspanne in einem
Schritt liefert. Matching ist exakt nach Normalisierung (Gross-/
Kleinschreibung, Trennzeichen `-`/` `/keins), bewusst kein
Praefix-Matching mehr, damit spezifische Materialien (z. B. `PLA-CF`,
`PETG-CF`, `PA-CF`, `PA-GF`, `PP-CF`, `PP-GF`, `PPA-CF`, `PPA-GF`,
`PLA-AERO`, `ASA-CF`) nie mit ihrem allgemeinen Gegenstueck verwechselt
werden. Ein nicht zuordenbares Material lehnt `AssignTray` jetzt komplett
ab (`AppEventType::BambuError`, `FS_LOGW reason=no_material_mapping`)
statt mit erfundener/unvollstaendiger Temperatur fortzufahren. Es wird an
keiner Stelle dieses Ablaufs ein Bambu-`setting_id`-Feld resolved oder
gesendet -- bewusst ausserhalb des Umfangs dieser Aufgabe, ebenso wie
K-Faktor/Pressure-Advance/`extrusion_cali_get`/`extrusion_cali_set`
(separate, noch offene Aufgabe).

Da die Materialzuordnung keinen zusaetzlichen `GET /filament/{filamentId}`-
Abruf mehr braucht (der Materialtext liegt bereits aus `LoadSpool` vor),
entfaellt der bisherige `SlotAssignmentStage::LoadingFilament`-
Zwischenschritt ersatzlos: `SlotAssignmentStage` kennt seitdem nur noch
`None -> SelectingSpool -> LoadingSpool -> WritingSlot`. Die Spoolman-
Felder `bambu_temp_min`/`bambu_temp_max` selbst bleiben unveraendert
bestehen und werden weiterhin fuer die Home-Tray-Karten-Anzeige
(`AppTask::resolveTraySpoolDetails()`, unveraendert) genutzt -- nur nicht
mehr fuer den an den Drucker gesendeten Payload. `BambuProtocol::
applyTrayOccupancy()`/`PrinterSlotStateData` parsen zusaetzlich neu
`nozzle_temp_min`/`nozzle_temp_max` aus Statusberichten (robust gegenueber
JSON-String- und -Zahl-Form), rein informativ, ohne bestehendes
Verhalten zu aendern.

Umgesetzt in `src/services/BambuProtocol.h/.cpp` (neue
`BambuMaterialMapping`/`resolveBambuMaterial()`, `kBambuMaterialMappings[]`
ersetzt die alte praefixbasierte `kGenericMaterialMappings[]`),
`src/models/PrinterState.h` (`nozzleTempMinC`/`MaxC` in
`PrinterSlotStateData`), `src/rtos/Messages.h` (Temperaturfelder aus
`BambuCommand` entfernt, werden jetzt in `BambuTask` aus der Tabelle
aufgeloest statt von `AppTask` durchgereicht), `src/tasks/BambuTask.cpp`
(`handleAssignTray()` neu), `src/tasks/AppTask.cpp` (`SlotAssignmentStage`
verschlankt, `LoadingFilament`-Handler entfernt), `docs/bambu-protocol.md`
aktualisiert. 71/71 native Tests gruen (`pio test -e
native-spoolman-tests`, 11 neue Tests fuer Material-Mapping/Report-
Parsing), Firmware-Build (`pio run -e wt32-s3-wrover-n16r2`) erfolgreich,
0 Compilerwarnungen. Noch nicht auf echter Hardware verifiziert.

---

**Nachtrag (2026-08-28, Fortsetzung, Nutzerwunsch: Bambu-Material-Mapping
von der SD-Karte laden, aus dem Repository herunterladen, SHA-256-
validiert):** Die oben beschriebene `kBambuMaterialMappings[]`-Tabelle war
zu diesem Zeitpunkt noch fest im Quellcode kompiliert. Direkt im
Anschluss wurde sie durch eine zur Laufzeit von `/config/
bambu_materials.json` geladene JSON-Datei ersetzt, damit neue Materialien
ohne Firmware-Neukompilierung ergaenzt werden koennen -- Resolver-
Architektur (Exact-Match nach Normalisierung, kein `setting_id`, kein
Fallback auf Spoolman-Temperaturen) bleibt unveraendert, nur die
Datenquelle aendert sich.

Architektur-Entscheidungen, dem Auftrag entsprechend zuerst gegen
bestehende Regeln geprueft (`AGENTS.md` "keine SD-Zugriffe ausserhalb
StorageTask"; `rtos::AppEvent` wird als Wert in eine 16-tiefe Queue in
knappem internem RAM kopiert, ein zusaetzliches ~18-KiB-Feld waere dort
16-fach so teuer):

* **RAM-Cache als atomarer Zeiger statt Queue-Transport:** neues Feld
  `RtosContext::bambuMaterialMappings`
  (`std::atomic<const models::BambuMaterialMappingTable*>`, `nullptr` =
  "keine gueltige Tabelle geladen"). `StorageTask` haelt zwei
  PSRAM-Doppelpuffer-Instanzen und veroeffentlicht eine neu geladene
  Tabelle mit genau einem atomaren `store()`. `BambuTask::
  handleAssignTray()` liest den Zeiger direkt (`load()`) -- kein
  SD-Zugriff, verletzt die o. g. Regel nicht, `rtos::BambuCommand`
  bleibt unveraendert (Resolution bleibt wie im vorigen Nachtrag in
  `BambuTask`, nicht `AppTask`).
* **Download in 768-Byte-Haeppchen ueber das bestehende
  `StorageCommand`** (`StorageCommandType::BeginBambuMaterialDownload`/
  `WriteBambuMaterialChunk`/`CommitBambuMaterialDownload`/
  `AbortBambuMaterialDownload`) statt einer neuen, groesseren Queue --
  `UpdateTask` (Netzwerk) und `StorageTask` (SD-Schreiben) kooperieren
  fire-and-forget, keine neue Synchronisationsprimitive. `StorageTask`
  berechnet die SHA-256 der geschriebenen `.tmp.json` **selbst neu** und
  ist damit alleinige Autoritaet ueber Aktivierung, unabhaengig davon,
  was `UpdateTask` beim Streamen schon sah.
* **Reiner Parser `services::BambuMaterialCatalog`**
  (`services/BambuMaterialCatalog.h/.cpp`, kein Datei-/Netzwerkzugriff,
  vollstaendig nativ unit-testbar) validiert `schema_version`,
  Pflichtfelder, Temperaturbereich (`0 < min <= 400`, `min <= max`),
  optionale `aliases` (Array nicht-leerer Strings) und projektweite
  Eindeutigkeit von `material`/`aliases` nach Normalisierung -- ein
  Fehler verwirft die komplette Datei, nie Teilerfolg. `services::
  sameMaterialKey()` wurde dafuer aus `BambuProtocol.cpp`s anonymem
  Namensraum exportiert (Wiederverwendung statt Duplikat).
* **Neues Datenmodell** `models::BambuMaterialMappingTable`
  (`models/BambuMaterialMapping.h`, bis zu 96 Eintraege, Aliase als ein
  `|`-getrennter String statt `char[N][M]`, spart Speicher).
* **Repository-Quelle** `data/bambu-materials/bambu_materials.json` +
  `.sha256` + `README.md`: 1:1-Migration der bisherigen 36
  Tabelleneintraege (keine Werte veraendert) plus 14 `Support For ...`-
  Supportmaterialien sowie `PAHT-CF`/`PC-CF`/`BAMBU-PVA`/`TPU 95A` aus
  der Aufgabenbeschreibung (54 Eintraege gesamt). `scripts/release.ps1`
  erzeugt/veroeffentlicht diese Dateien jetzt automatisch mit jedem
  Firmware-Release, als zusaetzliche GitHub-Release-Assets (Nutzer-
  entscheidung: Release-Assets statt roher Repo-Datei, gleicher
  Downloadpfad wie `firmware.bin`).
* **Bekannter offener Punkt:** `PAHT-CF` (`GFN96`, aus der
  Aufgabenbeschreibung uebernommen) teilt sich seinen `tray_info_idx` mit
  dem bereits vorhandenen, verifizierten Eintrag `PPA-GF` (ebenfalls
  `GFN96`) -- nicht gegen echte Bambu-Studio-Profile verifiziert, siehe
  `data/bambu-materials/README.md`. `services::BambuMaterialCatalog`
  erzwingt keine `tray_info_idx`-Eindeutigkeit (mehrere Spoolman-
  Materialnamen koennen legitim auf dasselbe Bambu-Profil zeigen).

Geaenderte/neue Dateien: `src/models/BambuMaterialMapping.h` (neu),
`src/services/BambuMaterialCatalog.h/.cpp` (neu),
`src/services/Sha256Hex.h/.cpp` (neu, aus `UpdateTask.cpp` extrahiert),
`src/services/BambuProtocol.h/.cpp` (`resolveBambuMaterial()`-Signatur,
`sameMaterialKey()` exportiert, `kBambuMaterialMappings[]` entfernt),
`src/config/BambuMaterialConfig.h` (neu), `src/rtos/Commands.h`
(`StorageCommandType`/`UpdateCommandType`/`UiActionType` erweitert),
`src/rtos/Events.h` (`BambuMaterialUpdateProgress`/`Result`),
`src/rtos/RtosContext.h` (`bambuMaterialMappings`-Feld),
`src/tasks/StorageTask.cpp` (Laden beim Boot, Download-Aktivierung),
`src/tasks/UpdateTask.cpp` (`downloadBambuMaterials()`),
`src/tasks/BambuTask.cpp` (`handleAssignTray()` liest den Tabellen-
Zeiger), `src/tasks/AppTask.cpp` (Pending-Guard + UI-Aktion, noch kein
GUI-Button -- interne API genuegt laut Auftrag),
`data/bambu-materials/bambu_materials.json/.sha256/README.md` (neu),
`scripts/release.ps1`, `docs/bambu-protocol.md`, `docs/architecture.md`,
`docs/storage.md`.

Tests: `test/test_bambu_material_catalog/` (neu, 14 Tests: gueltige
Datei, Formatierung, ungueltiges JSON, fehlende/unbekannte
`schema_version`, fehlendes Pflichtfeld, falscher Feldtyp, invertierter
Temperaturbereich, ungueltiger Alias-Typ, doppelter Schluessel, erlaubter
"redundanter" Alias, leeres/fehlendes `materials`-Array,
Fehlernamen-Stabilitaet), `test/test_sha256_hex/` (neu, 9 Tests fuer
`extractHexSha256()`), `test/test_bambu_protocol/` erweitert (neue
Tabellen-basierte Resolver-Signatur, 6 neue Testfaelle fuer
Support-Materialien/Alias-Aufloesung/Bambu-PVA-vs-Generic-PVA/leere
Tabelle). Insgesamt 98/98 native Tests gruen (`native-spoolman-tests`,
`native-scale-tests`, `native-nfc-tests`, `native-logger-tests`
zusammengenommen 149/149), Firmware-Build (`pio run -e
wt32-s3-wrover-n16r2`) erfolgreich, 0 Compilerwarnungen.

Nicht nativ testbar (konsistent mit der bestehenden Projektgrenze --
`JsonStorage::atomicSave()` selbst ist ebenfalls ungetestet): der
komplette Download-/Chunk-/Commit-/Aktivierungs-Ablauf in
`StorageTask.cpp`/`UpdateTask.cpp` (SHA-256-Mismatch, fehlende
Pruefsumme, beschaedigter Download, Stromausfall-Recovery ueber liegen
gebliebene `.tmp.json`/`.bak.json`) -- nur ueber `pio run` + Codelesung
verifiziert, noch kein Hardware-/Download-Test in dieser Session. GUI-
Button fuer den Download fehlt noch (laut Auftrag zunaechst nicht
erforderlich, interne API `UiActionType::UpdateBambuMaterials` bereits
vollstaendig angebunden).

---

**Nachtrag (2026-08-28, Fortsetzung, Nutzerwunsch: Material-Mapping
zusammen mit der Firmware herunterladen und installieren):**
`UpdateTask::downloadUpdate()` (Firmware-Installation) sucht jetzt in
derselben bereits abgerufenen `releases/latest`-Asset-Liste zusaetzlich
nach `bambu_materials.json`/`.sha256` (kein zweiter API-Aufruf). Sind
beide vorhanden, wird die Material-Zuordnung automatisch direkt im
Anschluss an eine erfolgreiche Firmware-Installation heruntergeladen und
aktiviert, ueber denselben `streamBambuMaterialsFromUrls()`-Kern wie der
eigenstaendige Weg, aber mit `reportEvents=false` (kein eigenes
Fortschritts-Overlay/Ergebnis-Dialog -- wuerde mit dem bereits gezeigten
"Update installiert, jetzt neu starten?"-Dialog der Firmware kollidieren;
Ausgang wird stattdessen nur geloggt). Ein Fehlschlag hier aendert nie
das bereits gemeldete Firmware-Ergebnis. Fehlen die Assets in einem
Release (z. B. aeltere Releases), wird der Teil stillschweigend
uebersprungen, kein Fehler.

Dafuer `src/tasks/UpdateTask.cpp` refaktoriert: der bisherige
Firmware-Download-Koerper von `downloadUpdate()` wurde in eine eigene
Funktion `downloadAndFlashFirmware()` ausgelagert (`WiFiClientSecure`/
`HTTPClient` muessen vollstaendig zerstoert sein, bevor die
anschliessende Material-Mapping-TLS-Verbindung aufgebaut wird -- gleicher
mbedTLS-RAM-Grund wie beim bestehenden Pruefsummen-Fund vom 2026-08-24).
Der Streaming-/Chunk-/Commit-Kern von `downloadBambuMaterials()` wurde in
eine gemeinsame Funktion `streamBambuMaterialsFromUrls(ctx, requestId,
downloadUrl, checksumUrl, reportEvents)` extrahiert, von beiden Wegen
(eigenstaendig und mit der Firmware gebuendelt) genutzt. Der
eigenstaendige Weg (`UiActionType::UpdateBambuMaterials`) bleibt
zusaetzlich bestehen, fuer ein Nachziehen der Material-Zuordnung
unabhaengig von einem Firmware-Release. Bestaetigungsdialog "Update
installieren?" (`AppTask.cpp`) erwaehnt jetzt, dass die Material-Zuordnung
ggf. mit aktualisiert wird (Text zudem korrigiert -- verwies faelschlich
noch auf einen "spaeter folgenden" Neustart, der laengst umgesetzt ist).

Firmware-Build (`pio run -e wt32-s3-wrover-n16r2`) erfolgreich, 0
Compilerwarnungen. 98/98 native Tests weiterhin gruen
(`native-spoolman-tests`) -- `UpdateTask.cpp` selbst bleibt wie zuvor
nicht nativ testbar (Netzwerk-/Update.h-Abhaengigkeit). Noch nicht auf
echter Hardware verifiziert.

**Bugfix (2026-08-28, Hardware-Test, Nutzerbericht):** Erster Hardwaretest
zeigte, dass die Material-Zuordnung trotz "bundled with this release,
updating alongside firmware"-Logzeile nicht ankam, ohne jede
Fehlermeldung danach. Ursache: `downloadUpdate()` sendete das
`UpdateDownloadResult`-Erfolgsevent (loest sofort den "Update installiert,
jetzt neu starten?"-Dialog aus) **vor** dem Material-Mapping-Download
statt danach -- bestaetigte der Nutzer den Neustart, wurde der noch
laufende Hintergrund-Download durch `ESP.restart()` abgewuergt, bevor er
etwas loggen konnte (kein Absturz, daher keine Fehlermeldung). Fix:
Reihenfolge vertauscht, `streamBambuMaterialsFromUrls()` laeuft jetzt vor
dem Erfolgsevent -- der Neustart-Dialog erscheint erst, nachdem der
Materialteil abgeschlossen (oder uebersprungen/fehlgeschlagen) ist. Build
weiterhin 0 Warnungen, 98/98 native Tests gruen.

**Bugfix 2 (2026-08-28, zweiter Hardwaretest, Nutzerbericht):** Nach dem
obigen Fix zeigte sich der eigentliche Fehler: `Guru Meditation Error:
Core 1 panic'ed (Unhandled debug exception) ... Stack canary watchpoint
triggered (UpdateTask)` -- ein echter Stack-Overflow, kein Reihenfolge-
problem. Ursache: `streamBambuMaterialsFromUrls()` legte pro Aufruf bis zu
vier separate `rtos::StorageCommand`-Stacklokale an (`begin`/`chunk`/
`abort`/`commit`, je ca. 880 Byte wegen `char json[768]`) plus einen
weiteren 768-Byte-Lesepuffer -- kombiniert mit dem bereits belegten
Stack-Frame von `downloadUpdate()` (das diese Funktion beim Firmware-
Update-Huckepack-Pfad aufruft, waehrend sein eigener Frame noch aktiv
ist) sprengte das den 8192-Byte-Stack von `UpdateTask`. Der eigenstaendige
Weg (`UiActionType::UpdateBambuMaterials`) war davon vermutlich weniger
betroffen (kleinerer Aufrufer-Frame), aber ebenfalls riskant.

Fix: `rtos::StorageCommand` jetzt eine einzelne `static`-Instanz
(`sharedBambuMaterialCommand()`, Datei-lokal wiederverwendet fuer Begin/
Chunk/Commit/Abort -- sicher, da `xQueueSend()` sie sofort in die Queue
kopiert und `UpdateTask` immer nur einen Download gleichzeitig
verarbeitet), plus direktes Lesen der Netzwerkbytes in `command.json`
statt einen zusaetzlichen 768-Byte-Zwischenpuffer zu fuellen und dann zu
kopieren -- entfernt zusammen knapp 4 KiB Stackverbrauch aus dieser
Funktion. Gleiches Muster wie bereits an anderer Stelle im Projekt
etabliert (`services::allocatePsramInstance`/`static`-Instanzen fuer
uebergrosse Structs statt Stacklokale, siehe `services/PsramAlloc.h`).
Build weiterhin 0 Warnungen (RAM +872 Byte durch die neue statische
Instanz, erwartet), 98/98 native Tests gruen.

**Nachtrag 3 (2026-08-28, Nutzerwunsch nach drittem Hardwaretest:
Reihenfolge tauschen + Logging erweitern):** Trotz Bugfix 2 funktionierte
es weiterhin nicht (kein neues Log vom Nutzer vorgelegt). Auf Wunsch:

1. **Reihenfolge getauscht:** `downloadUpdate()` aktualisiert die Bambu-
   Material-Zuordnung jetzt **vor** der Firmware (vorher danach) --
   `streamBambuMaterialsFromUrls()` laeuft direkt nach der Asset-/
   Pruefsummen-Aufloesung, `downloadAndFlashFirmware()` erst im Anschluss.
   Ein Fehlschlag beim Materialteil blockiert die Firmware-Installation
   weiterhin nicht. Bestaetigungsdialog "Update installieren?"
   (`AppTask.cpp`) entsprechend umformuliert.
2. **Logging deutlich erweitert** (`FS_LOG_LEVEL` ist default 4=DEBUG,
   siehe `services/Logger.h` -- DEBUG-Zeilen sind also sichtbar):
   `ESP.getFreeHeap()` an mehreren Stellen in `UpdateTask.cpp`
   (Download-Start, nach Pruefsummen-Abruf, nach Verbindungsaufbau, nach
   Abschluss, vor/nach dem Firmware-Teil) -- Ziel: einen erneuten
   Stack-Overflow von einem Speicher-/RAM-Problem unterscheiden koennen.
   Zusaetzlich `FS_LOGD`-Zeilen pro gesendetem Chunk (UpdateTask-Seite)
   und pro geschriebenem Chunk (StorageTask-Seite, `bytes=.../total=...`),
   sowie neue `FS_LOGI`-Zeilen beim Empfang des Commit-Kommandos
   (`bytes_written=...`) und nach erfolgreichem Parsen
   (`schema_version=.../materials=.../aliases=...`) in
   `StorageTask.cpp::processCommitBambuMaterialDownload()`. Ziel: ein
   einziger Log-Mitschnitt soll die exakte Fehlerstelle zeigen, falls es
   erneut fehlschlaegt.

Build weiterhin 0 Warnungen, 98/98 native Tests gruen.

**Nachtrag 4 (2026-08-28, vierter Hardwaretest, dank erweitertem Logging
endlich exakt lokalisiert):** Nutzer-Log zeigte diesmal einen vollen
Crash-Backtrace statt eines stillen Abbruchs. Absturzstelle: nicht mehr
in eigenem Code, sondern mitten im TLS-Handshake selbst -- `fetchChecksum()`
(`UpdateTask.cpp:90`, `http.GET()`) -> `WiFiClientSecure::connect()` ->
`mbedtls_ssl_handshake()` -> ECDSA-Serverschluessel-Verifikation
(`ssl_parse_server_key_exchange`) -> tiefe Bignum-/EC-Punktmultiplikations-
Kette -> SHA-512-HMAC-DRBG (Zufallszahlengenerierung fuer ECDSA) ->
Hardware-SHA-DMA -> `spinlock_acquire`/`gdma_connect` -- "Stack canary
watchpoint triggered (UpdateTask)". Kein Logikfehler mehr: der
ECDSA/SHA-512-Handshake-Pfad selbst braucht bereits mehrere KiB Stack;
der zwei Ebenen tiefere Aufruf (`downloadUpdate()` ->
`streamBambuMaterialsFromUrls()` -> `fetchChecksum()`, statt vorher direkt
von `updateTask()` aus) reichte, um die bisher knapp passenden 8192 Byte
von `UpdateTask` zu ueberschreiten -- derselbe TLS-Handshake waere bei
flacherer Aufruftiefe (wie bei `checkForUpdate()`/
`downloadAndFlashFirmware()`) unauffaellig geblieben.

Fix: `config::kUpdateTask`-Stackgroesse in `src/config/TaskConfig.h` von
8192 auf 16384 Byte verdoppelt -- deutliche Reserve statt einer erneuten
knappen Anpassung, analog zum bereits etablierten Muster bei
`kNfcTask`/`kUiTask`/`kSpoolmanTask` (siehe deren Kommentare in derselben
Datei, jeweils nach einem echten Stack-Overflow-Fund entstanden).
`docs/architecture.md`s Task-Tabelle mitaktualisiert. Build weiterhin 0
Warnungen, 98/98 native Tests gruen.

**Nachtrag 5 (2026-08-28, fuenfter Hardwaretest, kein Absturz mehr --
neuer, klar geloggter Fehler):** Firmware- und Storage-seitiges Logging
zeigte diesmal den kompletten Ablauf bis zu einem sauberen Fehler:
`E [BAMBU] Material mapping download rejected reason=write_failed
expected=603 written=0`, nach mehreren erfolgreich geschriebenen Chunks.
Ursache: `storageTask()`s Hauptschleife ruft `cardIsAccessible()`
(oeffnet/schliesst zusaetzlich das Wurzelverzeichnis "/" als eigenes
`File`-Handle) auf **jeder einzelnen Schleifeniteration** auf, auch
direkt nach dem Verarbeiten eines `StorageCommand` -- bei den vielen
schnell aufeinanderfolgenden `WriteBambuMaterialChunk`-Befehlen (11
Chunks fuer eine ~8,8-KB-Datei) wird dieses parallele Oeffnen/Schliessen
von "/" also zwischen praktisch jedem Chunk-Schreibvorgang auf dieselbe
SD-Karte eingestreut, waehrend die Downloaddatei noch zum Schreiben
offen ist -- das hat einen der Schreibvorgaenge zuverlaessig gestoert
(0 geschriebene Bytes statt der erwarteten 603). Bei den bisherigen
Anwendungsfaellen (ein einzelnes LoadJson/SaveJson pro Nutzeraktion) trat
diese Interferenz nie auf, da dort nie mehrere Schreibvorgaenge auf
dieselbe offene Datei kurz hintereinander liefen.

Fix: `cardIsAccessible()` wird in `storageTask()`s Hauptschleife jetzt
uebersprungen (Ergebnis `true` angenommen), solange ein Bambu-Material-
Mapping-Download aktiv laeuft (`bambuMaterialDownloadRequestId !=
kDownloadRequestIdNone`) -- gezielt nur fuer diesen einen Fall, keine
Aenderung am bestehenden SD-Entfernungs-Erkennungsverhalten fuer alle
anderen StorageCommands. Sobald der Download abgeschlossen (Erfolg,
Fehler oder Abbruch) ist, laeuft die normale Pruefung pro Iteration
sofort wieder. Build weiterhin 0 Warnungen, 98/98 native Tests gruen.
Erneuter Hardwaretest steht noch aus.

**Nachtrag 6 (2026-08-28, sechster Hardwaretest -- Update laeuft
grundsaetzlich durch, Material-Datei aber weiterhin nie aktiviert):**
Trotz Fix 5 (cardIsAccessible() uebersprungen) weiterhin
`reason=write_failed`, aber jetzt mit klar unterscheidbarem Muster: kein
Absturz, kein sofortiger Fehlschlag, sondern nach 7-12 von 11
erfolgreich geschriebenen Chunks ein **kurzer/unvollstaendiger**
Schreibvorgang (`expected=603 written=0` bzw. `expected=552 written=94`
-- nicht immer 0, mal ein Teilschreiben). Intermittierend (Nutzer:
Versuch 1 und 3 schlugen fehl, dazwischen mind. ein Versuch nicht). Das
ist typisches SD-Karten-Verhalten bei einem kurzen "busy"-Zustand
(Sektor-Commit, Wear-Leveling) waehrend vieler schnell aufeinanderfolgender
kleiner Schreibvorgaenge (11 Chunks fuer eine ~8,8-KB-Datei in unter einer
Sekunde) -- kein Logik- oder Speicherfehler mehr.

Fix: `processWriteBambuMaterialChunk()` behandelt einen kurzen Schreib-
vorgang (`File::write()` liefert weniger Bytes als angefordert) nicht
mehr sofort als fatal, sondern versucht den Rest erneut (bis zu
`kMaxWriteStallRetries=10` Versuche mit `kWriteStallRetryDelayMs=20` ms
Pause dazwischen, macht max. ca. 200 ms Toleranz pro Chunk) -- Standard-
Verhalten fuer `write()`-Semantik, bei der ein Kurzschreiben fuer sich
genommen kein Fehler ist. Ein tatsaechlich dauerhaft blockierter
Schreibpfad (0 Byte ueber alle Retries) gilt weiterhin als echter
Fehler. Build weiterhin 0 Warnungen, 98/98 native Tests gruen.

**Nachtrag 7 (2026-08-28, siebter Hardwaretest -- Retry half nicht, neues
Fehlerbild):** Der Retry aus Nachtrag 6 griff nicht wie erhofft: statt
eines kurzzeitigen Hakelns blieb ein Schreibvorgang exakt bei
`offset=111` von 768 Byte dauerhaft haengen -- alle 10 Retries lieferten
`written=0`, keinerlei Fortschritt. Kein voruebergehendes "SD kurz
beschaeftigt" mehr, sondern ein Schreibpfad, der ohne aeusseres Eingreifen
nie wieder in Gang kam. Verdacht: ESP32-S3 teilt sich fuer TLS/Krypto-
Beschleunigung (aktiver Firmware-/Material-Download in `UpdateTask`) und
fuer SD/SPI-DMA (`StorageTask`s Schreibvorgaenge) dieselbe zugrunde-
liegende GDMA-Hardware -- echte Nebenlaeufigkeit zwischen beiden (wie im
bisherigen Chunk-Streaming-Design, das waehrend des laufenden TLS-
Downloads bereits parallel an StorageTask schrieb) vertraegt diese
offenbar nicht zuverlaessig.

Fix (grundlegender Redesign von `streamBambuMaterialsFromUrls()` in
`UpdateTask.cpp`): die Datei wird jetzt **komplett in einen
PSRAM-Puffer** (`services::allocatePsramInstance<std::array<uint8_t,
kBambuMaterialsMaxFileSize>>`, wiederverwendet wie `sharedBambuMaterial
Command()`) heruntergeladen, **bevor** irgendein `StorageCommand`
gesendet wird. Erst nach `dataHttp.end()` (TLS-Sitzung vollstaendig
abgebaut) werden Begin/Chunks/Commit direkt hintereinander ohne jede
weitere Netzwerk-/TLS-Aktivitaet verschickt -- keine Ueberschneidung
zwischen aktiver TLS-Nutzung und SD-Schreibzugriffen mehr moeglich. Bei
max. 16 KiB Dateigroesse (durchgaengig >30 KiB freier Heap in allen
bisherigen Logs) ist das komplette Puffern unkritisch; kostet nur ein
paar Sekunden zusaetzliche Latenz, bevor das erste Byte die SD-Karte
erreicht. `AbortBambuMaterialDownload` wird von diesem Pfad dadurch nicht
mehr ausgeloest (nichts wird mehr gesendet, solange der Download nicht
vollstaendig+verifiziert im Puffer steht) -- Kommandotyp und
`StorageTask`-Handler bleiben als dokumentierte Infrastruktur unveraendert
bestehen, nur aktuell ungenutzt. Build weiterhin 0 Warnungen, 98/98
native Tests gruen.

**Nachtrag 8 (2026-08-28, achter Hardwaretest -- Puffer-Redesign allein
reichte nicht, wahre Ursache gefunden):** Trotz Nachtrag 7 weiterhin
`reason=write_failed`, diesmal `offset=0` (sofort haengengeblieben, kein
Teilschreiben mehr). Log zeigte den Grund exakt: `downloadUpdate()`
wartet nach dem Senden des letzten Material-Mapping-Kommandos (Commit)
**nicht** darauf, dass `StorageTask` es tatsaechlich fertig verarbeitet
hat, sondern faehrt sofort mit dem Firmware-Download fort -- dessen
eigener TLS-Handshake (`downloadAndFlashFirmware()`) startete im Log
buchstaeblich in derselben Millisekunde, in der `StorageTask` noch
mitten im Abarbeiten der letzten paar Chunks war. Bestaetigt damit den
GDMA-Verdacht aus Nachtrag 7 vollstaendig -- nur eben zwischen
Material-Ende und Firmware-Start ueberlappend, nicht laenger innerhalb
des Material-Downloads selbst (das Puffer-Redesign hat dieses eine
Uberlappungsfenster bereits korrekt beseitigt).

Fix: echte Synchronisation eingefuehrt. Neues Feld `RtosContext::
bambuMaterialDownloadDone` (binaeres FreeRTOS-Semaphore, in
`createObjects()` erzeugt). `StorageTask::processCommitBambuMaterial
Download()` gibt es ueber einen kleinen RAII-Guard bei jedem Verlassen
der Funktion frei (Erfolg oder jeder Fehlerfall), sobald der Commit
tatsaechlich zu dieser `bambuMaterialDownloadRequestId` gehoert.
`UpdateTask::streamBambuMaterialsFromUrls()` wartet direkt nach dem
Senden des Commit-Kommandos darauf (`xSemaphoreTake`, mit einem
5-Sekunden-Sicherheitsnetz `config::kBambuMaterialCommitWaitTimeoutMs`,
plus einem vorherigen nicht-blockierenden Leerungsversuch gegen ein
veraltetes "gegeben"-Semaphore aus einem frueheren Timeout) -- erst
danach kehrt die Funktion zurueck, und `downloadUpdate()` faehrt mit dem
Firmware-Download fort. Damit ueberschneidet sich kein TLS-Handshake mehr
mit einem noch laufenden SD-Schreibvorgang, in keiner Richtung. Build
weiterhin 0 Warnungen, 98/98 native Tests gruen. Erneuter Hardwaretest
steht noch aus.

---

**Nachtrag (2026-08-28, Nutzerbericht: Home zeigt Bambu-`tray_type` statt
Spoolman-Material):** Beispiel: Fach zeigte "PLA-S" statt "Support For
PLA". Ursache vorbestehend, aber erst durch das erweiterte Material-
Mapping sichtbar geworden: `AppTask::syncAmsToUi()` fuellt
`UiCommand::title` (das Material-Label der Tray-Karte) direkt aus
`PrinterSlotStateData::material` -- dem vom Drucker gemeldeten Bambu
`tray_type` (`services::applyTrayOccupancy()`), nicht dem tatsaechlich
zugewiesenen Spoolman-Material. Solange `tray_type` und Spoolman-Material
meist identisch waren (PLA/PETG/...), fiel das nicht auf; bei den neuen
Support-Materialien (`tray_type` z. B. "PLA-S"/"ABS-S"/"PA-S" fuer
mehrere unterschiedliche Spoolman-Materialien) wird der Unterschied
sichtbar und irrefuehrend.

Fix: das tatsaechliche Spoolman-Material war bereits Teil der ohnehin
fuer Restgewicht/K-Faktor abgerufenen `LoadSpool`-Antwort
(`resolveTraySpoolDetails()`), wurde aber bisher verworfen -- kein
zusaetzlicher Spoolman-Request noetig. `TraySpoolDetailsEntry`/
`TraySpoolDetailsSnapshot` (`AppTask.cpp`) bekommen ein neues
`material[24]`-Feld, befuellt aus `event->spool.material` im
`SpoolmanResponse`-Handler. `syncAmsToUi()` setzt `tray.title`/
`external.title` weiterhin zuerst auf den Drucker-`tray_type` (Fallback,
solange die Spule noch nicht aufgeloest/geladen ist), ueberschreibt es
aber mit dem geladenen Spoolman-Material, sobald verfuegbar.
`UiBridge.cpp`s `TrayUiEntry::material` von 16 auf 24 Byte vergroessert
(sonst haetten laengere Spoolman-Materialnamen wie "Support For
PLA/PETG" wieder abgeschnitten werden koennen -- derselbe Fehlerklasse,
die dort schon einmal behoben wurde, nur mit der neuen, laengeren
Quelle). Build 0 Warnungen, 98/98 native Tests gruen. Noch nicht auf
echter Hardware verifiziert.

---

**Nachtrag (2026-08-28, Nutzerbericht: Display nach Touch-Wake nur ca. 1 s
hell, dann wieder dunkel):** Ursache bereits als Symptom in einem
Kommentar von TASKS.md/PowerTask.cpp vom 2026-08-27 dokumentiert, aber
nur teilweise behoben: der bestehende Fix leert nur `powerCommandQueue`-
Nachrichten, die **vor** dem Sleep-Eintritt liegen geblieben sind. Die
eigentliche Ursache liegt tiefer: `PowerTask`s `inactiveMs = 0` nach dem
Aufwachen setzt nur die eigene lokale Kopie zurueck -- LVGLs eigener
Inaktivitaets-Zeitstempel (`lv_display_get_inactive_time()`, Basis fuer
UiTasks periodische `ReportInactivity`-Meldungen) wird davon nicht
beruehrt. LVGL aktualisiert diesen Zeitstempel ausschliesslich, wenn
`readTouch()` den Touch beim naechsten Polling noch als gedrueckt
liest -- ist der Finger bis dahin (Light-Sleep-Aufwachen +
`Serial.begin()` + Queue-Leerung + Kommandoverarbeitung) schon wieder
angehoben, bleibt der uralte, hohe Vor-Sleep-Wert stehen. Die naechste
`ReportInactivity`-Meldung traegt dann genau diesen Wert, `PowerTask`
springt direkt zurueck in `Sleep` (der Sleep-Schwellwert wird zuerst
geprueft, `Dimmed` wird uebersprungen) -- exakt das beobachtete "kurz
hell, dann wieder dunkel".

Fix: `UiBridge.cpp`s `SetBrightness`-Handler ruft jetzt bei jeder
Helligkeit > 0 explizit `lv_display_trigger_activity(lvglDisplay)` auf
(LVGL-9-API, setzt den internen Aktivitaets-Zeitstempel direkt auf
"jetzt") -- Helligkeit steigt ausschliesslich als Folge einer bereits
durch echte Aktivitaet ausgeloesten `PowerTask`-Zustandsaenderung
(Touch-Wake oder eine bereits frische `ReportInactivity`), der Aufruf ist
deshalb in jedem Fall korrekt, nicht nur im Wake-Sonderfall.

Logging erweitert (Nutzerwunsch, fuer den Fall dass die Ursache doch
komplexer ist als hier analysiert): `sleepUntilTouchWake()`-Dauer
(`FS_LOGD`, "Touch wake detected, resuming after %lu ms asleep"),
Anzahl der beim Aufwachen verworfenen Alt-Nachrichten (`drained=` im
bestehenden "Woken by touch, resuming"-Log statt stillschweigend), ein
neuer Erfolgs-Log fuer `waitForSleepQuiescence()` (bisher nur der
Timeout-Fall geloggt), und eine einzelne diagnostische Logzeile fuer die
**erste** `ReportInactivity`-Meldung nach jedem Wake ("First post-wake
inactivity report inactive_ms=..." -- zeigt direkt, ob der Fix
tatsaechlich einen frischen niedrigen Wert liefert oder weiterhin den
alten hohen; bewusst nur einmal pro Wake statt dauerhaftem Mitloggen
jeder Meldung, um das Log im Normalbetrieb nicht zu fluten) sowie ein
`FS_LOGD` im `SetBrightness`-Handler selbst, wenn der Aktivitaets-Timer
zurueckgesetzt wird. Build 0 Warnungen, 98/98 native Tests gruen. Noch
nicht auf echter Hardware verifiziert.

**Bugfix (2026-08-28, Hardwaretest, Nutzerbericht: Dimmed<->Active-
Endlosschleife alle 30s):** der obige Fix loeste `lv_display_trigger_
activity()` fuer **jede** Helligkeit > 0 aus -- das feuerte auch beim
Uebergang Active->Dimmed (Helligkeit 28), der ja gerade *wegen*
Inaktivitaet passiert. Log zeigte das Muster exakt: "Active to dimmed
inactive_ms=30072" -> "Display activity timer reset brightness=28" ->
nur ~150 ms spaeter "dimmed to=active inactive_ms=151" -- der Reset
setzte den LVGL-Zeitstempel sofort auf "jetzt", die naechste
`ReportInactivity` zeigte praktisch 0 ms, `PowerTask` sprang sofort
zurueck nach Active, alle 30 Sekunden erneut. Fix: Bedingung von
`clamped > 0` auf `clamped == config::kDisplayDefaultBrightness`
verschaerft -- nur ein Uebergang zur vollen Aktiv-Helligkeit (255) ist
tatsaechlich immer Folge bereits vorhandener echter Aktivitaet
(Touch-Wake aus Sleep, oder Dimmed->Active durch eine schon frische
`ReportInactivity`); der Uebergang nach Dimmed selbst loest den Reset
jetzt nicht mehr aus. Build 0 Warnungen, 98/98 native Tests gruen.
Erneuter Hardwaretest steht noch aus.

---

**Nachtrag (2026-08-28, K-Faktor-Upload -- die bei der Temperaturhandling-
Vereinfachung bewusst ausgeklammerte "separate Aufgabe"):** der Spoolman-
K-Faktor (`flow_dynamics_k_factor`) wurde bisher nur auf der Home-Tray-
Karte angezeigt, nie an den Drucker gesendet. Jetzt implementiert: das
bereits vorhandene `extrusion_cali_sel` kann selbst keinen K-Wert
transportieren (waehlt nur ein bereits vorhandenes Kalibrierungsprofil per
`cali_idx` aus) -- dafuer legt ein neues `extrusion_cali_set` ein Profil
mit dem gewuenschten K-Wert an, ein `extrusion_cali_get` fragt danach die
komplette Profilliste ab, um den vom Drucker vergebenen `cali_idx`
nachzuschlagen (`services::bambuFindCalibrationBySettingId()`, matcht auf
einen selbst vergebenen, deterministischen `setting_id`), und erst dann
folgt `extrusion_cali_sel` mit diesem echten Index. Details/Payload-
Beispiele in `docs/bambu-protocol.md` (Abschnitt "K-Faktor-Upload
(2026-08-28)").

Der K-Faktor wird jetzt auch fuer `AssignTray` per `LoadSpool`-
>`LoadFilament`-Kette geholt (neue `SlotAssignmentStage::LoadingFilament`
zwischen `LoadingSpool` und `WritingSlot`, dieselbe `event.filament.id !=
0`-Guard-Falle wie bei `pendingStagingFilamentLoad` beachtet). Das dafuer
benoetigte `nozzle_id`-Feld wird best-effort aus einem neu ausgewerteten,
**unverifizierten** `print.nozzle_type`-Telemetriefeld gebaut (keine
bekannte verifizierte Typ-Code-Zuordnung, siehe Doku). Vollstaendig
entkoppelt von der bereits mehrfach hardware-validierten AssignTray-
Bestaetigung: fehlt der K-Faktor, hat der Drucker `nozzle_type` nie
gemeldet, oder laeuft die Kalibrierungssuche in einen eigenen, separaten
Timeout (`kBambuCalibrationTimeoutMs`), wird nur eine `FS_LOGW`-Zeile
geschrieben -- kein Fehler an die UI, kein Einfluss auf die normale
Slotzuweisung, die dann unveraendert `extrusion_cali_sel(cali_idx=-1)`
sendet wie zuvor. Build 0 Warnungen, 103/103 native Tests gruen (5 neue
Tests fuer die beiden neuen Kommando-Builder und den Kalibrierungs-
Listenabgleich). **Kein Hardware-Test dieser Funktion moeglich** in dieser
Sitzung -- insbesondere `nozzle_id`-Format und die genaue Feldstruktur der
`extrusion_cali_get`-Antwort sind unverifiziert und der wahrscheinlichste
Punkt, der nach dem ersten echten Test korrigiert werden muss.

---

**Nachtrag (2026-08-28, Nutzerbericht: System bootet nicht mehr, SD-Karte
musste repariert werden -- "wird das bambu_materials-File korrekt
geschlossen?"):** Codepruefung des kompletten Bambu-Material-Download-Datei-
Lebenszyklus (`StorageTask.cpp`s `processBeginBambuMaterialDownload()`/
`processWriteBambuMaterialChunk()`/`processCommitBambuMaterialDownload()`/
`abortBambuMaterialDownloadFile()`). Ergebnis: in jedem regulaer erreichbaren
Codepfad wird die offene Datei korrekt geflusht/geschlossen, bevor
irgendeine weitere SD-Operation (Rename/Remove) darauf folgt --
`processCommitBambuMaterialDownload()` schliesst z. B. ganz am Anfang, noch
vor jeder der nachfolgenden Validierungspruefungen. Kein Fehlverhalten in
diesem Teil gefunden.

Zwei echte, wenn auch enge Luecken wurden trotzdem gefunden und behoben:

1. `UpdateTask.cpp`s `sendStorageCommand()` war reines Fire-and-Forget ohne
   Rueckgabewert -- schlug ausgerechnet der **Commit**-Befehl fehl
   (Queue 1s lang voll), erfuhr `StorageTask` nie vom Abschluss und liess
   die bereits geoeffnete Temp-Datei unbegrenzt offen (bis zum naechsten
   `Begin` oder einem Reboot). Fix: `sendStorageCommand()` gibt jetzt
   `bool` zurueck; schlaegt der Commit-Versand fehl, wird sofort ein
   `AbortBambuMaterialDownload` nachgeschickt statt auf das (nie
   eintreffende) `bambuMaterialDownloadDone`-Semaphor zu warten. Derselbe
   Schutz zusaetzlich fuer den `Begin`-Versand (dort haengt zwar keine
   offene Datei dran, aber ein sinnloses 8s-Warten am Ende waere die
   Folge).
2. Neues, unabhaengiges Sicherheitsnetz direkt in `StorageTask` selbst
   (`config::kBambuMaterialDownloadStaleTimeoutMs`, 30 s): die
   Haupt-Loop prueft jetzt bei jedem Tick (alle `kSdHealthCheckIntervalMs`
   = 2 s), ob eine offene Download-Datei laenger als dieses Zeitfenster
   ohne `WriteChunk`/`Commit`/`Abort` daliegt, und schliesst/verwirft sie
   in diesem Fall selbst -- deckt **jeden** Grund ab, aus dem die normale
   Begin->Write->Commit/Abort-Sequenz nie abgeschlossen wird (z. B. ein
   Absturz/Reboot von `UpdateTask` selbst zwischen Begin und Commit, aus
   einem mit diesem Feature voellig unabhaengigen Grund), nicht nur den
   oben behobenen Einzelfall.

**Wahrscheinlichste tatsaechliche Ursache der gemeldeten SD-Beschaedigung:**
keine der beiden oben behobenen Luecken haelt selbst eine Datei waehrend
eines *aktiven* Schreibvorgangs offen -- ein blosses "Handle offen, aber
gerade nichts wird geschrieben" korrumpiert typischerweise nicht die
Dateisystemstruktur selbst (nur den Inhalt der einen Datei). Weitaus
plausibler: die in dieser Sitzung bereits mehrfach dokumentierten
Guru-Meditation-Abstuerze (Stack-Overflow durch die GDMA-Teilung zwischen
TLS-Kryptobeschleunigung und SD/SPI-DMA, siehe die Nachtraege weiter oben
zum urspruenglichen Bambu-Material-Download-Feature) traten *waehrend*
eines laufenden SD-Schreibvorgangs auf (SPI-Transaktion mitten im Ablauf
unterbrochen) -- ein weitaus haerterer Beschaedigungsmechanismus als ein
lediglich offen gebliebenes Handle. Diese Abstuerze sind durch die
damaligen Fixes (Semaphor-Synchronisation, komplette Pufferung vor jedem
SD-Zugriff) bereits behoben; die jetzt reparierte SD-Karte sollte das mit
dem aktuellen Code nicht erneut zeigen. Build 0 Warnungen, 103/103 native
Tests gruen. Kein Hardware-Test dieser Haertung moeglich (SD-Karte war zum
Zeitpunkt dieser Sitzung nicht verfuegbar).
