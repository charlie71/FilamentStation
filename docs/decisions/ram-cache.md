# Entscheidung: Kein Spoolman-RAM-Cache

Stand: 2026-08-19

## Entscheidung

FilamentStation verwendet vorerst keinen fachlichen RAM-Cache für Spoolman-
Spulen, Filamente oder Hersteller. Jede Suche, Detailabfrage und Änderung wird
online über den SpoolmanTask gegen den verbundenen Server ausgeführt.

Die maximal 20 Einträge des Spulen-Auswahldialogs sind ausschließlich der
flüchtige View-Zustand der aktuell angezeigten Serverantwort. Sie werden nicht
taskübergreifend als Datenquelle angeboten, nicht nach einem Neustart erhalten
und nicht als Offline-Fallback verwendet. Eine Auswahl wird weiterhin durch
die Online-Prüfung im AppTask geschützt.

## Grenzen

- TTL: entfällt, weil keine Daten wiederverwendet werden.
- Maximale Cachegröße: null Einträge.
- Write-Invalidierung: entfällt; die nächste Abfrage lädt wieder vom Server.
- Disconnect: Es existiert kein wiederverwendbarer Cache, der verworfen werden
  müsste.
- Offline-Betrieb: Der sichtbare UI-Zustand berechtigt niemals zu einem
  Spoolman-Workflow ohne `EVENT_SPOOLMAN_READY`.

Ein RAM-Cache darf später nur mit expliziter TTL, harter Größenbegrenzung,
Invalidierung nach jeder Schreiboperation und vollständigem Verwerfen beim
Disconnect eingeführt werden. Er darf niemals eine Offline-Datenquelle sein.

## Begründung

Die derzeitigen Datenmengen und Abläufe benötigen keinen Cache. Ein Cache würde
zusätzliche Stale-Data- und Invalidierungsfälle erzeugen, ohne einen aktuell
nachgewiesenen Nutzen zu liefern. Der direkte Onlinezugriff hält Spoolman klar
als einzige Quelle der Wahrheit.
