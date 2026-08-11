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
* [x] `/config/ui.json`
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

Spulenauswahl wird in späteren Workflows über wiederverwendbare Komponenten umgesetzt.

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

* [x] `SCR_SETTINGS_PRINTERS`
* [x] Druckerliste
* [x] hinzufügen
* [x] bearbeiten
* [x] löschen
* [x] aktivieren/deaktivieren
* [x] Standarddrucker
* [x] aktiver Drucker
* [x] `SCR_SETTINGS_PRINTER_EDIT`
* [x] Anzeigename
* [x] Host/IP
* [x] Seriennummer
* [x] LAN-Zugangscode
* [x] Zugangscode maskieren
* [x] Verbindung testen
* [x] Speichern
* [x] Abbrechen
* [x] kein Security Key

## 3.16 Weitere Settings-Screens

* [x] `SCR_SETTINGS_WIFI`
* [x] `SCR_SETTINGS_SCALE`
* [x] `SCR_SETTINGS_DEVICE`
* [x] `SCR_SETTINGS_DIAGNOSTICS`
* [x] `SCR_SETTINGS_FIRMWARE`

## 3.17 Dialoge und Overlays

* [x] Boot-Fortschritt
* [x] Verbindungsfortschritt
* [x] NFC-Leseoverlay
* [x] NFC-Schreiboverlay
* [x] Gewichtsstabilisierung
* [x] Spoolman-Anfrage
* [x] Bambu-Verbindung
* [x] Bestätigungsdialog
* [x] Fehlerdialog
* [x] Erfolgsdialog
* [x] Neustartbestätigung
* [x] WLAN-Resetbestätigung

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

* [x] HX711-Pins verifizieren
* [x] DOUT-Interruptfähigkeit prüfen
* [x] Interrupt registrieren
* [x] ISR mit `IRAM_ATTR`
* [x] ISR weckt ScaleTask
* [x] keine HX711-Kommunikation in ISR

## 4.2 ScaleTask

* [x] auf Notification blockieren
* [x] Messwert lesen
* [x] Verbindungsfehler
* [x] Filter aufrufen
* [x] Event an AppTask

## 4.3 Filter

* [x] gleitender Mittelwert
* [x] Tiefpassfilter
* [x] Ausreißer
* [x] negative Kleinwerte
* [x] Stabilität
* [x] Stabilitätszeit
* [x] Konfiguration zentral

## 4.4 Tarierung und Kalibrierung

* [x] Commands über Queue
* [x] Tarieren
* [x] Kalibrierung
* [x] Kalibrierfaktor
* [x] Speicherung über StorageTask
* [x] `/config/scale.json`
* [x] Laden beim Start
* [x] Zurücksetzen

## 4.5 GUI-Anbindung

* [x] WeightDisplay mit ScaleTask
* [x] stabil anzeigen
* [x] instabil anzeigen
* [x] Fehler anzeigen
* [x] Quick Weight freischalten
* [x] Advanced Weight mit realen Daten
* [x] Scale-Settings mit realen Daten
* [x] Tarierworkflow
* [x] Kalibrierworkflow

## 4.6 Quick Weight

* [x] aktuelle Spule
* [x] aktuelles Gewicht
* [x] Stabilität
* [x] berechnetes Restgewicht
* [x] letzte Messung
* [x] Bestätigung
* [x] AppTask-Aktion

## 4.7 Advanced Weight

* [x] gebrauchte Spule
* [x] volle/neue Spule
* [x] Leergewicht korrigieren
* [x] Ausgangsgewicht korrigieren
* [x] Zusammenfassung
* [x] Bestätigung
* [x] Ergebnisdialog

## 4.8 Tests

* [x] Filtertest
* [x] Stabilitätstest
* [x] Ausreißertest
* [x] Kalibrierberechnung
* [x] simulierte Interruptfolge

### Abnahmekriterien Phase 4

* ScaleTask blockiert ereignisgesteuert.
* Kein Busy Waiting.
* Kalibrierung bleibt erhalten.
* Ruhendes Gewicht stabil.
* GUI bleibt reaktionsfähig.

---

# Phase 5 – NFC/RFID und Tag-Workflows

## 5.1 PN532-Hardware

* [x] Schnittstelle festlegen
* [x] IRQ prüfen
* [x] Interrupt einrichten
* [x] ISR weckt NfcTask
* [x] keine Buskommunikation in ISR

## 5.2 Gemeinsamer Bus

* [x] Touch-/PN532-Bus prüfen
* [x] I²C-Mutex falls nötig
* [x] maximale Haltezeit
* [x] Deadlocks vermeiden

## 5.3 Grundlegendes NFC lesen und schreiben

Bereits implementierte Basisfunktionalität:

* [x] UID lesen
* [x] NDEF lesen
* [x] `spoolman:<id>` parsen
* [x] Bambu-Tag erkennen
* [x] Legacy-Tag erkennen
* [x] Tag schreiben
* [x] Tag löschen
* [x] Tag verifizieren
* [x] Entprellung

Die bestehende Funktionalität ist Ausgangspunkt für die folgenden Aufgaben und soll nicht unnötig neu geschrieben werden.

---

## 5.4 Tag-Abstraktions- und Parserarchitektur

* [x] vorhandene NfcTask-Implementierung analysieren
* [x] vorhandene Erkennung aus 5.3 weiterverwenden
* [x] `TagTechnology` definieren
* [x] `TagFormat` definieren
* [x] `TagDefinition` definieren
* [x] `TagReadResult` definieren
* [x] `RawTagData` definieren
* [x] `ITagParser` Interface definieren
* [x] `TagParserRegistry` implementieren
* [x] Parser hardwareunabhängig halten
* [x] Parser dürfen keinen SD-Zugriff durchführen
* [x] Parser dürfen keinen Spoolman-Zugriff durchführen
* [x] Parser dürfen keine GUI-Funktionen aufrufen
* [x] deterministische Parser-Reihenfolge implementieren
* [x] unbekannten Tagfall definieren
* [x] leeren NDEF-Tag erkennen
* [x] vorhandene `spoolman:<id>`-Erkennung in `FilamentStationTagParser` kapseln
* [x] vorhandene Bambu-Erkennung in neue Architektur integrieren
* [x] vorhandene Legacy-Erkennung in neue Architektur integrieren
* [x] AppEvent um klassifizierte Tagdaten erweitern
* [x] bestehende API möglichst kompatibel halten
* [x] Build durchführen
* [x] Parser-Basistests erstellen

### Abnahmekriterien 5.4

* bestehende PN532-Hardwarelogik funktioniert weiterhin
* NfcTask erkennt Technologie und logisches Format getrennt
* Native-, Bambu-, Legacy- und Unknown-Fälle laufen über gemeinsame Abstraktion
* Parser besitzen keine Netzwerk-/Storage-Abhängigkeiten
* unbekannte Tags erzeugen keine erfundenen Daten
* `pio run` erfolgreich

---

## 5.5 Native FilamentStation-Tags

Unterstützte Chips:

* NTAG213
* NTAG215
* NTAG216

NTAG215 ist der bevorzugte Standard.

* [ ] NTAG213 erkennen
* [ ] NTAG215 erkennen
* [ ] NTAG216 erkennen
* [ ] NDEF-Payload `spoolman:<id>` lesen
* [ ] Spool-ID validieren
* [ ] ungültige Payload klar ablehnen
* [ ] leeren NDEF-Tag erkennen
* [ ] native Tags schreiben
* [ ] geschriebenen Tag erneut lesen
* [ ] UID prüfen
* [ ] Payload prüfen
* [ ] Verifikation erst danach erfolgreich melden
* [ ] native Tags löschen
* [ ] Schreibfähigkeit vor Schreibvorgang prüfen
* [ ] `SCR_TAG_ACTION_SELECT` mit realer Tagklassifikation verbinden
* [ ] `SCR_TAG_REVIEW` verwenden
* [ ] `SCR_TAG_WRITE` verwenden
* [ ] `SCR_TAG_RESULT` verwenden
* [ ] vorhandene Spule über `CMP_SPOOL_PICKER` auswählen
* [ ] zuletzt verwendete Spule unterstützen
* [ ] optional Quick/Advanced Weight danach anbieten
* [ ] automatisierte Parser-Tests

### Abnahmekriterien 5.5

* NTAG213/215/216 können als native FilamentStation-Tags verwendet werden
* Payload bleibt `spoolman:<id>`
* Schreiben wird durch erneutes Lesen verifiziert
* keine Filamentstammdaten werden auf Tag dupliziert

---

## 5.6 Originale Bambu-Lab-Tags

Wichtige Regel:

> Originale Bambu-Lab-Tags sind read-only aus Sicht von FilamentStation.

* [ ] vorhandene Bambu-Erkennung aus 5.3 analysieren
* [ ] Bambu-Technologie dokumentieren
* [ ] UID sicher auslesen
* [ ] vorhandene lesbare Definition-Daten extrahieren
* [ ] öffentliche/technische Formatspezifikation dokumentieren
* [ ] keine unbekannten Datenfelder erfinden
* [ ] `BambuLabTagParser` implementieren
* [ ] Daten in `TagDefinition` überführen
* [ ] Hersteller abbilden
* [ ] Material abbilden
* [ ] Farbe abbilden
* [ ] Farbcode abbilden
* [ ] bekannte Temperaturinformationen abbilden
* [ ] bekannte Gewichtsinformationen abbilden
* [ ] lokale UID-Zuordnung prüfen
* [ ] vorhandenes Mapping direkt verwenden
* [ ] ohne Mapping Importworkflow starten
* [ ] `SCR_TAG_DEFINITION_IMPORT` verwenden
* [ ] `SCR_BAMBU_SPOOL_TYPE` verwenden, falls notwendig
* [ ] bestehende Spoolman-Spule über `CMP_SPOOL_PICKER` auswählbar machen
* [ ] UID-Mapping nach erfolgreicher Zuordnung speichern
* [ ] Bambu-Tags aus sämtlichen Schreib-/Löschaktionen ausschließen
* [ ] UI darf „Tag schreiben“ bei originalem Bambu-Tag nicht anbieten
* [ ] automatisierter Test: Bambu-Tag erzeugt niemals Write-Command

### Abnahmekriterien 5.6

* Originale Bambu-Tags werden erkannt
* Definition-Daten werden normalisiert
* UID kann Spoolman-Spule zugeordnet werden
* Tag selbst wird niemals verändert
* keine Security-Key-Funktion notwendig

---

## 5.7 OpenPrintTag – Read Support

* [ ] aktuelle OpenPrintTag-Spezifikation beziehungsweise Primärquelle recherchieren und dokumentieren
* [ ] Erkennungsmerkmale dokumentieren
* [ ] benötigte Tagtechnologien dokumentieren
* [ ] Testdaten aus belastbarer Quelle bereitstellen
* [ ] `OpenPrintTagParser` implementieren
* [ ] Hersteller parsen, falls vorhanden
* [ ] Material parsen, falls vorhanden
* [ ] Farbe parsen, falls vorhanden
* [ ] Gewichte parsen, falls vorhanden
* [ ] Temperaturen parsen, falls vorhanden
* [ ] unbekannte optionale Felder tolerieren
* [ ] Daten in `TagDefinition` überführen
* [ ] `SCR_TAG_DEFINITION_IMPORT` verwenden
* [ ] vorhandene Spoolman-Spule verbinden
* [ ] Import nach Spoolman anbieten
* [ ] lokales Mapping nur bei Bedarf erzeugen
* [ ] keine Schreibunterstützung implementieren
* [ ] keine Datenfelder erfinden
* [ ] Parser-Tests mit dokumentierten Testvektoren

### Abnahmekriterien 5.7

* bekannte OpenPrintTags werden zuverlässig erkannt
* Daten können nach Spoolman übernommen werden
* fremde Tags werden nicht fälschlich als OpenPrintTag erkannt
* Version 1 schreibt keine OpenPrintTags

---

## 5.8 OpenTag3D – Read Support

* [ ] aktuelle OpenTag3D-Spezifikation beziehungsweise Primärquelle recherchieren und dokumentieren
* [ ] Erkennungsmerkmale dokumentieren
* [ ] benötigte Tagtechnologien dokumentieren
* [ ] Testdaten bereitstellen
* [ ] `OpenTag3DParser` implementieren
* [ ] Hersteller parsen
* [ ] Material parsen
* [ ] Farbe parsen
* [ ] Gewichte parsen
* [ ] Temperaturen parsen
* [ ] vorhandene optionale Felder abbilden
* [ ] Daten in `TagDefinition` überführen
* [ ] `SCR_TAG_DEFINITION_IMPORT` verwenden
* [ ] vorhandene Spoolman-Spule verbinden
* [ ] Import nach Spoolman anbieten
* [ ] keine Schreibunterstützung implementieren
* [ ] Parser-Tests mit dokumentierten Testvektoren

### Abnahmekriterien 5.8

* bekannte OpenTag3D-Tags werden zuverlässig erkannt
* normalisierte Daten können an Spoolman weitergegeben werden
* unbekannte Felder verursachen keinen Absturz
* Version 1 schreibt keine OpenTag3D-Tags

---

## 5.9 Legacy- und unbekannte Tags

### Legacy

* [ ] vorhandene Legacy-Erkennung aus 5.3 in `LegacyTagParser` migrieren
* [ ] unterstützte Legacy-Formate explizit dokumentieren
* [ ] verschlüsselte Security-Key-Formate nicht automatisch übernehmen
* [ ] bekannte Daten anzeigen
* [ ] nach Spoolman importieren
* [ ] mit bestehender Spule verbinden
* [ ] Migration auf natives `spoolman:<id>`-Format anbieten, wenn Tag sicher beschreibbar ist
* [ ] Tag löschen nur bei eindeutig unterstütztem beschreibbarem Format
* [ ] `SCR_TAG_LEGACY` verwenden

### Unknown

* [ ] `SCR_TAG_UNKNOWN` bereitstellen beziehungsweise anbinden
* [ ] Tagtechnologie anzeigen
* [ ] UID anzeigen
* [ ] NDEF-Status anzeigen
* [ ] Schreibfähigkeit nur anzeigen, wenn sicher bekannt
* [ ] optional UID über `CMP_SPOOL_PICKER` einer Spoolman-Spule zuordnen
* [ ] unbekannten Tag nicht automatisch beschreiben
* [ ] unbekannten MIFARE-Classic-Speicher nicht verändern
* [ ] Mapping als JSON speichern

### Abnahmekriterien 5.9

* bekannte Legacy-Tags können migriert werden
* unbekannte Tags werden sicher behandelt
* UID-only-Zuordnung ist möglich
* kein unbekannter Tag wird automatisch verändert

---

## 5.10 Tagoperationen aus Staging

Abhängig vom Tagtyp und den Fähigkeiten müssen Aktionen dynamisch freigegeben werden.

* [ ] Tag schreiben für nativen beschreibbaren NTAG
* [ ] Tag neu verknüpfen
* [ ] Tag trennen
* [ ] Tag löschen, wenn sicher unterstützt
* [ ] Bambu-Tag: Schreiben deaktiviert
* [ ] Bambu-Tag: Löschen deaktiviert
* [ ] OpenPrintTag: Schreiben deaktiviert
* [ ] OpenTag3D: Schreiben deaktiviert
* [ ] Unknown: Schreiben standardmäßig deaktiviert
* [ ] Fortschritt anzeigen
* [ ] Verifikation durchführen
* [ ] destruktive Aktionen bestätigen
* [ ] Tagentfernung während Operation behandeln
* [ ] Fehlerstatus sauber an AppTask melden

---

## 5.11 Tag-Mappings

* [ ] `/mappings/nfc-spools.json`
* [ ] `/mappings/bambu-tags.json`
* [ ] `/mappings/open-tags.json`
* [ ] Mapping-Schema definieren
* [ ] UID normalisiert speichern
* [ ] Tagformat mitspeichern
* [ ] Spool-ID speichern
* [ ] Laden nur über StorageTask
* [ ] Speichern nur über StorageTask
* [ ] Mapping-Konflikte erkennen
* [ ] doppelte UID erkennen
* [ ] ungültige Spool-ID erkennen
* [ ] Mapping entfernen
* [ ] Mapping ersetzen nur nach Bestätigung
* [ ] beschädigte Mapping-Datei behandeln

### Abnahmekriterien Phase 5

* NfcTask arbeitet ereignisgesteuert.
* Bestehende Funktionalität aus 5.1–5.3 bleibt erhalten.
* NTAG213/215/216 funktionieren als native FilamentStation-Tags.
* Originale Bambu-Tags werden read-only unterstützt.
* OpenPrintTag wird gelesen und importierbar.
* OpenTag3D wird gelesen und importierbar.
* Legacy-Tags werden sicher migriert.
* Unbekannte Tags werden nicht automatisch verändert.
* Tagparser sind von Hardware, Spoolman und Storage getrennt.
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
* [ ] Ergebnisse für `CMP_SPOOL_PICKER`
* [ ] kompakte Spuleninfos für Workflows liefern

## 7.3 Hersteller und Filamente

* [ ] Hersteller suchen
* [ ] Hersteller anlegen
* [ ] Filament suchen
* [ ] Filament anlegen
* [ ] Dubletten vermeiden
* [ ] Eingaben validieren

## 7.4 Generischer TagDefinition-Import

Der Import darf nicht tagformatspezifisch im SpoolmanClient implementiert werden.

Eingabe:

```text
TagDefinition
```

* [ ] `TagDefinition` auf Spoolman-Felder abbilden
* [ ] Hersteller finden
* [ ] Material normalisieren
* [ ] Filament finden
* [ ] Farbe berücksichtigen
* [ ] Temperaturen berücksichtigen
* [ ] Gewichte berücksichtigen
* [ ] passende Datensätze vorschlagen
* [ ] fehlenden Hersteller optional anlegen
* [ ] fehlendes Filament optional anlegen
* [ ] Spule anlegen
* [ ] Spulen-ID zurückmelden
* [ ] Dublettenwarnung
* [ ] unvollständige Tagdaten in UI kennzeichnen
* [ ] Bambu-Definition importieren
* [ ] OpenPrintTag-Definition importieren
* [ ] OpenTag3D-Definition importieren
* [ ] Legacy-Definition importieren

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

* asynchrone Spoolman-Aufträge
* UI blockiert nicht
* Picker kann Spulen auswählen
* generischer Tagimport funktioniert
* Gewicht aktualisiert
* Cache als JSON

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

* mindestens zwei Drucker unterstützt
* aktiver Drucker permanent sichtbar
* Daten werden nicht vermischt
* kein Neustart beim Wechsel
* kein Security-Key-Workflow

---

# Phase 9 – Integrierte Workflows

## 9.1 Hauptworkflow

* [ ] Drucker auswählen
* [ ] AMS auswählen
* [ ] NFC/RFID-Spule erkennen
* [ ] Staging anzeigen
* [ ] Gewicht erfassen
* [ ] Spoolman aktualisieren
* [ ] Slot auswählen
* [ ] Bambu konfigurieren
* [ ] Ergebnis anzeigen

## 9.2 Native FilamentStation-Tags

* [ ] bekannten Tag erkennen
* [ ] Spoolman-ID laden
* [ ] Staging
* [ ] wiegen
* [ ] Spoolman aktualisieren
* [ ] optional AMS zuweisen

## 9.3 Bambu-Tag-Workflow

* [ ] originalen Bambu-Tag erkennen
* [ ] UID-Mapping prüfen
* [ ] Definition auslesen
* [ ] vorhandene Spule verbinden oder importieren
* [ ] Mapping speichern
* [ ] wiegen
* [ ] optional AMS zuweisen
* [ ] Tag selbst unverändert lassen

## 9.4 OpenPrintTag-Workflow

* [ ] Format erkennen
* [ ] Definition anzeigen
* [ ] Spoolman-Match
* [ ] verbinden oder importieren
* [ ] Staging
* [ ] optional wiegen

## 9.5 OpenTag3D-Workflow

* [ ] Format erkennen
* [ ] Definition anzeigen
* [ ] Spoolman-Match
* [ ] verbinden oder importieren
* [ ] Staging
* [ ] optional wiegen

## 9.6 Legacy-Workflow

* [ ] Legacy erkennen
* [ ] Daten anzeigen
* [ ] importieren
* [ ] verbinden
* [ ] optional auf natives Format migrieren
* [ ] löschen nur wenn sicher unterstützt

## 9.7 Unknown-Tag-Workflow

* [ ] Technologie anzeigen
* [ ] UID anzeigen
* [ ] NDEF-Status
* [ ] optional UID einer Spule zuordnen
* [ ] keine automatische Änderung

## 9.8 Staging-Workflow

* [ ] Quick Weight
* [ ] Advanced Weight
* [ ] Configure Slot
* [ ] Clear Staging
* [ ] Write Tag abhängig von Fähigkeiten
* [ ] Link Tag
* [ ] Unlink Tag
* [ ] Erase Tag abhängig von Fähigkeiten

## 9.9 Slot-Workflow

* [ ] Slotdetails
* [ ] Spuleninformationen
* [ ] Configure from Staging
* [ ] Configure Manually über `CMP_SPOOL_PICKER`
* [ ] Untag Slot
* [ ] Reset Slot
* [ ] Reapply Assignment
* [ ] Refresh Slot

## 9.10 Zustandsautomat

* [ ] alle Screens durch AppTask
* [ ] erlaubte Übergänge
* [ ] Zurücknavigation
* [ ] Abbruch
* [ ] Request-ID
* [ ] Printer-ID
* [ ] verspätete Antworten
* [ ] doppelte Aktionen
* [ ] Tagentfernung mitten im Workflow
* [ ] Formatwechsel nach erneutem Scan

### Abnahmekriterien Phase 9

* Workflows ohne serielle Bedienung.
* Kein Busy Waiting.
* Kommunikation nur über RTOS-Mechanismen.
* Tagfähigkeiten bestimmen verfügbare Aktionen.
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

## 10.3 NFC/RFID-Robustheit

* [ ] Tag während Lesen entfernen
* [ ] Tag während Schreiben entfernen
* [ ] zwei Tags schnell hintereinander
* [ ] unbekannter NDEF-Inhalt
* [ ] beschädigter NDEF-Inhalt
* [ ] ungültige Spoolman-ID
* [ ] Bambu-Tag darf nie beschrieben werden
* [ ] OpenPrintTag darf in V1 nicht beschrieben werden
* [ ] OpenTag3D darf in V1 nicht beschrieben werden
* [ ] unbekannter MIFARE-Tag bleibt unverändert
* [ ] Parser wirft bei unvollständigen Daten keinen Absturz
* [ ] Mapping-Konflikt
* [ ] Mapping auf nicht existierende Spule

## 10.4 Netzwerkfehler

* [ ] WLAN während HTTP trennen
* [ ] Spoolman neu starten
* [ ] langsame Antwort
* [ ] ungültige Antwort
* [ ] WiFi-Reconnect
* [ ] MQTT-Reconnect

## 10.5 Mehrdruckerfehler

* [ ] Druckerwechsel während MQTT
* [ ] Druckerwechsel während Slotupdate
* [ ] Drucker offline
* [ ] mehrere Drucker offline
* [ ] aktiven Drucker löschen
* [ ] Standarddrucker löschen
* [ ] Antwort eines alten Druckers
* [ ] AMS wird getrennt
* [ ] Slotdaten verschiedener Drucker nicht vermischen

## 10.6 Workflowfehler

* [ ] Spoolman während Tagimport aus
* [ ] NFC-Tag während Wizard entfernt
* [ ] Waage instabil
* [ ] Queue voll
* [ ] Antwort zu spät
* [ ] Benutzer bricht ab
* [ ] doppelte Messung verhindern

## 10.7 Watchdog und Langzeittest

* [ ] alle Tasks blockieren oder geben CPU frei
* [ ] keine langen kritischen Abschnitte
* [ ] keine langen Mutexhaltezeiten
* [ ] mehrstündiger Betrieb
* [ ] Speicherverbrauch
* [ ] UI-Reaktionszeit
* [ ] Dateiintegrität
* [ ] kontrollierter Neustart nur durch AppTask

### Abnahmekriterien Phase 10

* keine offensichtlichen Speicherlecks
* keine unnötig laufenden Tasks
* keine Deadlocks
* Dateiwiederherstellung funktioniert
* Mehrdruckerdaten bleiben getrennt
* unbekannte Tags bleiben unverändert

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

## 11.2 NFC/RFID-Dokumentation

* [ ] unterstützte Tagtechnologien
* [ ] unterstützte Tagformate
* [ ] FilamentStation-NDEF-Format
* [ ] NTAG215 als Empfehlung
* [ ] NTAG213-Kompatibilität
* [ ] NTAG216-Kompatibilität
* [ ] Bambu-Read-only-Regel
* [ ] Bambu-Mapping
* [ ] OpenPrintTag-Quelle/Spezifikation
* [ ] OpenTag3D-Quelle/Spezifikation
* [ ] unterstützte Legacy-Formate
* [ ] Verhalten unbekannter Tags
* [ ] Parserarchitektur
* [ ] Tag-Capability-Matrix

## 11.3 Speicher und Daten

* [ ] JSON-Schemas
* [ ] SD-Verzeichnisstruktur
* [ ] Backup-Strategie
* [ ] Cache-Strategie
* [ ] Pending-Measurement-Strategie
* [ ] Tag-Mapping-Schemas

## 11.4 Workflows

* [ ] Screenübersicht
* [ ] Navigationsdiagramm
* [ ] Hauptworkflow
* [ ] Staging-Workflow
* [ ] Slot-Workflow
* [ ] Native-Tag-Workflow
* [ ] Bambu-Tag-Workflow
* [ ] OpenPrintTag-Workflow
* [ ] OpenTag3D-Workflow
* [ ] Legacy-Workflow
* [ ] Unknown-Tag-Workflow
* [ ] Mehrdruckerworkflow

## 11.5 Bedienungsanleitungen

* [ ] Installation
* [ ] WLAN
* [ ] WiFiManager
* [ ] Spoolman
* [ ] Waagenkalibrierung
* [ ] NFC/RFID
* [ ] eigene Tags beschreiben
* [ ] Bambu-Tag importieren
* [ ] OpenPrintTag importieren
* [ ] OpenTag3D importieren
* [ ] Drucker hinzufügen
* [ ] Drucker wechseln
* [ ] AMS-Zuweisung
* [ ] Firmwareupdate

## 11.6 Entwicklerdokumentation

* [ ] Build
* [ ] Upload
* [ ] Tests
* [ ] EEZ-Studio-Export
* [ ] neuen Screen ergänzen
* [ ] neue Action ergänzen
* [ ] neuen Task ergänzen
* [ ] neues JSON-Schema ergänzen
* [ ] neuen Tagparser ergänzen

## 11.7 Lizenz und Release

* [ ] Bibliothekslizenzen
* [ ] keine unzulässig kopierten SpoolEase-Dateien
* [ ] Quellenhinweise
* [ ] Quellen für externe Tagformate
* [ ] eigene Lizenz
* [ ] Drittanbieterhinweise
* [ ] Versionsnummer
* [ ] Changelog
* [ ] Release-Build
* [ ] reproduzierbarer Build
* [ ] bekannte Einschränkungen
* [ ] bestätigen, dass kein Security-Key-Workflow existiert

### Abnahmekriterien Phase 11

* neuer Entwickler kann bauen
* Hardware kann aufgebaut werden
* Workflows sind dokumentiert
* Tagformate sind dokumentiert
* Release ist reproduzierbar
