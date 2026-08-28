# Bambu-Material-Mapping (`bambu_materials.json`)

Diese Datei ist die Repository-Quelle für das Bambu-AMS-Material-Mapping
(`material` → `tray_info_idx`/`tray_type`/`nozzle_temp_min`/`nozzle_temp_max`),
das früher als fest kompilierte Tabelle in `src/services/BambuProtocol.cpp`
lag. Sie wird zur Laufzeit von `tasks::storageTask()` als
`/config/bambu_materials.json` von der SD-Karte geladen (siehe
`docs/bambu-protocol.md`).

## Wo die Datei auf dem Gerät liegt

```text
/config/bambu_materials.json
```

Beim Fehlen dieser Datei (z. B. erster Start auf einer leeren SD-Karte)
lehnt `AssignTray` jede Slot-Zuordnung mit
`reason=material_mapping_unavailable` ab -- es gibt bewusst **keinen**
Fallback auf eine fest kompilierte Tabelle.

## Erstinstallation

Diese Datei manuell auf die SD-Karte kopieren:

```text
data/bambu-materials/bambu_materials.json  ->  /config/bambu_materials.json
```

Alternativ kann sie über die interne Firmware-Downloadfunktion
(`UpdateCommandType::DownloadBambuMaterials`, ausgelöst über
`UiActionType::UpdateBambuMaterials`) aus dem zuletzt veröffentlichten
GitHub-Release heruntergeladen werden -- SHA-256-validiert, bevor sie
aktiviert wird (siehe unten und `docs/bambu-protocol.md`).

## Material hinzufügen

Die Firmware muss dafür **nicht** geändert/neu kompiliert werden.
Stattdessen ein neues Objekt im Array `materials` ergänzen:

```json
{
  "material": "MY-MATERIAL",
  "tray_info_idx": "GXXXXX",
  "tray_type": "PLA",
  "nozzle_temp_min": 190,
  "nozzle_temp_max": 240,
  "aliases": [
    "My Material",
    "MYMAT"
  ]
}
```

`aliases` ist optional; falls vorhanden, ein Array nichtleerer Strings.
Nach Normalisierung (Groß-/Kleinschreibung ignoriert, `-`/`_`/Leerzeichen
werden beim Vergleich übersprungen) müssen `material` und alle `aliases`
über die **gesamte** Datei eindeutig sein -- ein Duplikat verwirft die
komplette Datei beim Laden (`reason=duplicate_lookup_key`).

Danach:

1. JSON validieren (z. B. `pio test -e native-bambu-material-catalog-tests`
   oder ein beliebiger JSON-Validator).
2. SHA-256-Prüfsumme neu erzeugen:
   ```powershell
   (Get-FileHash -Path data/bambu-materials/bambu_materials.json -Algorithm SHA256).Hash.ToLower() |
     Set-Content -Path data/bambu-materials/bambu_materials.json.sha256 -NoNewline -Encoding ascii
   ```
3. Bei einem Firmware-Release wird diese Datei automatisch mit
   veröffentlicht (siehe `scripts/release.ps1`).
4. Auf dem Gerät: Datei herunterladen (siehe oben) oder manuell auf die
   SD-Karte kopieren, danach neu starten.

Eine manuell geänderte JSON-Datei mit **alter** Prüfsumme wird beim
Repository-Download-Update nicht aktiviert (`reason=sha256_mismatch`) --
die Prüfsumme muss nach jeder inhaltlichen Änderung neu erzeugt werden.

## Herkunft der Werte

`tray_info_idx` referenziert Bambu Studios eingebaute *generische*
(nicht markenspezifische) Filament-Profil-IDs, community-dokumentiert über
`Bambu-Research-Group/RFID-Tag-Guide` und die WolfWithSword-Home-Assistant-
Bambu-Lab-Integration -- nicht von Bambu Lab selbst veröffentlicht. Werte
werden verbatim übernommen, nie algorithmisch abgeleitet/geraten. Die
Support-Material-Einträge (`Support For ...`) stammen aus der
Aufgabenbeschreibung vom 2026-08-28 und sind bislang **nicht** gegen echte
Bambu-Studio-Profile verifiziert -- insbesondere `PAHT-CF` (`GFN96`) teilt
sich seinen `tray_info_idx` mit dem bereits vorhandenen, verifizierten
Eintrag `PPA-GF` (ebenfalls `GFN96`); das ist ein offener
Verifikationspunkt, kein bestätigter Fehler (`tray_info_idx`-Eindeutigkeit
wird von `services::BambuMaterialCatalog` bewusst nicht erzwungen, da
mehrere Spoolman-Materialnamen legitim auf dasselbe Bambu-Profil zeigen
können).

Dieser Katalog ist **nicht abschließend** -- der Zweck der Auslagerung auf
die SD-Karte ist gerade, neue Bambu-Materialien unabhängig von der Firmware
nachpflegen zu können.
