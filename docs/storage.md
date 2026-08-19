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

Nach Mount und Verzeichnispruefung stellt der StorageTask die sechs Dateien
`device.json`, `network.json`, `spoolman.json`, `ui.json`, `scale.json` und
`nfc.json` unter `/config` bereit. Fehlt eine Datei, wird ihr dokumentierter
Standardinhalt atomar geschrieben und erneut validiert. Eine vorhandene gueltige
Datei bleibt unveraendert. Vorhandene Temp- oder Backup-Dateien durchlaufen die
Wiederherstellungslogik aus Phase 2.4.

Nach erfolgreichem SD-Mount fordert der AppTask die Netzwerkdatei an. Nur der
StorageTask liest und validiert sie; anschließend sendet er eine wertbasierte
`NetworkSettings`-Nachricht an den AppTask. Dieser reicht die Konfiguration an
den NetworkTask weiter. Damit greifen weder AppTask noch NetworkTask direkt auf
die SD-Karte zu.

Ist eine vorhandene Datei beschaedigt und nicht wiederherstellbar, wird sie
nicht ueberschrieben. Der StorageTask meldet den Fehler, laesst
`EVENT_SD_READY` geloescht und verlangt entsprechend der SD-Fehlerstrategie
einen Neustart.

Die Unit-Tests fuer Standardwerte, Objektwurzel, Schema-Version, Zeitstempel und
Serialisierung lassen sich fuer das ESP32-S3-Ziel kompilieren. Der Lauf vom
2026-08-03 wurde wegen eines durch einen anderen Prozess belegten COM4-Ports
nicht auf der Hardware ausgefuehrt; PlatformIO meldete deshalb null ausgefuehrte
Testfaelle.
