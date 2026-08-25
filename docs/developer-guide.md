# Entwicklerhandbuch

Kurzanleitungen für die häufigsten Entwicklungsaufgaben in diesem Projekt.
Referenzwissen (Architektur, Tasks, Queues, Events) steht in
`docs/architecture.md`; dies hier ist bewusst als "Kochbuch" gehalten --
welche Datei(en) für eine bestimmte Änderung angefasst werden müssen.

## Build

```text
pio run -e wt32-s3-wrover-n16r2
```

Zielumgebung laut `platformio.ini`: ESP32-S3 (`esp32-s3-devkitc-1`-Board,
QIO-QSPI, 16 MB Flash, `default_16MB.csv`-Partitionstabelle). Ein
erfolgreicher Build für diese Umgebung muss **0 Compilerwarnungen**
erzeugen -- das ist in diesem Projekt eine harte Regel, keine Empfehlung
(siehe die zahlreichen "hex escape sequence out of range"-Funde in
`TASKS.md`, die genau deshalb sofort behoben wurden).

## Upload

```text
pio run -e wt32-s3-wrover-n16r2 -t upload
```

Fester Port `COM5` (`upload_port`/`monitor_port` in `platformio.ini`).
Serieller Monitor:

```text
pio device monitor -e wt32-s3-wrover-n16r2
```

**Bekannte Stolperfalle:** Der Upload schlägt auf diesem Setup gelegentlich
beim ersten (manchmal zweiten) Versuch mit "Could not open COM5" oder "No
serial data received" fehl -- in der Praxis reicht fast immer ein
Aus-/Einstecken des USB-Kabels vor dem nächsten Versuch. Ein offener eigener
`pio device monitor` blockiert ebenfalls den Port und muss vor dem Upload
geschlossen werden.

## Tests

Vier getrennte native Testumgebungen, je nach betroffenem Subsystem:

| Umgebung | Deckt ab |
|---|---|
| `native-spoolman-tests` | Spoolman-Client/-URL/-Katalog, Bambu-Konfiguration/-Protokoll, SemVer, Druckermodell -- die mit Abstand größte Suite |
| `native-scale-tests` | `ScaleFilter`, `ScaleMath` |
| `native-nfc-tests` | NFC-Payload, NTAG21x, `TagIdentity`, alle Tag-Parser inkl. Registry, OpenPrintTag, OpenTag3D |
| `native-logger-tests` | Logger-Format/-Filter |

```text
pio test -e native-spoolman-tests
pio test -e native-scale-tests
pio test -e native-nfc-tests
pio test -e native-logger-tests
```

Jede Umgebung kompiliert nur die für sie relevanten Quellen
(`build_src_filter` in `platformio.ini`) -- eine Änderung an
`services/ScaleMath.cpp` erfordert also `native-scale-tests`, nicht
zwingend `native-spoolman-tests`. Alle vier laufen mit
`toolchain-gccmingw32` (GCC 5.1.0) -- **kein C++17-Nested-Namespace-Syntax**
(`namespace a::b { }`) in Dateien, die eine dieser Suiten kompilieren muss;
stattdessen die verschachtelte Form (`namespace a { namespace b { ... } }`),
siehe `services/SpoolmanUrl.h` als bestehendes Vorbild.

Für die Zielhardware existiert kein separates Testziel; dort zählt der
tatsächliche Betrieb (Build + Flash + manuelle/geloggte Verifikation).

## EEZ Export

Das UI-Layout lebt in `ui-project/FilamentStation.eez-project` (EEZ Studio
Projektdatei) und wird daraus nach `src/ui/generated/screens.c`/`.h`
exportiert -- **dieser Export ist ein manueller Schritt in der EEZ-Studio-
Desktop-Anwendung, kein Teil des PlatformIO-Builds.** Nach jeder Änderung am
`.eez-project` (egal ob per Hand in EEZ Studio oder programmatisch, siehe
unten) muss vor dem nächsten Build/Flash erneut in EEZ Studio exportiert
werden -- sonst bleiben `screens.c`/`.h` auf dem alten Stand und ein
gerade committeter Style/Layout-Fix zeigt sich nicht auf dem Gerät (mehrfach
in `TASKS.md` als Fehlerursache dokumentiert, z. B. "Vor dem Flashen muss
der Nutzer erneut in EEZ Studio exportieren").

Für gezielte, skriptbare Änderungen am `.eez-project` (z. B. viele
gleichartige Buttons in Serie anpassen) steht das `cli-anything-eez-studio`-
Werkzeug zur Verfügung (`project validate`/`project widgets` zum
Gegenprüfen nach einer Änderung); komplexere Layoutarbeit bleibt
Handarbeit in der Desktop-App.

Bildschirmtexte auf den in `TASKS.md` als "Allgemeine Regel" benannten
Screens (u. a. `SCR_SETTINGS_HOME`, `SCR_STAGING_DETAILS`,
`SCR_STAGING_ACTIONS`, `SCR_TRAY_ACTIONS`, `SCR_TAG_ACTION_SELECT`,
`SCR_TAG_LEGACY`, `SCR_TAG_DEFINITION_IMPORT`, `SCR_TAG_RESULT`) dürfen
**nicht** durch `setControlText()` in `UiBridge.cpp` überschrieben werden --
der im EEZ-Projekt gesetzte Text ist verbindlich; ein gewünschter Textwechsel
gehört ins `.eez-project`, nicht in C++.

## Logger

Siehe `docs/logging.md` für das vollständige Referenzwissen (Format, Level,
Komponenten, `componentMinimumLevel()`). Kurzfassung zum Hinzufügen einer
neuen Komponente: Eintrag in `enum class LogComponent` (`services/Logger.h`)
**und** einen passenden `case` in `componentText()`
(`services/LoggerFormat.cpp`) -- der `switch` dort hat bewusst kein
`default:`-Label, ein vergessener Eintrag fällt also als Compilerwarnung
auf, nicht erst als `"UNKNOWN"` auf dem Gerät.

## Screen

1. Screen im EEZ-Projekt anlegen, exportieren (siehe "EEZ Export").
2. Neuen Wert in `enum class UiScreenId` (`rtos/Commands.h`) ergänzen.
3. In `UiBridge.cpp`s `processUiCommand()`/`showScreen()`-Dispatch
   (`switch` über `UiScreenId`) einen `case` für den neuen Wert ergänzen --
   verweist typischerweise auf `objects.<neuer_screen_name>` aus der
   generierten `screens.h`.
4. Zurück-Navigation in `AppTask.cpp`s `Back`-Handler ergänzen (siehe
   `docs/workflows.md`, Abschnitt "Navigation" -- es gibt keinen
   generischen Stack, jeder Screen braucht sein eigenes Zurück-Ziel).
5. Falls der Screen dynamische Inhalte zeigt: passenden `UiCommandType`
   (siehe "Action" unten) zum Befüllen ergänzen.

## Action

1. Neuen Wert in `enum class UiActionType` (`rtos/Commands.h`) ergänzen.
   Falls die Aktion zwingend eine Online-Spoolman-Verbindung braucht, auch
   in `requiresOnlineSpoolman()` (selbe Datei) eintragen -- das ist das
   zentrale Gate, das `AppTask.cpp` vor jeder Aktionsbehandlung prüft
   (`docs/workflows.md`, Abschnitt "Spoolman Offline Error Flow").
2. In `UiBridge.cpp` den auslösenden LVGL-Callback per `bindClick()`/
   passendem Event auf `sendAction(UiActionType::..., ...)` verdrahten.
3. In `AppTask.cpp`s `handleUiAction()`-`switch` (über `UiActionType`) einen
   neuen `case` ergänzen -- das ist der eigentliche Ort der Fachlogik.
   `UiAction` transportiert nur feste, generische Felder
   (`printerId`/`spoolId`/`amsId`/`trayId`/`value`/`text`); für zusätzliche
   Werte eines bestehenden Feldes wiederverwenden, kein neues Feld ad hoc
   einführen (die Struktur ist bewusst klein und trivial kopierbar
   gehalten).

## Task

Ein neuer Task lohnt sich für einen eigenständigen fachlichen
Zuständigkeitsbereich mit eigener Hardware/eigenem externen Dienst
(bestehendes Muster: ein Task pro Peripherie/Dienst, siehe
`docs/architecture.md`, Abschnitt "Tasks"). Anhand von `UpdateTask`
(TASKS.md Phase 13) als jüngstem Beispiel:

1. `TaskSettings`-Konstante in `config/TaskConfig.h` (Name, Stacktiefe,
   Priorität -- Startwert typischerweise analog zu einem bestehenden
   ähnlich belasteten Task, mit Kommentar zur Begründung).
2. Command-Queue-Feld und `TaskHandle_t`-Feld in `rtos/RtosContext.h`;
   Erzeugung in `createObjects()`/`createServiceTasks()`
   (`rtos/RtosContext.cpp`) -- `xQueueCreate(...)` bzw.
   `createTask(tasks::neuerTask, config::kNeuerTask, this, &neuerTask)`.
3. Kommando-Enum (`enum class ...CommandType`) und Kommando-Struct in
   `rtos/Commands.h`/`Messages.h`, inkl.
   `static_assert(std::is_trivially_copyable_v<...>)`.
4. Funktionsdeklaration in `tasks/Tasks.h`, Implementierung in einer neuen
   `tasks/<Name>Task.cpp` -- Grundform: Endlosschleife, die blockierend auf
   der eigenen Command-Queue wartet (`xQueueReceive(..., portMAX_DELAY)`)
   und Ergebnisse als `AppEvent` über `appEventQueue` zurückmeldet.
5. Neue `LogComponent` (siehe "Logger" oben), falls der Task eigenständig
   protokolliert.
6. `docs/architecture.md`s Tabellen (Tasks/Queues/Events) entsprechend
   ergänzen.

## JSON

Ein neues persistiertes Dokument (siehe `docs/storage.md` für die
bestehende Liste) braucht:

1. Neuen Wert in `enum class StorageDocumentType` (`rtos/Messages.h`).
2. In `services/JsonStorage.cpp`: einen `apply<Name>Defaults()` für den
   Standardinhalt bei fehlender Datei, sowie einen `validate<Name>(...)`
   für die Schema-Prüfung -- beide werden über je einen `switch`
   (`case StorageDocumentType::<Name>: ...`) an den zentralen
   Laden/Speichern-Pfaden eingehängt (mehrere `switch`-Stellen in dieser
   Datei, siehe die bestehenden Fälle als Vorlage).
3. Falls die Datei automatisch beim SD-Start angelegt werden soll: Eintrag
   in `kInitialDocuments` (`tasks/StorageTask.cpp`) mit Pfad und Typ --
   der Pfad muss in einem der in `kRequiredDirectories` (selbe Datei)
   gelisteten Verzeichnisse liegen.
4. Laden/Speichern läuft für Anwendungscode ausschließlich über
   `StorageCommand`/`storageCommandQueue` -- kein direkter Dateisystemzugriff
   außerhalb von `StorageTask` (siehe `docs/architecture.md`).

## Tagparser

Ein neues NFC-Tagformat wird als eigene Klasse implementiert, die
`nfc::ITagParser` erfüllt (`format()`, `canParse()`, `parse()` --
`nfc/ITagParser.h`):

1. Klasse in `nfc/TagParsers.h`/`.cpp` deklarieren/implementieren, analog zu
   einem bestehenden Parser (z. B. `OpenTag3DParser`) als Vorlage.
2. Instanz und Registrierung in `TagParserRegistry`s Default-Konstruktor
   (`nfc/TagParserRegistry.cpp`) ergänzen. **Reihenfolge ist relevant:**
   `parse()` fragt die registrierten Parser der Reihe nach, der erste mit
   `canParse() == true` gewinnt -- ein neuer, breit zugreifender
   `canParse()` vor einem bestehenden spezifischeren Parser kann diesen
   verdecken.
3. Neuen Wert in `enum class TagFormat` (`models/TagDefinition.h`).
4. Schreib-/Löschfähigkeit wird zentral in `TagParserRegistry::parse()`
   vergeben, nicht vom Parser selbst -- Version 1 gewährt das nur
   `FilamentStation` und (eingeschränkt) `Legacy`; ein neues Fremdformat
   bleibt also automatisch read-only, ohne dass der Parser das explizit
   erzwingen muss.
5. Formatdokumentation nach dem Muster von `docs/openprinttag.md`/
   `docs/opentag3d.md` ergänzen (Primärquelle, erkanntes NDEF-Muster,
   abgebildete Felder, Schreibschutz-Begründung).
6. Native Tests unter `test/test_tag_payload/` ergänzen, laufen über
   `pio test -e native-nfc-tests` (siehe "Tests" oben).

## Spoolman Extra Field

Das bestehende Muster für ein eigenes Spoolman-Extrafeld ist `tag`
(`SpoolmanClient::ensureTagExtraField()`, `services/SpoolmanClient.cpp`):
`GET /field/spool` prüft, ob das Feld bereits existiert und kompatibel ist;
fehlt es, legt `POST /field/spool/tag` es mit `{"name": ..., "field_type":
"text"}` neu an. Ein neues eigenes Feld nach demselben Muster ergänzen:
eigene `ensure...Field()`-Methode mit eigenem Feldschlüssel/-namen/-typ.

**Wichtige Falle:** Spoolman kodiert Extrafeld-Werte immer als JSON-String,
auch für Zahlen (`"220"` wird zu `"\"220\""` bzw. `"220.0"` zu
`"\"220.0\""`) -- deshalb existieren die beiden generischen Hilfsfunktionen
`decodeTextExtraField()`/`decodeNumberExtraField()` (`SpoolmanClient.h`)
für das Entpacken, statt jedes Feld einzeln naiv zu parsen.
