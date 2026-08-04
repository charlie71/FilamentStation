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

Jede Konfigurationsdatei enthaelt zusaetzlich `documentType`. Dadurch wird
beispielsweise eine versehentlich als `network.json` abgelegte Device-Datei
erkannt. Die ersten fachlichen Felder und Standardwerte sind:

| Datei | `documentType` | Pflichtfelder und Standardwerte |
|---|---|---|
| `/config/device.json` | `device` | `deviceName`: `"FilamentStation"` |
| `/config/network.json` | `network` | `hostname`: `"filamentstation"`, `dhcp`: `true` |
| `/config/spoolman.json` | `spoolman` | `enabled`: `false`, `serverUrl`: `""` |
| `/config/ui.json` | `ui` | `language`: `"de"`, `weightUnit`: `"g"` |
| `/config/scale.json` | `scale` | `calibrated`: `false` |
| `/config/nfc.json` | `nfc` | `tagSchemaVersion`: `1` |

`deviceName`, `hostname`, `language` und `weightUnit` muessen nichtleere
Zeichenketten sein. `dhcp`, `enabled` und `calibrated` sind boolesche Werte.
Wenn Spoolman aktiviert wird, darf `serverUrl` nicht leer sein.
`tagSchemaVersion` muss aktuell den Wert 1 besitzen.

Die Scale-Datei enthaelt absichtlich noch keinen erfundenen Kalibrierfaktor oder
GPIO-Wert. Spoolman enthaelt keine Zugangsdaten. Netzwerkparameter enthalten
kein WLAN-Passwort; dieses bleibt wie vorgesehen im ESP32-/WiFiManager-
Systembereich. Weitere Felder werden erst zusammen mit der jeweiligen
Funktionsphase definiert und validiert.

## Vorlaeufige Groessenlimits

| Dokumenttyp | Maximum |
|---|---:|
| Scale | 4 KiB |
| Device, Network, Spoolman, Bambu | 8 KiB |
| UI, NFC | 16 KiB |
| Diagnostics | 32 KiB |

Die Grenzen werden spaeter gegen die realen Schemas und Messdaten geprueft.
