# Speicherung

## Verzeichnisstruktur

Der StorageTask legt nach erfolgreichem SD-Mount folgende Verzeichnisse im
Wurzelverzeichnis an:

```text
/config
/cache
/queue
/mappings
/diagnostics
/logs
```

Jeder Pfad wird nach dem Oeffnen beziehungsweise Erzeugen darauf geprueft, dass
er tatsaechlich ein Verzeichnis ist. Existiert an einem erforderlichen Pfad
eine Datei oder kann das Verzeichnis nicht erzeugt werden, bleibt
`EVENT_SD_READY` geloescht und der Fehler wird bis zum Neustart verriegelt.

Phase 2.2 erzeugt keine Dateien und implementiert noch keine JSON-Verarbeitung.

Nach erfolgreicher Strukturpruefung protokolliert der StorageTask den Kartentyp,
die physische Kartenkapazitaet sowie Gesamt-, Belegt- und Freispeicher des
Dateisystems. Die Werte werden in Bytes und abgerundeten MiB ausgegeben.

Der Hardwaretest vom 2026-08-03 bestaetigte, dass alle sechs Verzeichnisse auf
der eingesetzten SD-Karte erzeugt beziehungsweise als Verzeichnisse validiert
wurden.

## JSON-Grundfunktionen

`JsonStorage` kann JSON laden, die Groessengrenze vor dem Parsen pruefen,
Standardmetadaten einsetzen, Schema-Version und UTC-Zeitstempel validieren sowie
ein validiertes Dokument serialisieren. Strukturierte Fehlercodes unterscheiden
unter anderem fehlende Dateien, Groessenverletzungen, Lesefehler, JSON-
Parserfehler, ungueltige Metadaten und Fehler der atomaren Transaktion.

Atomisches Speichern verwendet fuer ein Ziel wie `/config/scale.json` die
Dateien `/config/scale.tmp.json` und `/config/scale.bak.json`. Das neue Dokument
wird zuerst in die temporaere Datei geschrieben, geflusht, geschlossen und nach
erneutem Oeffnen validiert. Eine vorhandene Zieldatei wird danach als Backup
umbenannt. Erst dann wird die temporaere Datei zum Ziel. Nach erfolgreicher
Validierung des Ziels wird das Backup entfernt.

`recoverAtomicSave()` behandelt unterbrochene Transaktionen deterministisch:
Ein gueltiges Ziel gewinnt und veraltete Hilfsdateien werden entfernt. Fehlt ein
gueltiges Ziel, wird zuerst eine gueltige temporaere Datei uebernommen, andernfalls
ein gueltiges Backup wiederhergestellt. Sind vorhandene Kandidaten alle
ungueltig, wird ein strukturierter Wiederherstellungsfehler geliefert.

Die Dateisystemmethoden von `JsonStorage` duerfen ausschliesslich aus dem
`StorageTask` aufgerufen werden. Damit bleibt dieser Task alleiniger Besitzer der
SD-Karte.

## Storage-Queue

Lese- und Schreibanfragen werden als `StorageCommand` an die
`storageCommandQueue` gesendet. Der StorageTask prueft, dass der Pfad absolut,
frei von `..`, eine JSON-Datei und einem der verwalteten Verzeichnisse
zugeordnet ist. `LoadJson` oeffnet und validiert die Datei. `SaveJson` parst den
begrenzten Nachrichtenpuffer und verwendet danach den atomaren Schreibablauf.

Erfolg und Fehler werden mit unveraenderter `requestId` an die
`appEventQueue` gemeldet. Eine erfolgreiche Leseantwort enthaelt in `value` die
validierte Dateigroesse; eine erfolgreiche Schreibantwort die geschriebenen
Bytes. Ein Fehlerereignis enthaelt den strukturierten `JsonStorageError`-Wert.
Das vollstaendige geladene Dokument bleibt in dieser Phase im StorageTask, weil
die fachlichen, typisierten Datenmodelle noch nicht definiert sind.

Die Queue ist FIFO und der StorageTask beendet jede Operation, bevor er die
naechste Anfrage annimmt. Bei einer entfernten oder beim Start fehlenden Karte
werden Anfragen explizit abgelehnt und niemals als erfolgreich gemeldet.

## Gewichtsaktualisierungen

Quick- und Advanced-Weight-Messungen werden ausschliesslich bei aktiver
Spoolman-Verbindung uebertragen. Schlaegt die Anfrage fehl, wird keine lokale
Warteschlange angelegt und kein automatischer Wiederholungsversuch gestartet.
Die GUI meldet den Fehler unmittelbar; der Benutzer kann den Wiegevorgang
manuell erneut ausfuehren.

Fruehere Firmwarestaende konnten `/queue/pending-weight.json` beziehungsweise
`/queue/pending-measurements.json` erzeugen. Der AppTask beauftragt den
StorageTask beim SD-Start einmalig mit dem Entfernen dieser Altdateien. Sie
werden weder geladen noch als Offline-Datenquelle verwendet.

## Spoolman-Daten

Spulen, Filamente und Hersteller werden ausschliesslich online vom
SpoolmanTask geladen. Es gibt keinen persistenten Spoolman-Cache und keinen
SD-basierten Offline-Fallback. Das weiterhin vorhandene Verzeichnis `/cache`
enthaelt keine autoritative Spoolman-Kopie.

Zur Bereinigung bestehender Installationen entfernt der StorageTask im Auftrag
des AppTask beim SD-Start die frueher vorgesehenen Dateien
`/cache/spools.json`, `/cache/filaments.json` und `/cache/vendors.json`. Diese
Dateien werden nicht gelesen oder ausgewertet.

Auch ein fachlicher RAM-Cache wird derzeit bewusst nicht verwendet. Die
Entscheidung und die Abgrenzung zum flüchtigen UI-View-Zustand sind unter
[`decisions/ram-cache.md`](decisions/ram-cache.md) dokumentiert.

## Erste Konfigurationsdateien

Nach Mount und Verzeichnispruefung stellt der StorageTask die sieben Dateien
`device.json`, `network.json`, `spoolman.json`, `bambu.json`, `ui.json`,
`scale.json` und `nfc.json` unter `/config` sowie zusaetzlich
`/mappings/printer-slots.json` bereit (`kInitialDocuments`,
`StorageTask.cpp:30`). Fehlt eine Datei, wird ihr dokumentierter
Standardinhalt atomar geschrieben und erneut validiert. Eine vorhandene gueltige
Datei bleibt unveraendert. Vorhandene Temp- oder Backup-Dateien durchlaufen die
Wiederherstellungslogik aus Phase 2.4.

## Drucker/Fach->Spule-Zuordnung (`/mappings/printer-slots.json`)

Nutzerwunsch vom 2026-08-24: bildet je Drucker/AMS/Fach die zuletzt bestaetigte
Spoolman-`spoolId` ab, zusammen mit dem `material`/`colorHex`, das der Drucker
zum Zuordnungszeitpunkt gemeldet hat (`models::TraySpoolCache`,
`models/TraySpoolCache.h`). Ersetzt einen frueheren Versuch, dieselbe
Zuordnung ueber ein eigenes MQTT-Feld im Drucker selbst zu speichern -- ein
Hardwaretest zeigte, dass der Drucker das nicht dauerhaft haelt (siehe
`docs/bambu-protocol.md`). Deshalb wird die Zuordnung stattdessen lokal auf
der SD-Karte gehalten.

Diese Datei liegt zwar im selben `/mappings`-Verzeichnis wie die drei
obsoleten NFC-Migrationsdateien (`docs/legacy-and-unknown-tags.md`), hat
aber nichts mit NFC-Tag-Identitaeten zu tun -- sie bildet ausschliesslich
Drucker-Fach-Positionen auf Spoolman-Spulen ab. `StorageTask.cpp`s
`isMappingPath()` (fuer die einmalige NFC-Legacy-Migration) prueft deshalb
gezielt nur die drei NFC-Mapping-Pfade und schliesst
`/mappings/printer-slots.json` nicht mit ein.

Geschrieben wird bei jeder erfolgreich vom Drucker bestaetigten
Fachzuordnung beziehungsweise -entfernung (`AppTask.cpp`,
`persistTraySpoolCache()`) -- Fire-and-forget ohne Dialog/Pending-State:
schlaegt das Speichern fehl, ist die Assoziation einfach noch nicht
dauerhaft und wird bei der naechsten erfolgreichen Zuordnung erneut
versucht. Beim Lesen (`resolveTraySpoolCacheSpoolId()`) wird ein Eintrag nur
verwendet, wenn das aktuell vom Drucker gemeldete `material`/`colorHex`
noch mit dem beim Speichern erfassten Stand uebereinstimmt -- eine
Abweichung bedeutet, dass die physische Spule ausserhalb dieser App
gewechselt wurde, und die Zuordnung gilt dann als unbekannt (Anzeige "?").

Das ist bewusst **keine** Ausnahme von "kein persistenter
Offline-Spoolman-Cache" (siehe Abschnitt "Spoolman-Daten" unten): persistiert
wird ausschliesslich die Identitaets-Assoziation (welche `spoolId` gehoert zu
diesem Fach), keine Spoolman-Stammdaten. Restgewicht und K-Faktor der so
identifizierten Spule werden weiterhin bei jedem Start frisch von Spoolman
geladen und nicht auf der SD-Karte gehalten (`traySpoolDetails` in
`AppTask.cpp`, reiner RAM-Cache).

Nach erfolgreichem SD-Mount fordert der AppTask die Netzwerkdatei an. Nur der
StorageTask liest und validiert sie; anschließend sendet er eine wertbasierte
`NetworkSettings`-Nachricht an den AppTask. Dieser reicht die Konfiguration an
den NetworkTask weiter. Damit greifen weder AppTask noch NetworkTask direkt auf
die SD-Karte zu.

Ist eine vorhandene Datei beschaedigt und nicht wiederherstellbar, wird sie
nicht ueberschrieben. Der StorageTask meldet den Fehler, laesst
`EVENT_SD_READY` geloescht und verlangt entsprechend der SD-Fehlerstrategie
einen Neustart.

## Bambu-Material-Mapping (`/config/bambu_materials.json`)

Nutzerwunsch vom 2026-08-28: bildet Spoolman-Materialtexte (z. B. `PLA`,
`PLA-CF`) auf Bambus AMS-Profil (`tray_info_idx`/`tray_type`/
`nozzle_temp_min`/`nozzle_temp_max`) ab -- fruehher eine fest kompilierte
Tabelle in `src/services/BambuProtocol.cpp`, jetzt eine JSON-Datei, damit
neue Materialien ohne Firmware-Neukompilierung ergaenzt werden koennen
(siehe `docs/bambu-protocol.md` fuer das vollstaendige Schema).

Diese Datei ist **kein** `kInitialDocuments`-Eintrag (siehe oben) -- es
gibt bewusst keine automatisch erzeugte Default-Datei, und sie nutzt
**nicht** `JsonStorage`s Envelope/Validator (`schemaVersion`/`updatedAt`/
`documentType`), sondern ein eigenes, im Auftrag vorgegebenes Schema
(`schema_version`/`materials[]`, siehe `services::BambuMaterialCatalog`).
Die `.tmp.json`/`.bak.json`-Namenskonvention wird trotzdem uebernommen
(`config::kBambuMaterialsTempPath`/`kBambuMaterialsBackupPath`) -- gleiche
Optik auf der SD-Karte wie jedes andere Dokument, auch wenn die
Aktivierungslogik eigenstaendig implementiert ist (`StorageTask.cpp::
activateBambuMaterialFile()`), da `JsonStorage::atomicSave()`s Validator
dieses Schema nicht versteht.

Geladen wird einmal beim Boot (`loadBambuMaterialCatalog()`, direkt nach
den `kInitialDocuments`) und erneut nach einem erfolgreich aktivierten
Download (`StorageCommandType::CommitBambuMaterialDownload`) -- die
geparste Tabelle wird **nicht** ueber eine Queue transportiert (zu gross
fuer `AppEvent`, siehe `docs/architecture.md`), sondern per atomarem
Zeiger `RtosContext::bambuMaterialMappings` veroeffentlicht. Fehlt die
Datei oder ist sie ungueltig, bleibt dieser Zeiger `nullptr` -- es gibt
bewusst **keinen** Fallback auf eine fest kompilierte Tabelle.

Der Laufzeit-Download (`UpdateCommandType::DownloadBambuMaterials`) nutzt
denselben GitHub-Release-Mechanismus wie das Firmware-Update, schreibt
aber **nie** selbst auf die SD-Karte (nur `StorageTask` darf das) --
`UpdateTask` streamt die HTTPS-Antwort in `StorageCommand.json`-grossen
Haeppchen (`StorageCommandType::BeginBambuMaterialDownload`/
`WriteBambuMaterialChunk`/`CommitBambuMaterialDownload`/
`AbortBambuMaterialDownload`) an `StorageTask`, das die geschriebene
`.tmp.json` selbst per SHA-256 verifiziert, parst/validiert und erst dann
atomar aktiviert. Details siehe `docs/bambu-protocol.md`.

Die Unit-Tests fuer Standardwerte, Objektwurzel, Schema-Version, Zeitstempel und
Serialisierung lassen sich fuer das ESP32-S3-Ziel kompilieren. Der Lauf vom
2026-08-03 wurde wegen eines durch einen anderen Prozess belegten COM4-Ports
nicht auf der Hardware ausgefuehrt; PlatformIO meldete deshalb null ausgefuehrte
Testfaelle.
