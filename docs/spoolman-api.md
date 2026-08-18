# Spoolman API

## Konfiguration und Verbindungstest

Die normalisierte Basis-URL und das HTTP-Timeout werden in
`/config/spoolman.json` gespeichert. Der SpoolmanTask fuehrt den
Verbindungstest im Network-Kontext aus und fragt nacheinander
`/api/v1/health` sowie `/api/v1/info` ab. Nur erfolgreiche, als JSON lesbare
Antworten setzen `EVENT_SPOOLMAN_READY`; die Versionsnummer aus `info` wird in
der GUI angezeigt.

Spulenabfragen und Aenderungsoperationen gehoeren zu Phase 7.2 und sind noch
nicht implementiert.
