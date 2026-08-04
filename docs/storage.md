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

`JsonStorage` kann JSON aus einem bereits vom StorageTask geoeffneten `File`
laden, die Groessengrenze vor dem Parsen pruefen, Standardmetadaten einsetzen,
Schema-Version und UTC-Zeitstempel validieren sowie ein validiertes Dokument in
ein `Print`-Ziel serialisieren. Strukturierte Fehlercodes unterscheiden unter
anderem fehlende Dateien, leere oder zu grosse Dokumente, Lesefehler,
Parserfehler, ungueltige Metadaten und Serialisierungsfehler.

Der Dienst oeffnet selbst keine SD-Pfade. Damit bleibt der direkte SD-Zugriff
beim StorageTask. Atomisches Schreiben und Backups gehoeren zu Phase 2.4 und
sind noch nicht implementiert.

Die Unit-Tests fuer Standardwerte, Objektwurzel, Schema-Version, Zeitstempel und
Serialisierung lassen sich fuer das ESP32-S3-Ziel kompilieren. Der Lauf vom
2026-08-03 wurde wegen eines durch einen anderen Prozess belegten COM4-Ports
nicht auf der Hardware ausgefuehrt; PlatformIO meldete deshalb null ausgefuehrte
Testfaelle.
