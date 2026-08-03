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
