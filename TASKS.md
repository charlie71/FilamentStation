# TASKS.md – FilamentStation

## Allgemeine Regeln

* Jede Phase wird einzeln umgesetzt.
* Tasks müssen auf Ereignisse blockieren.
* Busy Waiting ist nicht zulässig.
* Interrupts führen keine Hardwarekommunikation aus.
* Nur der StorageTask greift auf die SD-Karte zu.
* Nur der UiTask greift auf LVGL zu.
* AppTask koordiniert alle fachlichen Abläufe.
* Alle persistenten Anwendungsdateien sind gültige JSON-Dateien.
* Codex hakt nur tatsächlich umgesetzte und geprüfte Aufgaben ab.

---

# Phase 0 – PlatformIO und Projektbasis

## 0.1 PlatformIO-Projekt

* [x] PlatformIO-Projekt für ESP32-S3 anlegen
* [x] Arduino Framework konfigurieren
* [x] C++17 aktivieren
* [x] seriellen Monitor mit 115200 Baud konfigurieren
* [x] Flashgröße und PSRAM vorbereiten
* [x] `.gitignore` anlegen
* [x] Build-Anleitung in `README.md` erstellen

## 0.2 Grundstruktur

* [x] Verzeichnisstruktur aus `AGENTS.md` anlegen
* [x] `BoardConfig.h` anlegen
* [x] `AppConfig.h` anlegen
* [x] `TaskConfig.h` anlegen
* [x] `Secrets.example.h` anlegen
* [x] minimale Modelle und Message-Typen erzeugen
* [x] noch keine Hardwarebibliotheken einbinden

## 0.3 Minimaler Build

* [x] Startmeldung über Serial ausgeben
* [x] Chipmodell ausgeben
* [x] Heap und PSRAM ausgeben
* [x] `pio run` erfolgreich ausführen

### Abnahmekriterien

* Projekt kompiliert ohne neue Warnungen.
* Keine Zugangsdaten sind enthalten.
* Noch keine GPIOs wurden erfunden.
* Die Grundstruktur entspricht `AGENTS.md`.

---

# Phase 1 – FreeRTOS-Infrastruktur

## 1.1 RTOS-Kontext

* [x] `RtosContext` implementieren
* [x] zentrale Handles für Queues anlegen
* [x] zentrale Handles für Tasks anlegen
* [x] System Event Group anlegen
* [x] benötigte Mutexes anlegen
* [x] Fehler bei der Erzeugung aller RTOS-Objekte behandeln

## 1.2 Nachrichtentypen

* [x] `AppEvent` definieren
* [x] `UiCommand` definieren
* [x] `ScaleCommand` definieren
* [x] `NfcCommand` definieren
* [x] `StorageCommand` definieren
* [x] `NetworkCommand` definieren
* [x] `SpoolmanCommand` definieren
* [x] `BambuCommand` vorbereiten
* [x] `requestId` für asynchrone Antworten vorsehen

## 1.3 Task-Gerüste

* [x] UiTask-Gerüst
* [x] AppTask-Gerüst
* [x] ScaleTask-Gerüst
* [x] NfcTask-Gerüst
* [x] StorageTask-Gerüst
* [x] NetworkTask-Gerüst
* [x] SpoolmanTask-Gerüst
* [x] BambuTask nur als deaktivierter Platzhalter

Jeder Task muss zunächst auf seiner Queue oder Event Group blockieren.

## 1.4 Task-Konfiguration

* [x] Namen zentral definieren
* [x] Stackgrößen zentral definieren
* [x] Prioritäten zentral definieren
* [x] Core-Affinitäten zentral definieren
* [x] keine unkommentierten Taskparameter in `main.cpp`

## 1.5 Kommunikationstest

* [x] Testereignis vom UiTask an AppTask senden
* [x] AppTask sendet Antwort an UiTask
* [x] Queue-Timeout behandeln
* [x] Queue-Überlauf erkennen
* [x] Kommunikationsablauf protokollieren

### Abnahmekriterien

* Alle Tasks werden erfolgreich erzeugt.
* Keine Task verwendet eine schnelle Polling-Schleife.
* Testnachricht läuft über die vorgesehenen Queues.
* Arduino-`loop()` enthält keine Anwendungslogik.
* Stack- und Prioritätswerte sind dokumentiert.

---

# Phase 2 – SD-Karte und JSON-Speicherung

## 2.1 SD-Hardware

* [x] verwendete SD-Schnittstelle feststellen
* [x] Pinbelegung dokumentieren
* [x] SD-Karte ausschließlich im StorageTask initialisieren
* [x] Card-Detect-Pin prüfen
* [x] Card-Detect-Interrupt verwenden, wenn vorhanden
* [x] Entfernen und erneutes Einsetzen erkennen

## 2.2 Dateisystemstruktur

* [x] `/config` anlegen
* [x] `/cache` anlegen
* [x] `/queue` anlegen
* [x] `/mappings` anlegen
* [x] `/diagnostics` anlegen
* [x] `/logs` anlegen

## 2.3 JsonStorage

* [x] JSON-Datei aus `File` laden
* [x] JSON-Datei validieren
* [x] JSON-Datei serialisieren
* [x] maximale Dateigröße prüfen
* [x] verständliche Fehlercodes definieren
* [x] `schemaVersion` verarbeiten
* [x] Standardwerte einsetzen

## 2.4 Atomisches Speichern

* [x] temporäre `.tmp.json`-Datei schreiben
* [x] Datei flushen und schließen
* [x] temporäre Datei erneut validieren
* [x] bestehende Datei als `.bak.json` sichern
* [x] temporäre Datei umbenennen
* [x] Backup nach Erfolg entfernen
* [ ] Wiederherstellung nach Stromausfall testen

## 2.5 Storage-Queue

* [ ] Leseanfragen über `storageCommandQueue`
* [ ] Schreibanfragen über `storageCommandQueue`
* [ ] Antworten über `appEventQueue`
* [ ] mehrere Anfragen geordnet abarbeiten
* [ ] keine SD-Zugriffe aus anderen Tasks zulassen

## 2.6 Erste Dateien

* [ ] `/config/device.json`
* [ ] `/config/network.json`
* [ ] `/config/spoolman.json`
* [ ] `/config/ui.json`
* [ ] `/config/scale.json`
* [ ] `/config/nfc.json`

### Abnahmekriterien

* JSON-Dateien können gespeichert und erneut geladen werden.
* Beschädigte JSON-Dateien werden erkannt.
* Backup-Dateien können wiederhergestellt werden.
* Kein anderer Task greift direkt auf die SD-Karte zu.
* Bei fehlender SD-Karte wird kein Speichern vorgetäuscht.

---

# Phase 3 – Display, Touch, LVGL und UiTask

## 3.1 Hardwareprüfung

* [ ] Displaycontroller verifizieren
* [ ] Touchcontroller verifizieren
* [ ] Pinbelegung dokumentieren
* [ ] externe GPIO-Konflikte prüfen
* [ ] Displayhelligkeit prüfen

## 3.2 LovyanGFX

* [ ] Display initialisieren
* [ ] Rotation konfigurieren
* [ ] Farbtest durchführen
* [ ] Touchkoordinaten lesen
* [ ] Touchrotation prüfen

## 3.3 LVGL

* [ ] LVGL 9.x integrieren
* [ ] Renderpuffer einrichten
* [ ] PSRAM-Nutzung prüfen
* [ ] Flush-Callback implementieren
* [ ] Touch-Callback implementieren
* [ ] LVGL ausschließlich im UiTask betreiben

## 3.4 Ereignisgesteuerter UiTask

* [ ] `uiCommandQueue` verarbeiten
* [ ] nächsten LVGL-Ausführungszeitpunkt verwenden
* [ ] unnötig kurze feste Schleifen vermeiden
* [ ] optional Touch-IRQ untersuchen
* [ ] keine UI-Änderung aus anderen Tasks erlauben

## 3.5 EEZ Studio

* [ ] Projektauflösung 480 × 320
* [ ] LVGL 9 als Ziel
* [ ] EEZ Flow deaktiviert
* [ ] Export nach `src/ui/generated`
* [ ] Bootscreen
* [ ] Hauptmenü
* [ ] Spule-wiegen-Screen
* [ ] Einstellungen
* [ ] Fehlerdialog

### Abnahmekriterien

* Display und Touch funktionieren.
* LVGL läuft ausschließlich im UiTask.
* Andere Tasks aktualisieren die UI nur über Queues.
* Generierter Code wird nicht manuell verändert.
* Oberfläche bleibt während anderer Taskaktivitäten bedienbar.

---

# Phase 4 – Waage und ScaleTask

## 4.1 HX711-Hardware

* [ ] HX711-Pins verifizieren
* [ ] DOUT-Interruptfähigkeit prüfen
* [ ] DOUT-Interrupt registrieren
* [ ] ISR mit `IRAM_ATTR`
* [ ] ISR weckt ScaleTask über Task Notification
* [ ] keine HX711-Kommunikation in der ISR

## 4.2 ScaleTask

* [ ] auf Task Notification blockieren
* [ ] Messwert nach Benachrichtigung lesen
* [ ] Verbindungsfehler erkennen
* [ ] Messwert an Filter übergeben
* [ ] Ereignis an AppTask senden

## 4.3 Filterung

* [ ] gleitenden Mittelwert implementieren
* [ ] Tiefpassfilter implementieren
* [ ] Ausreißer erkennen
* [ ] negative Kleinwerte behandeln
* [ ] stabile Messung erkennen
* [ ] Stabilitätszeit konfigurieren

## 4.4 Tarierung und Kalibrierung

* [ ] Kommandos über ScaleCommandQueue
* [ ] Tarieren
* [ ] Kalibrierung mit Referenzgewicht
* [ ] Kalibrierwert über StorageTask speichern
* [ ] `/config/scale.json` verwenden
* [ ] Kalibrierwert beim Start laden

## 4.5 Tests

* [ ] Filtertest
* [ ] Stabilitätstest
* [ ] Ausreißertest
* [ ] Kalibrierberechnung
* [ ] simulierte Interruptfolge

### Abnahmekriterien

* ScaleTask blockiert bis ein Messwert verfügbar ist.
* Kein Busy Waiting auf HX711-DOUT.
* Kalibrierung wird als JSON auf SD gespeichert.
* Ruhendes Gewicht wird zuverlässig als stabil erkannt.
* UI bleibt während Messung und Kalibrierung aktiv.

---

# Phase 5 – NFC und NfcTask

## 5.1 PN532-Hardware

* [ ] Schnittstelle festlegen
* [ ] PN532-IRQ-Pin prüfen
* [ ] IRQ-Interrupt einrichten
* [ ] ISR weckt NfcTask
* [ ] keine I²C-Kommunikation in der ISR

## 5.2 Gemeinsamer I²C-Bus

* [ ] prüfen, ob Touch und PN532 denselben Bus verwenden
* [ ] bei gemeinsamem Bus I²C-Mutex anlegen
* [ ] Mutex nur aus Tasks verwenden
* [ ] maximale Haltezeit dokumentieren
* [ ] Deadlocks vermeiden

## 5.3 NFC lesen

* [ ] NfcTask blockiert auf Notification oder Command
* [ ] UID lesen
* [ ] NDEF lesen
* [ ] Payload `spoolman:<id>` parsen
* [ ] ungültige Payload melden
* [ ] wiederholtes Tag-Ereignis entprellen

## 5.4 NFC schreiben

* [ ] Schreibkommando über Queue
* [ ] Spoolman-ID validieren
* [ ] Payload schreiben
* [ ] Payload erneut lesen
* [ ] Ergebnis verifizieren
* [ ] Fehler an AppTask senden

## 5.5 Zuordnungen

* [ ] NFC-Zuordnungen über StorageTask speichern
* [ ] `/mappings/nfc-spools.json`
* [ ] Bambu-UID-Zuordnung vorbereiten
* [ ] keine geschützten Bambu-Tag-Bereiche verändern

### Abnahmekriterien

* NfcTask arbeitet ereignisgesteuert.
* Tag-Erkennung verwendet IRQ, sofern Hardware dies unterstützt.
* I²C wird durch einen Mutex geschützt, falls erforderlich.
* Tags können gelesen und verifiziert geschrieben werden.
* Zuordnungen liegen als JSON auf SD.

---

# Phase 6 – WiFiManager und NetworkTask

## 6.1 WiFiManager

* [ ] WiFiManager mit fester Version integrieren
* [ ] NetworkTask besitzt WiFiManager-Instanz
* [ ] Erstkonfiguration über Captive Portal
* [ ] Access Point mit Passwort schützen
* [ ] Portal-Timeout definieren
* [ ] Verbindungs-Timeout definieren

## 6.2 Blockierender Portalbetrieb

* [ ] Portal nur im NetworkTask starten
* [ ] UI bleibt während Portalbetrieb aktiv
* [ ] AppTask erhält Portalstatus
* [ ] Abbruch und Timeout behandeln
* [ ] kein `WiFiManager::process()` in Arduino-`loop()`

## 6.3 WiFi-Ereignisse

* [ ] `WiFi.onEvent()` registrieren
* [ ] Connected-Ereignis behandeln
* [ ] Got-IP-Ereignis behandeln
* [ ] Disconnect-Ereignis behandeln
* [ ] Lost-IP-Ereignis behandeln
* [ ] Callback sendet nur kurze Queue-Nachricht
* [ ] Event Group aktualisieren

## 6.4 Netzwerkkonfiguration

* [ ] zusätzliche Parameter aus `/config/network.json` laden
* [ ] Hostname
* [ ] DHCP oder statische IP
* [ ] DNS
* [ ] Portalname
* [ ] Timeouts
* [ ] Parameter über WiFiManager-Formular änderbar machen
* [ ] Änderungen über StorageTask speichern

## 6.5 UI

* [ ] WLAN-Status anzeigen
* [ ] IP-Adresse anzeigen
* [ ] „WLAN neu konfigurieren“
* [ ] „Captive Portal starten“
* [ ] „WLAN-Einstellungen löschen“

### Abnahmekriterien

* WLAN kann ohne Neukompilierung eingerichtet werden.
* Captive Portal blockiert nicht die übrigen Tasks.
* WiFi-Callbacks verändern keinen gemeinsam genutzten Zustand direkt.
* Zusätzliche Netzwerkparameter liegen als JSON auf SD.
* WLAN-Passwort wird nicht unverschlüsselt auf SD dupliziert.

---

# Phase 7 – SpoolmanTask

## 7.1 Konfiguration

* [ ] `/config/spoolman.json` definieren
* [ ] Serveradresse laden
* [ ] Port laden
* [ ] API-Pfad normalisieren
* [ ] Timeout laden

## 7.2 Kommunikation

* [ ] SpoolmanTask blockiert auf CommandQueue
* [ ] Health-Check
* [ ] Spule anhand ID laden
* [ ] Spulen suchen
* [ ] Messung übertragen
* [ ] HTTP-Statuscodes auswerten
* [ ] JSON sicher parsen

## 7.3 Cache

* [ ] Spoolman-Antworten über StorageTask cachen
* [ ] `/cache/spools.json`
* [ ] Cache-Alter speichern
* [ ] veralteten Cache markieren
* [ ] Cache nicht als führende Datenbank verwenden

## 7.4 Ausstehende Messungen

* [ ] Netzwerkfehler erkennen
* [ ] Benutzer über fehlgeschlagene Übertragung informieren
* [ ] optional Messung in `/queue/pending-measurements.json` speichern
* [ ] spätere Übertragung nur nach klarer Regel
* [ ] doppelte Übertragung verhindern

### Abnahmekriterien

* AppTask kann Spoolman-Aufträge asynchron auslösen.
* UI und AppTask blockieren nicht während HTTP-Anfragen.
* Spulendaten werden korrekt dargestellt.
* Messwerte können übertragen werden.
* Cache und Warteschlange liegen als JSON auf SD.

---

# Phase 8 – Vollständiger Anwendungsablauf

## 8.1 Zustandsautomat

* [ ] zentrale Zustände definieren
* [ ] zulässige Übergänge definieren
* [ ] Übergänge protokollieren
* [ ] verspätete Antworten behandeln
* [ ] `requestId` prüfen
* [ ] Abbruch ermöglichen

## 8.2 NFC-Wiegeablauf

* [ ] Tag erkennen
* [ ] Spoolman-ID laden
* [ ] Spulendaten anfordern
* [ ] stabile Messung abwarten
* [ ] Daten anzeigen
* [ ] Bestätigung abwarten
* [ ] Gewicht übertragen
* [ ] Erfolg anzeigen

## 8.3 Manueller Ablauf

* [ ] Spulen-ID eingeben
* [ ] Spule suchen
* [ ] Gewicht erfassen
* [ ] Messung speichern
* [ ] optional NFC-Tag schreiben

## 8.4 Fehlerfälle

* [ ] SD-Karte fehlt
* [ ] WLAN fehlt
* [ ] Spoolman fehlt
* [ ] Waage instabil
* [ ] NFC-Tag ungültig
* [ ] Tag wird entfernt
* [ ] Queue ist voll
* [ ] Antwort kommt zu spät
* [ ] Benutzer bricht ab

### Abnahmekriterien

* Vollständiger Workflow funktioniert ohne serielle Bedienung.
* Kein Teil des Ablaufs verwendet Busy Waiting.
* Services kommunizieren ausschließlich über die definierten RTOS-Mechanismen.
* Doppelte Messungen werden vermieden.
* Fehler führen zu einem definierten Zustand.

---

# Phase 9 – BambuTask und AMS

Erst beginnen, wenn Phase 8 zuverlässig funktioniert.

## 9.1 Bambu-Konfiguration

* [ ] `/config/bambu.json`
* [ ] Druckeradresse
* [ ] Seriennummer
* [ ] LAN-Zugangscode
* [ ] sichere Behandlung vertraulicher Daten
* [ ] keine Zugangsdaten protokollieren

## 9.2 BambuTask

* [ ] MQTT-Verbindung
* [ ] Statusmeldungen
* [ ] Wiederverbindung
* [ ] AMS-Daten
* [ ] Fehlerereignisse an AppTask
* [ ] keine direkte UI-Kommunikation

## 9.3 AMS-Zuordnung

* [ ] Spule per NFC erkennen
* [ ] Spoolman-Daten laden
* [ ] AMS-Slot auswählen
* [ ] Daten übertragen
* [ ] Druckerantwort prüfen
* [ ] Ergebnis anzeigen

### Abnahmekriterien

* BambuTask ist vollständig von Waage und Spoolman entkoppelt.
* Ausfall des Druckers beeinträchtigt die Grundfunktionen nicht.
* AMS-Slots können angezeigt und aktualisiert werden.

---

# Phase 10 – Robustheit und Diagnose

## 10.1 Task-Diagnose

* [ ] Stack High Water Marks
* [ ] Laufzeitstatistiken
* [ ] Queue-Auslastung
* [ ] Event Bits
* [ ] Heap
* [ ] PSRAM
* [ ] Diagnose in `/diagnostics/task-stats.json`

## 10.2 Fehler- und Belastungstests

* [ ] SD-Karte während Schreiben entfernen
* [ ] Stromausfall während Schreiben simulieren
* [ ] WLAN während HTTP-Anfrage trennen
* [ ] Spoolman neu starten
* [ ] HX711 trennen
* [ ] PN532 trennen
* [ ] Queue-Überlauf simulieren
* [ ] Interruptflut simulieren
* [ ] langsame SD-Karte testen

## 10.3 Langzeittest

* [ ] mehrstündiger Betrieb
* [ ] Speicherverbrauch beobachten
* [ ] Taskstacks beobachten
* [ ] Reconnect beobachten
* [ ] UI-Reaktionszeit beobachten
* [ ] Dateiintegrität prüfen

## 10.4 Watchdog

* [ ] alle Tasks blockieren oder geben CPU frei
* [ ] keine endlosen kritischen Abschnitte
* [ ] keine langen Mutexhaltezeiten
* [ ] kontrollierter Neustart nur durch AppTask
* [ ] Neustartgrund als JSON speichern

### Abnahmekriterien

* Keine offensichtlichen Speicherlecks.
* Keine Task läuft unnötig permanent.
* Beschädigte Dateien können wiederhergestellt werden.
* Hardware- und Netzwerkfehler führen nicht zu Deadlocks.
* Diagnoseinformationen sind als JSON verfügbar.

---

# Phase 11 – Dokumentation und Release

* [ ] Architekturdiagramm
* [ ] Taskdiagramm
* [ ] Queue- und Eventübersicht
* [ ] Interruptübersicht
* [ ] Taskprioritäten
* [ ] Stackgrößen
* [ ] GPIO-Tabelle
* [ ] Verdrahtungsplan
* [ ] JSON-Schemas
* [ ] SD-Verzeichnisstruktur
* [ ] WiFiManager-Anleitung
* [ ] Spoolman-Anleitung
* [ ] Kalibrierungsanleitung
* [ ] NFC-Format
* [ ] Bambu-Anleitung
* [ ] Build-Anleitung
* [ ] Testanleitung
* [ ] Lizenzprüfung
* [ ] Changelog
* [ ] reproduzierbarer Release-Build

### Abnahmekriterien

* Ein neuer Entwickler kann das Projekt kompilieren.
* Task- und Kommunikationsarchitektur sind nachvollziehbar.
* Alle JSON-Dateien und ihre Schemas sind dokumentiert.
* Hardware kann anhand der Dokumentation aufgebaut werden.
