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
* Spulenstatus,
* NFC/RFID-Zuordnungen.

Es gibt keine konkurrierende lokale Filament- oder Tag-Zuordnungsdatenbank.

---

# 2. Grundlegende Architekturentscheidungen

## 2.1 Spoolman ist verpflichtend

FilamentStation ist ein Spoolman-Client.

Ein vollständiger Betrieb ohne Verbindung zu Spoolman ist ausdrücklich nicht vorgesehen.

Ohne erreichbaren Spoolman-Server dürfen insbesondere nicht durchgeführt werden:

* Tag zuordnen,
* Tag-Zuordnung entfernen,
* Spule anhand Tag auflösen,
* Spule auswählen,
* Spule importieren,
* Gewicht dauerhaft aktualisieren,
* Spulenstammdaten ändern,
* Bambu-/OpenTag-Definitionen einer Spule zuordnen.

Weiterhin möglich bleiben:

* Geräteeinstellungen,
* WLAN-Konfiguration,
* Spoolman-Konfiguration,
* Diagnose,
* Firmwarefunktionen,
* Waagenkalibrierung,
* NFC/RFID-Tag technisch erkennen und klassifizieren.

Es gibt:

* keinen Offline-Spoolman-Modus,
* keine Offline-Zuordnungsdatenbank,
* keine Warteschlange für spätere Spoolman-Schreiboperationen,
* keinen persistenten Spoolman-Cache als Offline-Datenquelle.

## 2.2 Spoolman als Single Source of Truth

Spoolman ist die einzige persistente Datenquelle für die Beziehung:

```text
NFC/RFID-Tag ↔ Spule
```

Die Beziehung wird im zusätzlichen Spulenfeld:

```text
extra.tag
```

gespeichert.

Es dürfen keine parallelen lokalen UID→Spool-ID-Mappings geführt werden.

Insbesondere sind folgende bisherigen Dateien für die neue Architektur obsolet:

```text
/mappings/nfc-spools.json
/mappings/bambu-tags.json
/mappings/open-tags.json
```

Nach erfolgreicher Migration dürfen diese Dateien nicht mehr als Laufzeitdatenquelle verwendet werden.

---

# 3. Zielhardware

## 3.1 Hauptgerät

* WT32-SC01-Plus
* ESP32-S3
* Displayauflösung 480 × 320 Pixel
* kapazitiver Touchscreen
* PSRAM
* SD-Kartenanschluss

## 3.2 Externe Hardware

* HX711
* Wägezelle
* PN532 NFC/RFID-Leser
* NFC/RFID-Tags entsprechend Abschnitt 4
* optional Buzzer
* optional Status-LED
* optional Hardwaretaste
* optional SD-Card-Detect-Eingang

## 3.3 GPIO-Regel

Keine GPIO-Belegung erfinden.

Alle Pins werden zentral definiert in:

```text
src/config/BoardConfig.h
```

Vor Festlegung eines Pins müssen berücksichtigt werden:

* Display,
* Touch,
* SD,
* PSRAM,
* USB,
* Boot-Strapping,
* Interruptfähigkeit,
* bereits verwendete GPIOs.

---

# 4. Unterstützte NFC/RFID-Tags

Hardware-Technologie und logisches Datenformat sind getrennt zu behandeln.

## 4.1 Native FilamentStation-Tags

Unterstützt:

* NTAG213
* NTAG215 – bevorzugter Standard
* NTAG216

Verwendung:

* NDEF lesen,
* NDEF schreiben,
* eigenen FilamentStation-Payload löschen,
* Schreibvorgang verifizieren.

Payload:

```text
spoolman:<spool_id>
```

Beispiel:

```text
spoolman:42
```

Der Payload ist eine zusätzliche Information auf dem physischen Tag.

Die persistente Tag-Zuordnung erfolgt trotzdem ausschließlich über:

```text
Spoolman extra.tag
```

Der Payload `spoolman:<id>` darf nicht als konkurrierende Datenbank betrachtet werden.

## 4.2 Originale Bambu-Lab-RFID-Tags

Originale Bambu-Tags werden unterstützt.

Ziele:

* Tag erkennen,
* Bambu UUID lesen,
* Definition-Daten lesen,
* `TagDefinition` erzeugen,
* Spoolman-Spule anhand `extra.tag` finden,
* Import nach Spoolman ermöglichen.

Wichtige Regel:

> Originale Bambu-Lab-Tags werden niemals beschrieben oder gelöscht.

Die persistente Zuordnung wird ausschließlich in:

```text
Spoolman extra.tag
```

gespeichert.

## 4.3 OpenPrintTag

OpenPrintTag wird lesend unterstützt.

Ziele:

* erkennen,
* parsen,
* `TagDefinition` erzeugen,
* Spoolman-Spule zuordnen,
* Import nach Spoolman ermöglichen.

In Version 1 wird der Tag nicht verändert.

## 4.4 OpenTag3D

OpenTag3D wird lesend unterstützt.

In Version 1:

* lesen,
* parsen,
* Spoolman zuordnen,
* importieren,
* nicht verändern.

## 4.5 Legacy-Tags

Bekannte Legacy-Formate können gelesen und importiert werden.

Ein Legacy-Tag darf nur verändert werden, wenn der zuständige Parser ausdrücklich bestätigt, dass eine sichere Migration möglich ist.

## 4.6 Unbekannte Tags

Unbekannte Tags dürfen niemals automatisch verändert werden.

Mindestens anzeigen:

* Technologie,
* UID,
* NDEF-Status,
* Lesbarkeit,
* bekannte Schreibfähigkeit.

Eine UID darf über Spoolman `extra.tag` einer Spule zugeordnet werden.

---

# 5. TagIdentity und kanonische Tag-ID

## 5.1 Ziel

Alle Tagtypen müssen einen kanonischen String liefern, der in:

```text
Spoolman extra.tag
```

gespeichert wird.

Die Anwendungslogik verwendet dafür ein eigenes Modell.

Beispiel:

```cpp
enum class TagIdentitySource {
    Unknown,
    NfcUid,
    BambuUuid
};

struct TagIdentity {
    TagIdentitySource source;
    char value[40];
};
```

## 5.2 Normalisierung

Tag-Identifier werden grundsätzlich:

* als Hexadezimalstring,
* mit Großbuchstaben,
* ohne Doppelpunkte,
* ohne Bindestriche,
* ohne Leerzeichen

verwendet.

Beispiel:

```text
04:A2:11:FE:42:80:61
```

wird:

```text
04A211FE428061
```

## 5.3 Bambu

Bei originalen Bambu-Tags wird bevorzugt die stabile Bambu-Spulen-UUID verwendet, sofern sie vom vorhandenen Parser zuverlässig geliefert wird.

Beispiel:

```text
A1B2C3D4E5F6A1B2C3D4E5F6A1B2C3D4
```

Für normale NTAGs wird die NFC-UID verwendet.

Die gewählte Identität darf während eines Workflows nicht wechseln.

---

# 6. Spoolman-Feld `tag`

## 6.1 Definition

FilamentStation erwartet auf Spulenebene ein Spoolman-Extra-Feld:

```text
Key:  tag
Type: Text
```

Im Spool-JSON wird dieses Feld als:

```text
extra.tag
```

behandelt.

## 6.2 Initialisierung

Beim erfolgreichen Aufbau der Spoolman-Verbindung muss der SpoolmanClient prüfen, ob das Extra-Feld `tag` verfügbar ist.

Wenn es fehlt:

1. die tatsächlich vom verbundenen Spoolman unterstützte Extra-Field-API prüfen,
2. das Feld automatisch anlegen, sofern dies mit der verifizierten API möglich ist,
3. anschließend erneut prüfen.

Kann das Feld nicht bereitgestellt werden:

* Spoolman-Verbindung darf grundsätzlich als erreichbar gelten,
* NFC-Zuordnungsfunktionen gelten jedoch als nicht verfügbar,
* GUI zeigt einen verständlichen Fehler.

Keine API-Endpunkte erfinden.

Die konkrete API muss gegen die verwendete Spoolman-Version verifiziert werden.

## 6.3 Extra-Field-Encoding

Die Spoolman-API kann Extra-Felder API-seitig serialisiert repräsentieren.

Diese Besonderheit wird ausschließlich im `SpoolmanClient` behandelt.

Die übrige Anwendung sieht:

```cpp
spool.extraTag
```

als normalen String ohne zusätzliche JSON-Anführungszeichen oder Escape-Sequenzen.

---

# 7. Spoolman-basierte Tag-Auflösung

## 7.1 Grundprinzip

Ein Tag wird anhand seiner `TagIdentity` gegen:

```text
extra.tag
```

aufgelöst.

Benötigte Serviceoperation:

```cpp
findSpoolByTag(tagIdentity)
```

Ergebnis:

```cpp
enum class TagLookupStatus {
    NotFound,
    Found,
    Duplicate,
    Error
};
```

## 7.2 Eindeutigkeit

Ein Wert von:

```text
extra.tag
```

darf logisch nur genau einer Spule zugeordnet sein.

Mögliche Fälle:

### Kein Treffer

```text
NotFound
```

Tag ist nicht zugeordnet.

### Genau ein Treffer

```text
Found
```

Tag ist eindeutig zugeordnet.

### Mehrere Treffer

```text
Duplicate
```

Das ist ein Datenfehler.

FilamentStation darf in diesem Fall keine Spule automatisch auswählen.

GUI zeigt:

```text
Diese Tag-ID ist mehreren Spulen zugeordnet.
Bitte die Spoolman-Daten korrigieren.
```

## 7.3 Native `spoolman:<id>`-Tags

Enthält ein NTAG den Payload:

```text
spoolman:42
```

darf dieser Wert zur schnellen Plausibilitätsprüfung verwendet werden.

Die persistente Zuordnung bleibt jedoch:

```text
extra.tag
```

Bei Widerspruch zwischen:

```text
NDEF spoolman:<id>
```

und:

```text
Spoolman extra.tag
```

darf der Konflikt nicht still ignoriert werden.

Spoolman ist die führende Quelle.

---

# 8. Benutzerkonzept für Tag-Zuordnungen

Die Benutzeroberfläche kennt nur:

```text
Tag zuordnen
Tag-Zuordnung entfernen
```

Nicht als Benutzerfunktion anzeigen:

```text
Tag schreiben
Tag verknüpfen
Tag löschen
Tag trennen
```

Technische Schreib-/Löschoperationen dürfen intern weiterhin existieren.

---

# 9. TagCapabilities

```cpp
struct TagCapabilities {
    bool canAssociate;
    bool canWriteFilamentStationPayload;
    bool canClearFilamentStationPayload;
    bool preserveOriginalContent;
};
```

`writable == true` reicht nicht als Entscheidung aus.

## 9.1 Native NTAG213/215/216

Zuordnen:

```text
Spoolman extra.tag aktualisieren
+
spoolman:<id> schreiben
+
verifizieren
```

Entfernen:

```text
Spoolman extra.tag leeren
+
eigenen FilamentStation-Payload löschen
+
verifizieren
```

## 9.2 Bambu

Zuordnen:

```text
Spoolman extra.tag aktualisieren
```

Tag unverändert.

Entfernen:

```text
Spoolman extra.tag leeren
```

Tag unverändert.

## 9.3 OpenPrintTag/OpenTag3D

Gleiches Prinzip:

* Spoolman-Zuordnung ändern,
* physischen Tag nicht verändern.

## 9.4 Unknown

* Zuordnung über Spoolman möglich,
* Tag unverändert.

---

# 10. AssignTag-Workflow

Der AppTask orchestriert den Workflow.

Voraussetzung:

```text
EVENT_SPOOLMAN_READY
```

muss gesetzt sein.

Workflow:

```text
AssignTag(spoolId)
    ↓
TagIdentity bestimmen
    ↓
Spoolman findSpoolByTag(tag)
    ↓
bestehende Zuordnung prüfen
    ↓
Zielspule laden
    ↓
Spoolman extra.tag der Zielspule setzen
    ↓
Serverantwort verifizieren
    ↓
canWriteFilamentStationPayload?
    ├─ nein
    │    ↓
    │    Erfolg: ServerAssignmentOnly
    │
    └─ ja
         ↓
         NfcTask WriteFilamentStationPayload
         ↓
         Tag erneut lesen
         ↓
         UID/Identity prüfen
         ↓
         Payload prüfen
         ↓
         Erfolg: ServerAssignmentAndTagWritten
```

## 10.1 Bereits derselben Spule zugeordnet

Der Vorgang ist idempotent.

Kein unnötiges Spoolman-Update durchführen.

Bei beschreibbarem nativen Tag darf ein fehlender beziehungsweise falscher Payload repariert werden.

## 10.2 Tag bereits anderer Spule zugeordnet

Benutzer muss eine Neuzuordnung bestätigen.

Beispiel:

```text
Dieser Tag ist derzeit Spule #17 zugeordnet.

Neue Zuordnung:
Spule #42

[Abbrechen] [Neu zuordnen]
```

Beim Neu-Zuordnen:

1. alte Zuordnung bestimmen,
2. alten `extra.tag`-Wert entfernen,
3. neuen `extra.tag` setzen,
4. Fehlerfälle behandeln,
5. wenn nötig Best-Effort-Rollback versuchen.

Keine doppelte Zuordnung erzeugen.

## 10.3 Schreibfehler am physischen NTAG

Wenn:

* Spoolman erfolgreich aktualisiert wurde,
* anschließendes NDEF-Schreiben aber fehlschlägt,

bleibt die Spoolman-Zuordnung bestehen.

Anzeige:

```text
Tag wurde in Spoolman zugeordnet.
Der NFC-Tag konnte jedoch nicht aktualisiert werden.
```

Spoolman bleibt die führende Quelle.

---

# 11. RemoveTagAssignment-Workflow

Voraussetzung:

```text
EVENT_SPOOLMAN_READY
```

Workflow:

```text
RemoveTagAssignment
    ↓
TagIdentity bestimmen
    ↓
Spoolman findSpoolByTag(tag)
    ↓
zugeordnete Spule eindeutig bestimmen
    ↓
Spoolman extra.tag leeren
    ↓
Serverantwort verifizieren
    ↓
canClearFilamentStationPayload?
    ├─ nein
    │    ↓
    │    Erfolg: ServerAssignmentRemoved
    │
    └─ ja
         ↓
         eigenen Payload löschen
         ↓
         erneut lesen
         ↓
         verifizieren
```

Fremde/originale Tagdaten werden niemals gelöscht.

---

# 12. Keine lokalen Tag-Mappings

Folgende Dateien werden nicht mehr als Teil der Zielarchitektur verwendet:

```text
/mappings/nfc-spools.json
/mappings/bambu-tags.json
/mappings/open-tags.json
```

Folgende Komponenten dürfen nach der Migration nicht mehr existieren:

* lokales UID→Spool-ID-Repository,
* lokaler Tag-Mapping-Service,
* Mapping-Lookup über StorageTask,
* Mapping-Konfliktprüfung auf SD,
* lokale Zuordnung als Fallback bei Spoolman-Ausfall.

Der StorageTask ist nicht mehr an der normalen NFC-Zuordnung beteiligt.

---

# 13. Migration vorhandener lokaler Mappings

Die bisherige Firmware kann bereits Mapping-Dateien erzeugt haben.

Diese Daten dürfen nicht einfach ignoriert oder dauerhaft parallel weitergeführt werden.

Für eine Übergangsversion ist eine einmalige Migration zulässig.

## 13.1 Migrationsregeln

Wenn Legacy-Mapping-Dateien vorhanden sind:

1. Spoolman muss erreichbar sein.
2. Datei nur über StorageTask lesen.
3. jeden UID→Spool-ID-Eintrag validieren.
4. Tag-ID normalisieren.
5. Spule in Spoolman laden.
6. prüfen, ob `extra.tag` frei, identisch oder widersprüchlich ist.
7. nur eindeutige Einträge automatisch migrieren.
8. Konflikte nicht überschreiben.
9. Fehler protokollieren.
10. nach vollständig erfolgreicher Migration Legacy-Datei löschen.

## 13.2 Konflikt

Wenn eine Spule bereits einen anderen Tag besitzt oder der Tag bereits einer anderen Spule zugeordnet ist:

* nicht automatisch überschreiben,
* WARN/ERROR loggen,
* Legacy-Datei nicht vollständig löschen,
* Konflikt dokumentieren.

Die Migrationslogik ist Übergangscode und darf später entfernt werden.

---

# 14. Online-only-Spoolman-Betrieb

## 14.1 Keine Pending Writes

Nicht mehr verwenden:

```text
/queue/pending-measurements.json
```

für spätere Spoolman-Schreibvorgänge.

Bei Spoolman-Fehler:

* Operation bleibt fehlgeschlagen,
* GUI zeigt Fehler,
* Benutzer kann erneut versuchen.

Keine automatische spätere Synchronisierung.

## 14.2 Kein persistenter Spoolman-Cache

Folgende Dateien sind nicht als Offline-Datenquelle erforderlich:

```text
/cache/spools.json
/cache/filaments.json
/cache/vendors.json
```

Ein kurzlebiger RAM-Cache innerhalb einer aktiven Online-Sitzung ist erlaubt.

Regeln:

* niemals als Offline-Fallback,
* nach Spoolman-Schreiboperation invalidieren,
* mit begrenzter Lebensdauer,
* Server bleibt Quelle der Wahrheit.

---

# 15. FreeRTOS-Task-Architektur

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

## 15.1 UiTask

Verantwortlich für:

* LVGL,
* EEZ,
* Touch,
* UI-Kommandos,
* Screens,
* Dialoge.

Nur UiTask darf LVGL verwenden.

## 15.2 AppTask

Verantwortlich für:

* Zustandsautomat,
* Workflows,
* Task-Koordination,
* `requestId`,
* `printerId`,
* NFC-Zuordnungsworkflow,
* Spoolman-Verfügbarkeitsprüfung.

Keine direkten Hardwarezugriffe.

## 15.3 ScaleTask

Verantwortlich für:

* HX711,
* Tarierung,
* Kalibrierung,
* Filter,
* Stabilität.

## 15.4 NfcTask

Verantwortlich für:

* PN532,
* UID,
* NDEF,
* TagIdentity,
* Parser,
* Schreiben,
* Löschen,
* Verifikation.

Technische Commands dürfen heißen:

```text
ReadTag
WriteFilamentStationPayload
ClearFilamentStationPayload
VerifyTag
```

Sie sind keine Benutzeraktionen.

## 15.5 StorageTask

Verantwortlich für:

* Gerätekonfigurationen,
* JSON,
* Backups,
* Diagnose,
* Bambu-/Geräte-Daten, die tatsächlich lokal erforderlich sind.

Nicht verantwortlich für:

```text
NFC UID ↔ Spoolman ID
```

## 15.6 NetworkTask

Verantwortlich für WLAN und WiFiManager.

## 15.7 SpoolmanTask

Verantwortlich für:

* REST-Kommunikation,
* Spulen,
* Hersteller,
* Filamente,
* Gewicht,
* Extra Fields,
* Tag-Lookup,
* Tag-Zuordnung,
* Tag-Zuordnung entfernen.

## 15.8 BambuTask

Verantwortlich für mehrere Bambu-Drucker und AMS.

---

# 16. FreeRTOS-Kommunikation

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

Event Group mindestens:

```cpp
EVENT_UI_READY
EVENT_SD_READY
EVENT_SCALE_READY
EVENT_NFC_READY
EVENT_WIFI_CONNECTED
EVENT_SPOOLMAN_READY
EVENT_BAMBU_READY
EVENT_FATAL_ERROR
```

---

# 17. Polling und Interrupts

Keine Busy-Wait-Schleifen.

Tasks blockieren auf:

* Queue,
* Notification,
* Event Group,
* Timer.

ISRs führen keine:

* I²C-Kommunikation,
* SPI-Kommunikation,
* JSON-Verarbeitung,
* LVGL-Aufrufe,
* HTTP-Aufrufe,
* MQTT-Aufrufe,
* Logging-Ausgabe

durch.

---

# 18. Einheitliches Logging

## 18.1 Ziel

Alle FilamentStation-Anwendungslogs verwenden ein gemeinsames Format.

Nicht zulässig sind innerhalb des eigenen Anwendungscodes Mischformen wie:

```text
initial JSON files read
*wm:3 networks found
D (122212) [network.cpp]: Wifi Connected
[NfcTask] Tag detected
```

## 18.2 Kanonisches Format

```text
<LEVEL> [<COMPONENT>] <message> key=value key=value
```

Beispiele:

```text
I [APP] System started firmware=0.4.0
I [STORAGE] Initial JSON files loaded count=7
D [NET] WiFi scan completed networks=3
I [NET] WiFi connected ip=192.168.1.42
I [SPOOLMAN] Server connected version=0.26.0
D [NFC] Tag classified tech=NTAG215 format=FilamentStation uid=04A211FE428061
I [SPOOLMAN] Tag assigned spool_id=42 tag=04A211FE428061
W [SPOOLMAN] Duplicate tag assignment tag=04A211FE428061 matches=2
E [SPOOLMAN] Request failed op=SetSpoolTag status=503
```

## 18.3 Log-Level

Verwendet werden exakt:

```text
E = ERROR
W = WARN
I = INFO
D = DEBUG
T = TRACE
```

Keine zusätzlichen Levelnamen einführen.

## 18.4 Komponenten

Bevorzugte stabile Component-Tags:

```text
APP
RTOS
UI
DISPLAY
TOUCH
STORAGE
NET
SPOOLMAN
SCALE
NFC
BAMBU
```

Keine Dateinamen als wechselnde Component-Namen verwenden.

## 18.5 Zentrale Logger-API

Eine zentrale Logging-Schicht wird verwendet, beispielsweise:

```text
src/services/Logger.h
src/services/Logger.cpp
```

oder eine äquivalente bestehende Struktur.

Bevorzugte Makros:

```cpp
FS_LOGE(component, format, ...)
FS_LOGW(component, format, ...)
FS_LOGI(component, format, ...)
FS_LOGD(component, format, ...)
FS_LOGT(component, format, ...)
```

Direkte Aufrufe wie:

```cpp
Serial.print(...)
Serial.printf(...)
printf(...)
log_i(...)
log_d(...)
ESP_LOGI(...)
```

dürfen im normalen Anwendungscode nach der Migration nicht mehr für Laufzeit-Logging verwendet werden.

Ausnahmen:

* Logger-Implementierung selbst,
* unveränderbarer Drittanbieter-Code,
* ESP32-Bootloader,
* ROM-Ausgabe,
* Panic-/Crash-Ausgabe.

## 18.6 Thread-Sicherheit

Mehrere Tasks dürfen keine ineinander verschachtelten Logzeilen erzeugen.

Der Logger muss jede vollständige Zeile atomar ausgeben.

Mögliche Lösung:

* zentraler Logging-Mutex,
* fester Stackbuffer,
* genau eine serielle Ausgabe pro fertiger Logzeile.

Kein dynamischer Arduino-`String` für jede Logzeile notwendig.

Logging aus ISRs ist verboten.

## 18.7 Zeitstempel

FilamentStation selbst schreibt im seriellen Standardformat keinen Zeitstempel.

Der Zeitstempel wird durch PlatformIO ergänzt.

Dadurch bleibt das Geräteformat:

```text
I [NET] WiFi connected ip=192.168.1.42
```

einfach und wiederverwendbar.

## 18.8 PlatformIO Monitor

`platformio.ini` soll mindestens enthalten:

```ini
monitor_speed = 115200

monitor_filters =
    default
    esp32_exception_decoder
    time
    log2file
```

Dadurch können:

* Zeitstempel ergänzt,
* Logs in Datei geschrieben,
* ESP32-Crash-Adressen dekodiert

werden.

## 18.9 Drittanbieter-Logging

Drittanbieter-Ausgaben müssen, soweit mit offizieller API möglich, deaktiviert oder reduziert werden.

Insbesondere:

### WiFiManager

Die eigene Debugausgabe wie:

```text
*wm:
```

deaktivieren, wenn die verwendete Version dies unterstützt.

Stattdessen relevante Zustände selbst loggen:

```text
I [NET] WiFiManager portal started
D [NET] WiFi scan completed networks=3
I [NET] WiFi connected
```

### Arduino-ESP32 / ESP-IDF Runtime Debug

Framework-Debugausgaben im normalen Betrieb soweit sinnvoll reduzieren.

FilamentStation erzeugt eigene Netzwerkstatuslogs.

Boot-, Panic- und Exception-Ausgaben bleiben unverändert, damit der PlatformIO-Exception-Decoder funktioniert.

Drittanbieterbibliotheken werden nicht gepatcht, nur um deren Logformat zu ändern.

## 18.10 Log-Level-Konfiguration

Der maximale Log-Level soll zentral konfigurierbar sein.

Beispiel:

```cpp
enum class LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Trace
};
```

Empfohlene Defaults:

Release:

```text
INFO
```

Entwicklung:

```text
DEBUG
```

TRACE nur gezielt.

---

# 19. SpoolmanTask-Erweiterungen

Benötigte fachliche Operationen:

```text
HealthCheck
EnsureTagExtraField
GetSpool
FindSpools
FindSpoolByTag
SetSpoolTag
ClearSpoolTag
UpdateWeight
ImportTagDefinition
```

Die tatsächlichen REST-Endpunkte müssen gegen die verwendete Spoolman-Version geprüft werden.

Keine Endpunkte erfinden.

---

# 20. WiFiManager

WLAN wird über WiFiManager eingerichtet.

Captive Portal läuft im NetworkTask.

Andere Tasks bleiben aktiv.

SSID und WLAN-Passwort werden nicht zusätzlich auf SD gespeichert.

WiFiManager-Debugausgabe wird nach erfolgreicher Logger-Migration deaktiviert.

---

# 21. SD und JSON

Lokal gespeichert werden weiterhin notwendige Gerätekonfigurationen.

Beispiel:

```text
/config/device.json
/config/network.json
/config/spoolman.json
/config/bambu.json
/config/ui.json
/config/scale.json
/config/nfc.json

/mappings/printer-slots.json

/diagnostics/system.json
/diagnostics/last-error.json
/diagnostics/task-stats.json
```

Nicht Teil der Zielarchitektur:

```text
/mappings/nfc-spools.json
/mappings/bambu-tags.json
/mappings/open-tags.json
/queue/pending-measurements.json
/cache/spools.json
/cache/filaments.json
/cache/vendors.json
```

---

# 22. Spoolman-Konfiguration

Settings mindestens:

* Name,
* HTTP/HTTPS,
* Host/IP,
* Port,
* API-Basispfad,
* Timeout,
* Verbindung testen,
* Serverversion,
* Status des `tag` Extra-Felds.

Beispiel:

```text
Spoolman       verbunden
Version        0.26.0
NFC-Feld tag   bereit
```

Fehlt das Extra-Feld und kann es nicht angelegt werden:

```text
NFC-Feld tag   nicht verfügbar
```

---

# 23. Mehrere Bambu-Drucker

Jeder Druckerbefehl enthält:

```cpp
PrinterId printerId;
```

Druckerwechsel ohne Neustart.

Aktiver Drucker permanent sichtbar.

Originale Bambu-RFID-Tags bleiben read-only.

Bambu-UUIDs können direkt mit Spoolman `extra.tag` abgeglichen werden.

---

# 24. GUI-Grundlage

GUI orientiert sich funktional an SpoolEase.

Hauptfunktionen:

* AMS-/External-Übersicht,
* Staging,
* Druckerauswahl,
* Slotdetails,
* Quick Weight,
* Advanced Weight,
* Tag zuordnen,
* Tag-Zuordnung entfernen,
* Tag-Import,
* Bambu-Spulentyp,
* Diagnose,
* Settings.

Nicht anzeigen:

```text
Tag schreiben
Tag verknüpfen
Tag löschen
Tag trennen
```

---

# 25. Tag-Zuordnungsstatus

Der Status stammt ausschließlich aus Spoolman.

Mögliche Zustände:

```cpp
enum class TagAssignmentState {
    SpoolmanUnavailable,
    Unassigned,
    AssignedToSelectedSpool,
    AssignedToOtherSpool,
    DuplicateConflict,
    Error
};
```

GUI-Beispiele:

```text
Nicht zugeordnet
```

```text
Zugeordnet zu Spule #42
```

```text
Zugeordnet zu anderer Spule #17
```

```text
Zuordnungsfehler: Tag mehrfach in Spoolman vorhanden
```

Nicht aus lokalen Mapping-Dateien ableiten.

---

# 26. Staging-Aktionen

```text
Slot konfigurieren
Advanced Weight
Staging leeren
Tag zuordnen
Tag-Zuordnung entfernen
Spule auswählen
```

Tag-Aktionen werden deaktiviert, solange Spoolman nicht verbunden ist.

---

# 27. Ergebnisdialoge

Native NTAG:

```text
Tag erfolgreich zugeordnet.

Spoolman wurde aktualisiert und die Zuordnung
wurde zusätzlich auf dem NFC-Tag gespeichert.
```

Bambu/Open/Unknown:

```text
Tag erfolgreich zugeordnet.

Die Zuordnung wurde in Spoolman gespeichert.
Der originale Taginhalt wurde nicht verändert.
```

Entfernen native:

```text
Tag-Zuordnung entfernt.

Spoolman wurde aktualisiert und die
FilamentStation-Daten wurden vom Tag entfernt.
```

Entfernen Bambu/Open:

```text
Tag-Zuordnung entfernt.

Spoolman wurde aktualisiert.
Der originale Taginhalt wurde nicht verändert.
```

---

# 28. EEZ-Studio-Regeln

Bestehendes Projekt:

```text
ui-project/FilamentStation.eez-project
```

weiterverwenden.

Generierter Code:

```text
src/ui/generated/
```

nicht manuell ändern.

Eigene Actions außerhalb des generierten Codes.

---

# 29. Projektstruktur

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
│   ├── logging.md
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
    │   ├── Logger.cpp
    │   ├── Logger.h
    │   ├── SpoolmanClient.cpp
    │   └── ...
    ├── models/
    ├── nfc/
    └── ui/
```

---

# 30. Fehlerbehandlung

Jeder Fehler enthält mindestens:

* Quelle,
* Fehlercode,
* Schweregrad,
* requestId,
* optional printerId,
* verständliche Meldung.

Spoolman-Ausfall ist kein Anlass für Offline-Schreiboperationen.

Stattdessen:

* Fehler melden,
* Workflow stoppen,
* Retry anbieten.

---

# 31. Logging sensibler Informationen

Nicht loggen:

* WLAN-Passwort,
* vollständigen Bambu-Zugangscode,
* Tokens,
* Schlüssel.

NFC-UID beziehungsweise Bambu-UUID darf im DEBUG-Level vollständig geloggt werden.

Im INFO-Level nur dort, wo sie zur Diagnose des Zuordnungsworkflows sinnvoll ist.

---

# 32. Tests

Zusätzlich zu bereits vorhandenen Tests müssen geprüft werden:

## Logging

* Format für alle fünf Log-Level,
* Component-Tag,
* key=value-Felder,
* lange Logzeilen,
* gleichzeitige Logs mehrerer Tasks,
* Log-Level-Filterung,
* keine direkten alten Serial-Logs im Anwendungscode,
* WiFiManager-Debug deaktiviert,
* Exception-Decoder weiterhin nutzbar.

## Spoolman `extra.tag`

* Tag-Feld vorhanden,
* Tag-Feld fehlt,
* Tag-Feld kann angelegt werden,
* Tag-Feld kann nicht angelegt werden,
* Tag nicht gefunden,
* genau ein Treffer,
* mehrfacher Treffer,
* Setzen,
* Leeren,
* API-Fehler,
* JSON-Encoding/Decoding von Extra Fields.

## AssignTag

* NTAG → Spoolman + NDEF,
* Bambu → nur Spoolman,
* OpenPrintTag → nur Spoolman,
* OpenTag3D → nur Spoolman,
* Unknown → nur Spoolman,
* bereits gleiche Spule,
* andere Spule,
* Duplicate Conflict,
* Spoolman offline,
* physisches Schreiben schlägt nach erfolgreichem Spoolman-Update fehl.

## RemoveTagAssignment

* NTAG → Spoolman leeren + Payload entfernen,
* Bambu → nur Spoolman,
* Open → nur Spoolman,
* Unknown → nur Spoolman,
* kein Treffer,
* Duplicate Conflict,
* Spoolman offline.

## Migration

* alte Mapping-Datei fehlt,
* leere Mapping-Datei,
* eindeutiger Eintrag,
* Konflikt,
* ungültige Spool-ID,
* Spoolman offline,
* Datei erst nach vollständiger Migration löschen.

---

# 33. Coding-Regeln

* C++17.
* keine Busy-Wait-Schleifen.
* keine direkten Hardwarezugriffe aus AppTask.
* keine LVGL-Aufrufe außerhalb UiTask.
* keine SD-Zugriffe außerhalb StorageTask.
* keine lokalen NFC→Spool-Mappings.
* keine Offline-Spoolman-Schreibwarteschlange.
* Spoolman ist Single Source of Truth.
* Parser ohne Netzwerk- und Storagezugriff.
* technische Tagoperationen im NfcTask.
* Workflowentscheidung im AppTask.
* Spoolman-Persistenz im SpoolmanTask.
* Runtime-Logging ausschließlich über zentralen Logger.
* keine Logausgabe aus ISR.
* keine Formatdetails externer Protokolle erfinden.
* Queue-Nachrichten klein halten.

---

# 34. Regeln für Codex

Vor jeder Änderung:

1. `AGENTS.md` vollständig lesen.
2. `TASKS.md` vollständig lesen.
3. Repository analysieren.
4. bestehenden Code weiterverwenden.
5. keine bereits funktionierenden Bereiche unnötig neu schreiben.
6. aktuellen Taskstatus beachten.

Aktueller Stand:

> Alle bisherigen Aufgaben bis einschließlich Phase 7.5 sind abgeschlossen.

Die nächsten Migrationsaufgaben sind:

```text
7.6 Einheitliches Logging
7.7 NFC-Zuordnung nach Spoolman extra.tag migrieren
7.8 Online-only-Spoolman-Betrieb bereinigen
```

Wichtige Regeln:

* kein neues lokales Tag-Mapping einführen,
* bestehende Mapping-Logik gezielt entfernen,
* Spoolman `extra.tag` als einzige persistente Zuordnung verwenden,
* native NFC-Payloads bleiben zusätzliche physische Information,
* Spoolman bleibt authoritative,
* ohne Spoolman kein Zuordnungs-/Gewichts-/Importbetrieb,
* vorhandene NFC-Parser erhalten,
* vorhandene Waagenlogik erhalten,
* vorhandene Spoolman-REST-Infrastruktur erweitern,
* vorhandenes EEZ-Projekt erhalten,
* Drittanbieterbibliotheken nicht patchen, nur um Logging zu formatieren.

Nach jedem Teilpaket:

1. Tests ausführen.
2. `pio run`.
3. Compilerwarnungen prüfen.
4. geänderte Dateien auflisten.
5. nur tatsächlich erledigte Checkboxen abhaken.
6. nächste offene Aufgabe nennen.
