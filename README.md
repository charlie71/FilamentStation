# FilamentStation

FilamentStation ist die Firmware für eine eigenständige
Filamentverwaltungsstation: ein Touchscreen-Gerät auf Basis eines ESP32-S3
(WT32-SC01 Plus), das Filamentspulen per NFC/RFID identifiziert, sie wiegt
und ihre Zuordnung zentral in [Spoolman](https://github.com/Donkie/Spoolman)
verwaltet -- inklusive direkter Slot-Zuordnung auf mehreren Bambu-Lab-3D-
Druckern. Ziel ist, den bisher manuellen Weg "Spule identifizieren → Restgewicht
prüfen → in Spoolman und am Drucker eintragen" auf ein Antippen zu reduzieren.

Die Firmware basiert auf Arduino/FreeRTOS; das UI ist mit
[LVGL](https://lvgl.io/) und [EEZ Studio](https://www.envox.eu/eez-studio/)
gebaut.

## Core Features

- **NFC/RFID-Erkennung** eigener FilamentStation-Tags sowie Lesesupport für
  original Bambu-Lab-RFID, OpenPrintTag, OpenTag3D und ein älteres
  Legacy-Format -- Fremdformate bleiben grundsätzlich read-only.
- **Spoolman-Integration**: Spulensuche, Zuordnung/Entfernung ausschließlich
  über Spoolmans `extra.tag`-Feld (keine eigene lokale Zuordnungsdatenbank),
  automatische Extrafeld-Einrichtung, Gewichtsaktualisierung.
- **Waage** (HX711): Messen, Tarieren, Kalibrierung mit bekanntem
  Referenzgewicht.
- **Bambu-Lab-Mehrdruckerunterstützung** über das lokale LAN-MQTT-Protokoll:
  Verbindung zu mehreren Druckern, AMS-/Tray-Übersicht, Slot-Zuordnung direkt
  vom Gerät aus, lokal zwischengespeicherte Drucker-Fach-Zuordnung.
- **Energiesparen**: Aktiv-/Gedimmt-/Sleep-Statemachine mit Touch-Wake.
- **Firmware-Update über GitHub Releases**: Versionscheck, Download mit
  SHA-256-Verifikation, App-Rollback-Absicherung -- direkt am Gerät auslösbar.
- **SD-Karten-Speicherung** aller Konfigurationen mit atomarem Schreiben und
  automatischem Backup/Wiederherstellung.

Der vollständige, aktuelle Funktionsumfang steht in [`CHANGELOG.md`](CHANGELOG.md).

## Schnellstart

**Fertige Hardware, Firmware drauf?** →
**[Firmware im Browser flashen](https://charlie71.github.io/FilamentStation/)**
-- direkt über [ESP Web Tools](https://esphome.github.io/esp-web-tools/),
keine Installation nötig (Chrome/Edge/Opera am Desktop, per USB
verbunden). Dieselbe Seite bietet auch `bambu_materials.json` zum
Download an. Ein 3D-druckbares Gehäuse steht in
[`docs/hardware.md`](docs/hardware.md#gehäuse).

**Von Quellcode bauen:** Voraussetzung ist eine aktuelle
PlatformIO-Installation.

```text
pio run -e wt32-s3-wrover-n16r2 -t upload
```

Details zu Build, Upload, Tests und dem Erweitern der Firmware (neuer
Screen/Task/Tagparser/…) stehen im [Entwicklerhandbuch](docs/developer-guide.md).
Für die Bedienung des fertigen Geräts (Ersteinrichtung, WLAN, Spoolman,
Drucker hinzufügen, Firmware-Update, …) siehe die
[Benutzeranleitung](docs/user-guide.md).

## Dokumentation

**Für Nutzer:**

- [Benutzeranleitung](docs/user-guide.md) -- Installation, WLAN, Spoolman,
  Waage, NFC, Drucker, AMS, Firmware-Update.

**Für Entwickler:**

- [Entwicklerhandbuch](docs/developer-guide.md) -- Build, Upload, Tests, EEZ
  Export, und wie ein neuer Screen/Action/Task/JSON-Dokument/Tagparser/
  Spoolman-Extrafeld ergänzt wird.
- [Architektur](docs/architecture.md) -- Tasks, Queues, Events, IRQ,
  Prioritäten, Stacks.
- [FreeRTOS-Grundarchitektur](docs/rtos.md), [Logging](docs/logging.md),
  [Speicherung](docs/storage.md), [UI-Workflows](docs/workflows.md),
  [Tag-Identität/Capabilities/Spoolman-Zuordnung](docs/tag-identity.md).
- [Hardware](docs/hardware.md) -- Controller, Verdrahtung, GPIO-Übersicht,
  Bill of Materials.
- NFC-Tagformate: [eigenes Format](docs/nfc-tags.md),
  [Bambu-RFID](docs/bambu-rfid.md), [OpenPrintTag](docs/openprinttag.md),
  [OpenTag3D](docs/opentag3d.md),
  [Legacy/Unbekannt](docs/legacy-and-unknown-tags.md).
- [Bambu-LAN-MQTT-Protokoll](docs/bambu-protocol.md),
  [Spoolman-API-Nutzung](docs/spoolman-api.md).
- [Release-Prozess](docs/release.md) -- Lizenzen, Versionierung, Quellen,
  Known Issues.

Alle `docs/*.md`-Dateien sind das lebende Referenzwissen; die vollständige,
chronologische Entwicklungshistorie mit Begründung jeder Einzelentscheidung
steht in [`TASKS.md`](TASKS.md).

## Lizenz

MIT, siehe [`LICENSE`](LICENSE). Verwendete Drittanbieter-Lizenzen stehen in
[`docs/release.md`](docs/release.md#lizenzen).
