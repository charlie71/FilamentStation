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
SD-Karte; die Einbindung in die Storage-Queue erfolgt erst in Phase 2.5.

Die Unit-Tests fuer Standardwerte, Objektwurzel, Schema-Version, Zeitstempel und
Serialisierung lassen sich fuer das ESP32-S3-Ziel kompilieren. Der Lauf vom
2026-08-03 wurde wegen eines durch einen anderen Prozess belegten COM4-Ports
nicht auf der Hardware ausgefuehrt; PlatformIO meldete deshalb null ausgefuehrte
Testfaelle.
