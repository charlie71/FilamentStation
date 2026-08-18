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
| `/config/network.json` | `network` | `hostname`: `"filamentstation"`, `dhcp`: `true`, `ipAddress`: `""`, `gateway`: `""`, `subnetMask`: `""`, `dns`: `""`, `portalName`: `"FilamentStation"`, `portalTimeoutSeconds`: `180`, `connectTimeoutSeconds`: `20` |
| `/config/spoolman.json` | `spoolman` | `enabled`: `false`, `serverUrl`: `""` |
| `/config/ui.json` | `ui` | `language`: `"de"`, `weightUnit`: `"g"` |
| `/config/scale.json` | `scale` | `calibrated`: `false`, `tareOffsetCounts`: `0`, `factorCountsPerGram`: `1.0` |
| `/config/nfc.json` | `nfc` | `tagSchemaVersion`: `1` |

`deviceName`, `hostname`, `language` und `weightUnit` muessen nichtleere
Zeichenketten sein. `dhcp`, `enabled` und `calibrated` sind boolesche Werte.
Wenn Spoolman aktiviert wird, darf `serverUrl` nicht leer sein.
`tagSchemaVersion` muss aktuell den Wert 1 besitzen.

Die Scale-Datei speichert den zuletzt tarierten HX711-Rohwert und den bei der
Referenzkalibrierung ermittelten Faktor in Counts pro Gramm. Bei
`calibrated: true` darf der Faktor nicht null sein. Aeltere Scale-Dateien aus
Schema-Version 1 erhalten beim Laden kompatible Standardwerte. GPIO-Werte
bleiben weiterhin ausschliesslich in `BoardConfig.h`.

Spoolman enthaelt keine Zugangsdaten. Netzwerkparameter enthalten
kein WLAN-Passwort; dieses bleibt wie vorgesehen im ESP32-/WiFiManager-
Systembereich.

## Netzwerkparameter

`hostname` ist 1 bis 32 Zeichen lang und besteht aus Buchstaben, Ziffern und
Bindestrichen; ein Bindestrich darf nicht am Anfang oder Ende stehen. Bei
`dhcp: true` bleiben `ipAddress`, `gateway` und `subnetMask` leer und der ESP32
bezieht seine Parameter automatisch. Bei `dhcp: false` muessen diese drei
Felder gueltige IPv4-Adressen enthalten. `dns` ist optional; ein nichtleerer
Wert muss ebenfalls eine IPv4-Adresse sein.

`portalName` ist der lesbare, maximal 25 Zeichen lange Namensanteil des
Captive-Portals. Der NetworkTask haengt einen Bindestrich und sechs
hexadezimale Zeichen aus der Chip-ID an, sodass die WLAN-SSID eindeutig bleibt
und das 32-Zeichen-Limit nicht ueberschreitet. `portalTimeoutSeconds` liegt
zwischen 30 und 900 Sekunden, `connectTimeoutSeconds` zwischen 1 und 60
Sekunden.

Beispiel fuer DHCP:

```json
{
  "schemaVersion": 1,
  "updatedAt": "1970-01-01T00:00:00Z",
  "documentType": "network",
  "hostname": "filamentstation",
  "dhcp": true,
  "ipAddress": "",
  "gateway": "",
  "subnetMask": "",
  "dns": "",
  "portalName": "FilamentStation",
  "portalTimeoutSeconds": 180,
  "connectTimeoutSeconds": 20
}
```

## Vorlaeufige Groessenlimits

| Dokumenttyp | Maximum |
|---|---:|
| Scale | 4 KiB |
| Device, Network, Spoolman, Bambu | 8 KiB |
| UI, NFC | 16 KiB |
| Diagnostics | 32 KiB |

Die Grenzen werden spaeter gegen die realen Schemas und Messdaten geprueft.
