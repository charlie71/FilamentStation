# TASKS.md – FilamentStation

## Allgemeine Regeln

* Phasen in der angegebenen Reihenfolge bearbeiten.
* Pro Codex-Auftrag nur einen klar abgegrenzten Abschnitt umsetzen.
* Keine schnelle Polling-Schleife verwenden.
* Busy Waiting ist verboten.
* Interrupts führen keine Hardwarekommunikation aus.
* Nur StorageTask greift auf SD zu.
* Nur UiTask greift auf LVGL zu.
* AppTask koordiniert die fachlichen Abläufe.
* Alle persistenten Anwendungsdateien liegen als JSON auf SD.
* WLAN-Zugangsdaten sind die systembedingte WiFiManager-Ausnahme.
* Druckerbezogene Befehle enthalten immer `printerId`.
* Codex hakt nur tatsächlich implementierte und geprüfte Aufgaben ab.

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
* [x] `BoardConfig.h` anlegen
* [x] `AppConfig.h` anlegen
* [x] `TaskConfig.h` anlegen
* [x] `Secrets.example.h` anlegen
* [x] minimale Modelle erzeugen
* [x] Message-Typen vorbereiten
* [x] noch keine unnötigen Hardwarebibliotheken einbinden

## 0.3 Minimaler Build

* [x] Startmeldung über Serial
* [x] Chipmodell ausgeben
* [x] Heap ausgeben
* [x] PSRAM ausgeben
* [x] `pio run` erfolgreich ausführen

### Abnahmekriterien Phase 0

* Projekt kompiliert.
* Keine neuen Warnungen.
* Keine Zugangsdaten enthalten.
* Keine GPIOs erfunden.
* Projektstruktur entspricht `AGENTS.md`.

---

# Phase 1 – FreeRTOS-Infrastruktur

## 1.1 RtosContext

* [x] `RtosContext` implementieren
* [x] Task-Handles zentral verwalten
* [x] Queue-Handles zentral verwalten
* [x] Event Group anlegen
* [x] notwendige Mutexes anlegen
* [x] Erzeugungsfehler behandeln

## 1.2 Nachrichtentypen

* [x] `AppEvent` definieren
* [x] `UiCommand` definieren
* [x] `UiAction` definieren
* [x] `ScaleCommand` definieren
* [x] `NfcCommand` definieren
* [x] `StorageCommand` definieren
* [x] `NetworkCommand` definieren
* [x] `SpoolmanCommand` definieren
* [x] `BambuCommand` definieren
* [x] `requestId` vorsehen
* [x] `printerId` vorsehen

## 1.3 Task-Gerüste

* [x] UiTask
* [x] AppTask
* [x] ScaleTask
* [x] NfcTask
* [x] StorageTask
* [x] NetworkTask
* [x] SpoolmanTask
* [x] BambuTask

Jeder Task muss auf Queue, Event Group oder Task Notification blockieren.

## 1.4 Task-Konfiguration

* [x] Namen zentral definieren
* [x] Stackgrößen zentral definieren
* [x] Prioritäten zentral definieren
* [x] Core-Affinitäten zentral definieren
* [x] Taskparameter dokumentieren

## 1.5 Kommunikationstest

* [x] UiTask sendet Testaktion
* [x] AppTask empfängt Testaktion
* [x] AppTask sendet UiCommand
* [x] UiTask empfängt UiCommand
* [x] Queue-Timeout behandeln
* [x] Queue-Überlauf erkennen
* [x] Kommunikation protokollieren

### Abnahmekriterien Phase 1

* Alle Tasks werden erzeugt.
* Keine schnelle Polling-Schleife.
* Kommunikation funktioniert.
* `loop()` enthält keine Anwendungslogik.
* Stackgrößen und Prioritäten dokumentiert.

---

# Phase 2 – SD-Karte und JSON-Speicherung

## 2.1 SD-Hardware

* [x] SD-Schnittstelle verifizieren
* [x] Pinbelegung dokumentieren
* [x] SD nur im StorageTask initialisieren
* [x] Card-Detect prüfen
* [x] Card-Detect-Interrupt verwenden, falls vorhanden
* [x] Entfernen und Einsetzen erkennen

## 2.2 Verzeichnisstruktur

* [x] `/config`
* [x] `/cache`
* [x] `/queue`
* [x] `/mappings`
* [x] `/diagnostics`
* [x] `/logs`

## 2.3 JsonStorage

* [x] JSON laden
* [x] JSON validieren
* [x] JSON speichern
* [x] maximale Dateigröße prüfen
* [x] Fehlercodes definieren
* [x] `schemaVersion` verarbeiten
* [x] Standardwerte einsetzen
* [x] Schema-Migration vorbereiten

## 2.4 Atomisches Speichern

* [x] `.tmp.json` schreiben
* [x] flushen
* [x] schließen
* [x] erneut validieren
* [x] bestehende Datei als `.bak.json`
* [x] temporäre Datei umbenennen
* [x] Backup entfernen
* [ ] Wiederherstellung testen

## 2.5 Storage-Queue

* [x] Leseanfragen über Queue
* [x] Schreibanfragen über Queue
* [x] Antworten über App-Queue
* [x] mehrere Anfragen geordnet verarbeiten
* [x] direkte SD-Zugriffe anderer Tasks verhindern

## 2.6 Konfigurationsdateien

* [x] `/config/device.json`
* [x] `/config/network.json`
* [x] `/config/spoolman.json`
* [x] `/config/bambu.json`
gebaut* [x] `/config/ui.json`
* [x] `/config/scale.json`
* [x] `/config/nfc.json`

### Abnahmekriterien Phase 2

* JSON-Dateien speichern und laden.
* Beschädigte Dateien erkennen.
* Backups wiederherstellen.
* Nur StorageTask greift auf SD zu.
* Fehlende SD-Karte wird korrekt gemeldet.

---

# Phase 3 – Display, Touch, LVGL und GUI

## 3.1 Hardwareprüfung

* [x] Displaycontroller verifizieren
* [x] Touchcontroller verifizieren
* [x] Pinbelegung dokumentieren
* [x] GPIO-Konflikte prüfen
* [x] Hintergrundbeleuchtung prüfen

## 3.2 LovyanGFX

* [x] Display initialisieren
* [x] Rotation konfigurieren
* [x] Farbtest
* [x] Touchkoordinaten
* [x] Touchrotation
* [x] Touchtest

## 3.3 LVGL

* [x] LVGL 9 integrieren
* [x] `lv_conf.h`
* [x] Renderpuffer
* [x] PSRAM-Nutzung
* [x] Flush-Callback
* [x] Touch-Callback
* [x] LVGL nur im UiTask

## 3.4 UiTask

* [x] `uiCommandQueue` verarbeiten
* [x] LVGL-Timing verwenden
* [x] unnötig kurze Schleifen vermeiden
* [x] optional Touch-IRQ prüfen
* [x] keine UI-Aufrufe aus anderen Tasks

## 3.5 Bestehende EEZ-GUI migrieren

Die bereits vorhandene Implementierung aus Aufgabe 3.5 ist Ausgangspunkt und muss angepasst werden.

* [x] bestehendes EEZ-Projekt sichern
* [x] vorhandene Screens analysieren
* [x] vorhandene Navigation analysieren
* [x] unpassende Placeholder-Screens entfernen
* [x] vorhandene Widgets weiterverwenden, wenn sinnvoll
* [x] kein zweites EEZ-Projekt erzeugen
* [x] Screen-Namen aus `AGENTS.md` verwenden
* [x] permanente Drucker-Kopfzeile ergänzen
* [x] aktuellen Drucker immer anzeigen
* [x] Drucker-Kopfzeile antippbar machen
* [x] Settings-Schaltfläche ergänzen
* [x] globale untere Aktionsleiste erstellen
* [x] Home auf AMS-, External- und Staging-Struktur umbauen
* [x] Security-Key-Elemente entfernen
* [x] generierten Code nur nach `src/ui/generated/`
* [x] `pio run`

### Abnahmekriterien 3.5

* Vorhandenes Projekt wurde migriert.
* Kein paralleles UI-Projekt.
* Drucker permanent sichtbar.
* Kopfzeile öffnet Druckerauswahl.
* Keine Security-Key-Elemente.
* Build erfolgreich.

## 3.6 Designsystem und Komponenten

* [x] globale Farben
* [x] globale Schriftgrößen
* [x] globale Abstände
* [x] Mindesthöhe der Touchflächen
* [x] `CMP_TOP_PRINTER_BAR`
* [x] `CMP_BOTTOM_ACTION_BAR`
* [x] `CMP_STATUS_BADGE`
* [x] `CMP_CONNECTION_INDICATOR`
* [x] `CMP_AMS_SELECTOR`
* [x] `CMP_TRAY_CARD`
* [x] `CMP_STAGING_CARD`
* [x] `CMP_SPOOL_SUMMARY`
* [x] `CMP_WEIGHT_DISPLAY`
* [x] `CMP_PROGRESS_OVERLAY`
* [x] `CMP_CONFIRM_DIALOG`
* [x] `CMP_RESULT_DIALOG`
* [x] `CMP_ERROR_DIALOG`
* [x] `CMP_NUMERIC_INPUT`
* [x] `CMP_TEXT_INPUT`
* [x] `CMP_SETTINGS_BUTTON`

## 3.7 UI-Datenmodelle

* [x] `UiPrinterSummary`
* [x] `UiAmsSummary`
* [x] `UiTraySummary`
* [x] `UiStagingSummary`
* [x] `UiSpoolSummary`
* [x] `UiWeightState`
* [x] `UiConnectionState`
* [x] `UiSettingsState`
* [x] mehrere Drucker berücksichtigen
* [x] `printerId` in Aktionen
* [x] `amsId` in Aktionen
* [x] `trayId` in Aktionen
* [x] `spoolId` in Aktionen
* [x] Mock-Datenprovider

## 3.8 Home-Screen

* [x] `SCR_HOME`
* [x] Drucker-Kopfzeile
* [x] Druckerstatus
* [x] aktives AMS
* [x] mehrere AMS-Einheiten
* [x] vier Slots
* [x] External Slot
* [x] Staging
* [x] Gewicht
* [x] Stabilitätsstatus
* [x] NFC-Status
* [x] Spoolman-Status
* [x] WLAN-Status
* [x] Slot-Aktionen senden
* [x] Staging-Aktion senden
* [x] Druckerauswahl senden
* [x] Settings öffnen

## 3.9 Druckerauswahl

* [x] `SCR_PRINTER_SELECT`
* [x] Druckerliste
* [x] aktueller Drucker markiert
* [x] Standarddrucker markiert
* [x] Online-/Offline-Status
* [x] AMS-Anzahl
* [x] Druckerwechsel als UiAction
* [x] Rückkehr zur vorherigen Ansicht
* [x] Drucker verwalten

## 3.10 Staging-Screens

* [x] `SCR_STAGING_DETAILS`
* [x] Spoolman-ID
* [x] Hersteller
* [x] Material
* [x] Farbe
* [x] Leergewicht
* [x] Bruttogewicht
* [x] Restgewicht
* [x] NFC-Status
* [x] Quick Weight
* [x] Mehr
* [x] Schließen
* [x] `SCR_STAGING_ACTIONS`
* [x] Slot konfigurieren
* [x] Advanced Weight
* [x] Staging leeren
* [x] Tag schreiben
* [x] Tag verknüpfen
* [x] Tag trennen
* [x] Tag löschen
* [x] Spule suchen
* [x] Spulendetails

## 3.11 Slot-Screens

* [x] `SCR_TRAY_DETAILS`
* [x] Tab Slotinformationen
* [x] Tab Spuleninformationen
* [x] `SCR_TRAY_ACTIONS`
* [x] aus Staging konfigurieren
* [x] manuell konfigurieren
* [x] Zuordnung entfernen
* [x] Slot zurücksetzen
* [x] Zuordnung erneut anwenden
* [x] Slot aktualisieren
* [x] `SCR_TRAY_SELECT`
* [x] Druckerwechsel im Auswahlmodus
* [x] AMS-Wechsel im Auswahlmodus
* [x] Slot hervorheben
* [x] Zusammenfassung nach Auswahl

## 3.12 Spulen-Screens – entfällt

Separate Spulensuche und Spulendetailansicht werden nicht benötigt.

## 3.13 Settings-Grundstruktur

* [x] `SCR_SETTINGS_HOME`
* [x] WLAN
* [x] Spoolman
* [x] Waage
* [x] Bambu-Drucker
* [x] Gerät
* [x] Diagnose
* [x] Firmware
* [x] keine Security-Key-Kategorie
* [x] Navigation
* [x] Zurücknavigation

## 3.14 Spoolman-Settings

* [x] `SCR_SETTINGS_SPOOLMAN`
* [x] Verbindungsname
* [x] HTTP/HTTPS
* [x] Host/IP
* [x] Port
* [x] API-Basispfad
* [x] Timeout
* [x] Verbindung testen
* [x] Status
* [x] Serverversion
* [x] Speichern
* [x] Abbrechen
* [x] Eingabevalidierung
* [x] kein Security-Key-Feld

## 3.15 Druckerverwaltung

* [ ] `SCR_SETTINGS_PRINTERS`
* [ ] Druckerliste
* [ ] hinzufügen
* [ ] bearbeiten
* [ ] löschen
* [ ] aktivieren/deaktivieren
* [ ] Standarddrucker
* [ ] aktiver Drucker
* [ ] `SCR_SETTINGS_PRINTER_EDIT`
* [ ] Anzeigename
* [ ] Host/IP
* [ ] Seriennummer
* [ ] LAN-Zugangscode
* [ ] Zugangscode maskieren
* [ ] Verbindung testen
* [ ] Speichern
* [ ] Abbrechen
* [ ] kein Security Key

## 3.16 Weitere Settings-Screens

* [ ] `SCR_SETTINGS_WIFI`
* [ ] `SCR_SETTINGS_SCALE`
* [ ] `SCR_SETTINGS_DEVICE`
* [ ] `SCR_SETTINGS_DIAGNOSTICS`
* [ ] `SCR_SETTINGS_FIRMWARE`

## 3.17 Dialoge und Overlays

* [ ] Boot-Fortschritt
* [ ] Verbindungsfortschritt
* [ ] NFC-Leseoverlay
* [ ] NFC-Schreiboverlay
* [ ] Gewichtsstabilisierung
* [ ] Spoolman-Anfrage
* [ ] Bambu-Verbindung
* [ ] Bestätigungsdialog
* [ ] Fehlerdialog
* [ ] Erfolgsdialog
* [ ] Neustartbestätigung
* [ ] WLAN-Resetbestätigung

### Abnahmekriterien Phase 3

* Alle Screenskelett vorhanden.
* Navigation mit Mock-Daten möglich.
* Mehrdruckerunterstützung sichtbar.
* Spoolman eigener Settings-Screen.
* Keine Security-Key-Funktion.
* Build erfolgreich.

---

# Phase 4 – Waage und Gewicht

## 4.1 HX711-Hardware

* [ ] HX711-Pins verifizieren
* [ ] DOUT-Interruptfähigkeit prüfen
* [ ] Interrupt registrieren
* [ ] ISR mit `IRAM_ATTR`
* [ ] ISR weckt ScaleTask
* [ ] keine HX711-Kommunikation in ISR

## 4.2 ScaleTask

* [ ] auf Notification blockieren
* [ ] Messwert lesen
* [ ] Verbindungsfehler
* [ ] Filter aufrufen
* [ ] Event an AppTask

## 4.3 Filter

* [ ] gleitender Mittelwert
* [ ] Tiefpassfilter
* [ ] Ausreißer
* [ ] negative Kleinwerte
* [ ] Stabilität
* [ ] Stabilitätszeit
* [ ] Konfiguration zentral

## 4.4 Tarierung und Kalibrierung

* [ ] Commands über Queue
* [ ] Tarieren
* [ ] Kalibrierung
* [ ] Kalibrierfaktor
* [ ] Speicherung über StorageTask
* [ ] `/config/scale.json`
* [ ] Laden beim Start
* [ ] Zurücksetzen

## 4.5 GUI-Anbindung

* [ ] WeightDisplay mit ScaleTask
* [ ] stabil anzeigen
* [ ] instabil anzeigen
* [ ] Fehler anzeigen
* [ ] Quick Weight freischalten
* [ ] Advanced Weight mit realen Daten
* [ ] Scale-Settings mit realen Daten
* [ ] Tarierworkflow
* [ ] Kalibrierworkflow

## 4.6 Quick Weight

* [ ] aktuelle Spule
* [ ] aktuelles Gewicht
* [ ] Stabilität
* [ ] berechnetes Restgewicht
* [ ] letzte Messung
* [ ] Bestätigung
* [ ] AppTask-Aktion

## 4.7 Advanced Weight

* [ ] gebrauchte Spule
* [ ] volle/neue Spule
* [ ] Leergewicht korrigieren
* [ ] Ausgangsgewicht korrigieren
* [ ] Zusammenfassung
* [ ] Bestätigung
* [ ] Ergebnisdialog

## 4.8 Tests

* [ ] Filtertest
* [ ] Stabilitätstest
* [ ] Ausreißertest
* [ ] Kalibrierberechnung
* [ ] simulierte Interruptfolge

### Abnahmekriterien Phase 4

* ScaleTask blockiert ereignisgesteuert.
* Kein Busy Waiting.
* Kalibrierung bleibt erhalten.
* Ruhendes Gewicht stabil.
* GUI bleibt reaktionsfähig.

---

# Phase 5 – NFC und Tag-Workflows

## 5.1 PN532-Hardware

* [ ] Schnittstelle festlegen
* [ ] IRQ prüfen
* [ ] Interrupt einrichten
* [ ] ISR weckt NfcTask
* [ ] keine Buskommunikation in ISR

## 5.2 Gemeinsamer Bus

* [ ] Touch-/PN532-Bus prüfen
* [ ] I²C-Mutex falls nötig
* [ ] maximale Haltezeit
* [ ] Deadlocks vermeiden

## 5.3 NFC lesen und schreiben

* [ ] UID lesen
* [ ] NDEF lesen
* [ ] `spoolman:<id>` parsen
* [ ] Bambu-Tag erkennen
* [ ] Legacy-Tag erkennen
* [ ] Tag schreiben
* [ ] Tag löschen
* [ ] Tag verifizieren
* [ ] Entprellung

## 5.4 Neuer einfacher Tag

* [ ] `SCR_TAG_ACTION_SELECT`
* [ ] vorhandene Spule verbinden
* [ ] letzte Spule verbinden
* [ ] Spule suchen
* [ ] Daten prüfen
* [ ] optional wiegen
* [ ] schreiben
* [ ] verifizieren
* [ ] Ergebnis

## 5.5 Bambu-Definition-Tag

* [ ] UID lesen
* [ ] Definition anzeigen
* [ ] lokale Zuordnung prüfen
* [ ] Import anbieten
* [ ] bestehende Spule verbinden
* [ ] `SCR_BAMBU_SPOOL_TYPE`
* [ ] Low Temperature
* [ ] High Temperature
* [ ] Other
* [ ] Leergewicht prüfen
* [ ] Spoolman-Match suchen
* [ ] Importvorschau
* [ ] UID-Zuordnung speichern
* [ ] optional wiegen
* [ ] Ergebnis

## 5.6 Legacy-Tag

* [ ] altes Format erkennen
* [ ] Daten anzeigen
* [ ] importieren
* [ ] verbinden
* [ ] umschreiben
* [ ] löschen
* [ ] abbrechen

## 5.7 Tagoperationen aus Staging

* [ ] Tag schreiben
* [ ] Tag verknüpfen
* [ ] Tag trennen
* [ ] Tag löschen
* [ ] Fortschritt
* [ ] Verifikation
* [ ] Bestätigung destruktiver Aktionen

## 5.8 Mappings

* [ ] `/mappings/nfc-spools.json`
* [ ] `/mappings/bambu-tags.json`
* [ ] Laden über StorageTask
* [ ] Speichern über StorageTask
* [ ] Konflikte erkennen

### Abnahmekriterien Phase 5

* NfcTask arbeitet ereignisgesteuert.
* Tags können gelesen, geschrieben und verifiziert werden.
* Bambu- und Legacy-Workflows funktionieren.
* Keine Security-Key- oder Verschlüsselungslogik.

---

# Phase 6 – WiFiManager

## 6.1 WiFiManager

* [ ] feste Bibliotheksversion
* [ ] Instanz im NetworkTask
* [ ] Captive Portal
* [ ] AP-Passwort
* [ ] Portal-Timeout
* [ ] Verbindungs-Timeout

## 6.2 Portalbetrieb

* [ ] nur im NetworkTask
* [ ] UI bleibt aktiv
* [ ] AppTask erhält Status
* [ ] Abbruch
* [ ] Timeout
* [ ] kein `process()` in `loop()`

## 6.3 WiFi-Events

* [ ] `WiFi.onEvent()`
* [ ] Connected
* [ ] Got IP
* [ ] Disconnect
* [ ] Lost IP
* [ ] kurze Callback-Nachrichten
* [ ] Event Group aktualisieren

## 6.4 Netzwerkparameter

* [ ] `/config/network.json`
* [ ] Hostname
* [ ] DHCP/statisch
* [ ] DNS
* [ ] Portalname
* [ ] Timeouts
* [ ] Speichern über StorageTask

## 6.5 GUI

* [ ] WLAN-Status
* [ ] SSID
* [ ] IP
* [ ] RSSI
* [ ] Captive Portal starten
* [ ] WLAN neu konfigurieren
* [ ] WLAN-Daten löschen
* [ ] Portal-Anleitung
* [ ] kein Security Key

### Abnahmekriterien Phase 6

* WLAN ohne Neukompilierung.
* Portal blockiert andere Tasks nicht.
* WiFi-Callbacks bleiben kurz.
* Zusatzparameter als JSON.
* Passwort nicht auf SD dupliziert.

---

# Phase 7 – Spoolman

## 7.1 Konfiguration

* [ ] `/config/spoolman.json`
* [ ] GUI-Werte laden
* [ ] GUI-Werte speichern
* [ ] URL normalisieren
* [ ] Timeout
* [ ] Verbindung testen
* [ ] Status
* [ ] Version

## 7.2 Spulen

* [ ] Spule nach ID
* [ ] Spulen suchen
* [ ] Materialfilter
* [ ] Herstellerfilter
* [ ] Farbfilter
* [ ] Suchergebnisse an UI
* [ ] Details an UI

## 7.3 Hersteller und Filamente

* [ ] Hersteller suchen
* [ ] Hersteller anlegen
* [ ] Filament suchen
* [ ] Filament anlegen
* [ ] Dubletten vermeiden
* [ ] Eingaben validieren

## 7.4 Spulenimport

* [ ] Tagdaten abbilden
* [ ] Hersteller finden
* [ ] Filament finden
* [ ] passende Datensätze auswählen
* [ ] fehlende Datensätze anlegen
* [ ] Spule anlegen
* [ ] Spulen-ID zurückmelden

## 7.5 Gewicht

* [ ] Quick Weight übertragen
* [ ] Advanced Weight übertragen
* [ ] aktualisierte Spule neu laden
* [ ] Staging aktualisieren
* [ ] Fehlerdialog
* [ ] Pending Measurements

## 7.6 Cache

* [ ] `/cache/spools.json`
* [ ] `/cache/filaments.json`
* [ ] `/cache/vendors.json`
* [ ] Cache-Alter
* [ ] veraltete Daten markieren
* [ ] Cache nicht als führende Datenbank

### Abnahmekriterien Phase 7

* Asynchrone Spoolman-Aufträge.
* UI blockiert nicht.
* Suche funktioniert.
* Import funktioniert.
* Gewicht aktualisiert.
* Cache als JSON.

---

# Phase 8 – Bambu und mehrere Drucker

## 8.1 Datenmodell

* [ ] mehrere Drucker
* [ ] stabile PrinterId
* [ ] aktiver Drucker
* [ ] Standarddrucker
* [ ] aktives AMS je Drucker
* [ ] Cache je Drucker
* [ ] Slotzuordnung je Drucker

## 8.2 Bambu-Konfiguration

* [ ] `/config/bambu.json`
* [ ] Name
* [ ] Host/IP
* [ ] Seriennummer
* [ ] LAN-Zugangscode
* [ ] aktiviert
* [ ] Standard
* [ ] ausgewählt
* [ ] kein Security Key

## 8.3 BambuTask

* [ ] Commands mit printerId
* [ ] Events mit printerId
* [ ] Drucker aktivieren
* [ ] Drucker wechseln
* [ ] verbinden
* [ ] trennen
* [ ] Verbindung testen
* [ ] Status
* [ ] AMS-Liste
* [ ] Slots
* [ ] External Slot
* [ ] Slotdaten schreiben
* [ ] Slotdaten zurücksetzen
* [ ] Wiederverbindung

## 8.4 Druckerwechsel

* [ ] alten Zustand sichern
* [ ] aktiven Drucker ändern
* [ ] Kopfzeile aktualisieren
* [ ] AMS-Daten laden
* [ ] Staging erhalten
* [ ] veraltete Antworten ignorieren
* [ ] printerId prüfen

## 8.5 AMS-Zuordnung

* [ ] Spule aus Staging
* [ ] Drucker
* [ ] AMS
* [ ] Slot
* [ ] Daten vorbereiten
* [ ] BambuCommand
* [ ] Antwort
* [ ] Slots neu laden
* [ ] Ergebnis

## 8.6 Druckerverwaltungs-GUI

* [ ] hinzufügen
* [ ] bearbeiten
* [ ] löschen
* [ ] Standard setzen
* [ ] aktiv setzen
* [ ] Verbindung testen
* [ ] Zugangscode maskieren
* [ ] Löschbestätigung

### Abnahmekriterien Phase 8

* Mindestens zwei Drucker unterstützt.
* Aktiver Drucker permanent sichtbar.
* Daten werden nicht vermischt.
* Kein Neustart beim Wechsel.
* Kein Security-Key-Workflow.

---

# Phase 9 – Integrierte Workflows

## 9.1 Hauptworkflow

* [ ] Drucker auswählen
* [ ] AMS auswählen
* [ ] NFC-Spule erkennen
* [ ] Staging anzeigen
* [ ] Gewicht erfassen
* [ ] Spoolman aktualisieren
* [ ] Slot auswählen
* [ ] Bambu konfigurieren
* [ ] Ergebnis anzeigen

## 9.2 Staging-Workflow

* [ ] Quick Weight
* [ ] Advanced Weight
* [ ] Configure Slot
* [ ] Clear Staging
* [ ] Write Tag
* [ ] Link Tag
* [ ] Unlink Tag
* [ ] Erase Tag

## 9.3 Slot-Workflow

* [ ] Slotdetails
* [ ] Spulendetails
* [ ] Configure from Staging
* [ ] Configure Manually
* [ ] Untag Slot
* [ ] Reset Slot
* [ ] Reapply Assignment
* [ ] Refresh Slot

## 9.4 Definition-Tag-Workflow

* [ ] Tag erkennen
* [ ] Definition anzeigen
* [ ] Spulentyp auswählen
* [ ] Spoolman-Match
* [ ] Daten prüfen
* [ ] importieren
* [ ] wiegen
* [ ] zuweisen
* [ ] Ergebnis

## 9.5 Legacy-Workflow

* [ ] Legacy erkennen
* [ ] Daten anzeigen
* [ ] importieren
* [ ] verbinden
* [ ] umschreiben
* [ ] löschen

## 9.6 Zustandsautomat

* [ ] alle Screens durch AppTask
* [ ] erlaubte Übergänge
* [ ] Zurücknavigation
* [ ] Abbruch
* [ ] Request-ID
* [ ] Printer-ID
* [ ] verspätete Antworten
* [ ] doppelte Aktionen

### Abnahmekriterien Phase 9

* Workflows ohne serielle Bedienung.
* Kein Busy Waiting.
* Kommunikation nur über RTOS-Mechanismen.
* Fehler führen zu definiertem Zustand.

---

# Phase 10 – Robustheit und Diagnose

## 10.1 Task-Diagnose

* [ ] Stack High Water Marks
* [ ] Laufzeitstatistiken
* [ ] Queue-Auslastung
* [ ] Event Bits
* [ ] Heap
* [ ] PSRAM
* [ ] `/diagnostics/task-stats.json`

## 10.2 Hardware- und Speicherfehler

* [ ] SD während Schreiben entfernen
* [ ] Stromausfall simulieren
* [ ] HX711 trennen
* [ ] PN532 trennen
* [ ] langsame SD
* [ ] beschädigte JSON
* [ ] Backup-Wiederherstellung

## 10.3 Netzwerkfehler

* [ ] WLAN während HTTP trennen
* [ ] Spoolman neu starten
* [ ] langsame Antwort
* [ ] ungültige Antwort
* [ ] WiFi-Reconnect
* [ ] MQTT-Reconnect

## 10.4 Mehrdruckerfehler

* [ ] Druckerwechsel während MQTT
* [ ] Druckerwechsel während Slotupdate
* [ ] Drucker offline
* [ ] mehrere Drucker offline
* [ ] aktiven Drucker löschen
* [ ] Standarddrucker löschen
* [ ] Antwort eines alten Druckers
* [ ] AMS wird getrennt
* [ ] Slotdaten verschiedener Drucker nicht vermischen

## 10.5 Workflowfehler

* [ ] Spoolman während Tagimport aus
* [ ] NFC-Tag während Wizard entfernt
* [ ] Waage instabil
* [ ] Queue voll
* [ ] Antwort zu spät
* [ ] Benutzer bricht ab
* [ ] doppelte Messung verhindern

## 10.6 Watchdog und Langzeittest

* [ ] alle Tasks blockieren oder geben CPU frei
* [ ] keine langen kritischen Abschnitte
* [ ] keine langen Mutexhaltezeiten
* [ ] mehrstündiger Betrieb
* [ ] Speicherverbrauch
* [ ] UI-Reaktionszeit
* [ ] Dateiintegrität
* [ ] kontrollierter Neustart nur durch AppTask

### Abnahmekriterien Phase 10

* Keine offensichtlichen Speicherlecks.
* Keine unnötig laufenden Tasks.
* Keine Deadlocks.
* Dateiwiederherstellung funktioniert.
* Mehrdruckerdaten bleiben getrennt.

---

# Phase 11 – Dokumentation und Release

## 11.1 Technische Dokumentation

* [ ] Architekturdiagramm
* [ ] Taskdiagramm
* [ ] Queueübersicht
* [ ] Event-Group-Übersicht
* [ ] Interruptübersicht
* [ ] Taskprioritäten
* [ ] Stackgrößen
* [ ] GPIO-Tabelle
* [ ] Verdrahtungsplan
* [ ] Stückliste

## 11.2 Speicher und Daten

* [ ] JSON-Schemas
* [ ] SD-Verzeichnisstruktur
* [ ] Backup-Strategie
* [ ] Cache-Strategie
* [ ] Pending-Measurement-Strategie

## 11.3 Workflows

* [ ] Screenübersicht
* [ ] Navigationsdiagramm
* [ ] Hauptworkflow
* [ ] Staging-Workflow
* [ ] Slot-Workflow
* [ ] Tag-Workflow
* [ ] Bambu-Importworkflow
* [ ] Legacy-Workflow
* [ ] Mehrdruckerworkflow

## 11.4 Bedienungsanleitungen

* [ ] Installation
* [ ] WLAN
* [ ] WiFiManager
* [ ] Spoolman
* [ ] Waagenkalibrierung
* [ ] NFC
* [ ] Drucker hinzufügen
* [ ] Drucker wechseln
* [ ] AMS-Zuweisung
* [ ] Firmwareupdate

## 11.5 Entwicklerdokumentation

* [ ] Build
* [ ] Upload
* [ ] Tests
* [ ] EEZ-Studio-Export
* [ ] neuen Screen ergänzen
* [ ] neue Action ergänzen
* [ ] neuen Task ergänzen
* [ ] neues JSON-Schema ergänzen

## 11.6 Lizenz und Release

* [ ] Bibliothekslizenzen
* [ ] keine unzulässig kopierten SpoolEase-Dateien
* [ ] Quellenhinweise
* [ ] eigene Lizenz
* [ ] Drittanbieterhinweise
* [ ] Versionsnummer
* [ ] Changelog
* [ ] Release-Build
* [ ] reproduzierbarer Build
* [ ] bekannte Einschränkungen
* [ ] bestätigen, dass kein Security-Key-Workflow existiert

### Abnahmekriterien Phase 11

* Neuer Entwickler kann bauen.
* Hardware kann aufgebaut werden.
* Workflows sind dokumentiert.
* Screens sind dokumentiert.
* Release ist reproduzierbar.
