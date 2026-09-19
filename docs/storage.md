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

## Absturzdiagnose (`/diagnostics/coredump.json`, `/diagnostics/coredump_<slot>.bin`)

Nutzerwunsch vom 2026-09-03: verdichtet einen von der ESP-IDF bereits
automatisch in einer eigenen Flash-Partition abgelegten Coredump zu zwei
Zaehlern, die auf dem Diagnose-Bildschirm angezeigt werden, und sichert
zusaetzlich eine Rohkopie des Coredumps selbst auf der SD-Karte.

```json
{
  "schemaVersion": 1,
  "updatedAt": "1970-01-01T00:00:00Z",
  "documentType": "diagnostics",
  "totalBootCount": 42,
  "coredumpCount": 3,
  "bootCountAtLastCoredump": 39
}
```

* `totalBootCount` -- bei jedem Boot +1.
* `coredumpCount` -- nur erhoeht, wenn beim Boot tatsaechlich ein Coredump
  in Flash gefunden wurde.
* `bootCountAtLastCoredump` -- `totalBootCount`-Wert zum Zeitpunkt des
  letzten gefundenen Coredumps; daraus ergibt sich "Neustarts seit dem
  letzten Absturz" als `totalBootCount - bootCountAtLastCoredump`. Kein
  echter Zeitstempel (`updatedAt` bleibt der uebliche Platzhalter) -- diese
  Firmware hat keinerlei Uhrzeit-/Datumsquelle (weder NTP noch RTC).

### Woher der Coredump kommt, und wie er auf die SD-Karte kommt

`/diagnostics/coredump.json` ist nicht der Coredump selbst, sondern nur
seine Kurzzusammenfassung. Der eigentliche Coredump liegt zunaechst in
einer eigenen, von `default_16MB.csv` reservierten Flash-Partition (Typ
`data`, Subtyp `coredump`, 64 KiB) -- vom Arduino-ESP32-Kern selbst
geschrieben (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` ist fuer dieses Board
bereits aktiviert), sobald eine echte Exception/Panic auftritt.
`main.cpp::setup()` prueft noch vor dem ersten Taskstart per
`esp_core_dump_image_check()`/`esp_core_dump_get_summary()`, ob ein Fund
vorliegt, und loggt Task-Name, Programmzaehler und die rohe
Xtensa-Exception-Ursachennummer einmalig per `FS_LOGE`
(Log-Komponente `Rtos`).

**Nachtrag (4), direkt im Anschluss:** ein erster Entwurf dieser Funktion
liess den rohen Coredump ausschliesslich in der Flash-Partition liegen
und loeschte sie nach dem Speichern der Zaehler-Datei -- das gab dem
Nutzer zurecht als unpraktisch zurueckgemeldet: ein Absturz, der das
Geraet automatisch neu startet, laesst kaum ein Zeitfenster fuer einen
manuellen USB-Zugriff vor dem naechsten, die Partition loeschenden Boot.
`AppTask` kopiert den rohen Coredump deshalb jetzt zuerst auf die
SD-Karte, sobald `ctx.pendingCoredump.found` gesetzt ist:
`esp_core_dump_image_get()` liefert Flash-Adresse und tatsaechliche
Groesse des validierten Coredumps (nicht die volle 64-KiB-
Partitionsgroesse, sondern die im Coredump-eigenen Header vermerkte
echte Laenge), `exportPendingCoredumpToSd()` liest ihn in
768-Byte-Haeppchen per `spi_flash_read()` und streamt sie ueber die neuen
`StorageCommandType::BeginCoredumpExport`/`WriteCoredumpChunk`/
`CommitCoredumpExport`/`AbortCoredumpExport`-Befehle an `StorageTask`
(gleiches Haeppchen-Streaming-Muster wie der Bambu-Material-Download,
aber ohne dessen SHA-256-Pruefung/Aktivierungslogik -- eine reine
Binaerkopie braucht keine Schema-Validierung). Ziel ist eine rotierende
Datei `/diagnostics/coredump_<slot>.bin` mit `slot` = 1 bis 10
(`((coredumpCount - 1) % 10) + 1`) -- bei mehr als zehn Abstuerzen wird
die aelteste Slot-Datei einfach ueberschrieben, sodass die letzten zehn
Abstuerze immer nachvollziehbar bleiben, ohne die SD-Karte unbegrenzt
wachsen zu lassen.

Erst **nachdem** `/diagnostics/coredump.json` erfolgreich gespeichert
wurde, loescht `AppTask` die Flash-Partition
(`esp_core_dump_image_erase()`) -- schlaegt das Speichern fehl (z. B.
keine SD-Karte eingesetzt), bleibt der Coredump in der Partition
erhalten und wird beim naechsten Boot erneut ausgewertet statt verloren
zu gehen. Der SD-Export selbst ist bewusst **kein** Kriterium fuer diese
Entscheidung: er ist ein Best-Effort-Zusatz, kein Korrektheits-
Erfordernis. Schlaegt er fehl (siehe `StorageWriteCompleted`/
`StorageRequestError` fuer `kCoredumpExportRequestId`, nur geloggt),
wird der Absturz trotzdem korrekt gezaehlt und die Flash-Partition
trotzdem fuer den naechsten Absturz freigegeben -- nur die zusaetzliche
Rohkopie auf der SD-Karte fehlt dann fuer diesen einen Fall.

### Detailanalyse

Fuer eine grobe Einordnung genuegt oft schon die einzelne `FS_LOGE`-Zeile
aus dem seriellen Log (Task-Name plus Programmzaehler) zusammen mit
`xtensa-esp32s3-elf-addr2line -e firmware.elf <adresse>` (die Toolchain
liegt als PlatformIO-Paket bereits lokal vor) -- loest die einzelne
Adresse in Datei/Zeile auf.

Fuer eine vollstaendige Analyse (Backtrace mit Datei/Zeile, Stacks aller
Tasks, Register) die jetzt auf der SD-Karte gesicherte `coredump_<slot>.bin`
verwenden -- kein USB-Zugriff zum Absturzzeitpunkt mehr noetig, die Datei
kann jederzeit spaeter per SD-Kartenleser abgeholt werden:

1. Am PC: `pip install esp-coredump` (separates Python-Paket, nicht Teil
   dieses Projekts).
2. `esp-coredump info_corefile --core coredump_3.bin --core-format raw
   --prog firmware.elf` ausfuehren (Kurzform des Kernbefehls -- exakte
   Flag-Namen ueber `esp-coredump --help` pruefen, in dieser Umgebung
   nicht gegen ein reales Geraet verifiziert). `firmware.elf` muss
   zwingend exakt die Version sein, die zum Absturzzeitpunkt lief
   (`.pio/build/wt32-s3-wrover-n16r2/firmware.elf` des entsprechenden
   Builds, oder das Release-Artefakt derselben Version) -- bei einer
   abweichenden Version sind aufgeloeste Symbole/Zeilen falsch oder
   fehlen ganz.
3. Ergebnis ist ein vollstaendiger GDB-Backtrace mit Datei/Zeile, Register
   und den Stacks aller zum Absturzzeitpunkt aktiven Tasks.

Alternativ funktioniert weiterhin auch der direkte Live-Zugriff auf die
Flash-Partition per USB (`esp-coredump info_corefile --port COM5
--core-format elf firmware.elf`, ohne `--core`) -- nur eben nur, solange
die Partition noch nicht durch den naechsten erfolgreichen Boot geloescht
wurde. Fuer den ueblichen, unbeaufsichtigten Betrieb ist die gesicherte
`.bin`-Datei auf der SD-Karte der praktikablere Weg.

Die Unit-Tests fuer Standardwerte, Objektwurzel, Schema-Version, Zeitstempel und
Serialisierung lassen sich fuer das ESP32-S3-Ziel kompilieren. Der Lauf vom
2026-08-03 wurde wegen eines durch einen anderen Prozess belegten COM4-Ports
nicht auf der Hardware ausgefuehrt; PlatformIO meldete deshalb null ausgefuehrte
Testfaelle.
