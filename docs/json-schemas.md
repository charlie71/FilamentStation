# JSON-Schemas

## Gemeinsame Dokumenthuelle

Phase 2.3 definiert die gemeinsame, derzeit unterstuetzte Schema-Version 1:

```json
{
  "schemaVersion": 1,
  "updatedAt": "2026-08-03T12:00:00Z"
}
```

Der Dokumentwurzelknoten muss ein JSON-Objekt sein. `schemaVersion` muss eine
positive Ganzzahl mit dem Wert 1 sein. `updatedAt` verwendet das feste
UTC-Format `YYYY-MM-DDTHH:MM:SSZ`. Fehlende Metadaten erhalten beim Laden die
Standardwerte `1` und `1970-01-01T00:00:00Z`; der Epoch-Zeitpunkt kennzeichnet
dabei einen noch nicht fachlich aktualisierten Datensatz.

Die fachlichen Pflichtfelder und Standardwerte werden mit den konkreten
Dokumentmodellen festgelegt. Phase 2.3 erfindet dafuer noch keine Datenfelder.

## Vorlaeufige Groessenlimits

| Dokumenttyp | Maximum |
|---|---:|
| Scale | 4 KiB |
| Device, Network, Spoolman, Bambu | 8 KiB |
| UI, NFC | 16 KiB |
| Diagnostics | 32 KiB |

Die Grenzen werden spaeter gegen die realen Schemas und Messdaten geprueft.
