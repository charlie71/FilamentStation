# Bambu-Material-Mapping (`bambu_materials.json`)

Diese Datei ist die Repository-Quelle für das Bambu-AMS-Material-Mapping.
Seit dem Schema-v2-Umbau (2026-08-30) ist sie ein **priorisiertes
Regelwerk** (`rules[]`): jede Regel vergleicht Spoolmans
`material`/`name`/`manufacturer`-Felder gegen Bedingungen (`match`) und
liefert entweder ein Bambu-AMS-Profil (`tray_info_idx`/`tray_type`/
`nozzle_temp_min`/`nozzle_temp_max`) oder ein bewusstes "unsupported" mit
Begründung. Vorher (Schema v1, bis 2026-08-30) war es eine flache Liste
`material` → Profil ohne Priorisierung; das ältere Schema wird von der
Firmware nicht mehr akzeptiert (`reason=unsupported_schema_version`). Sie
wird zur Laufzeit von `tasks::storageTask()` als
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

## Material/Profil hinzufügen

Die Firmware muss dafür **nicht** geändert/neu kompiliert werden.
Stattdessen ein neues Objekt im Array `rules` ergänzen:

```json
{
  "id": "generic-my-material",
  "priority": 10,
  "match": {
    "material_exact": ["MY-MATERIAL", "My Material", "MYMAT"]
  },
  "result": {
    "status": "mapped",
    "tray_info_idx": "GXXXXX",
    "tray_type": "PLA",
    "nozzle_temp_min": 190,
    "nozzle_temp_max": 240
  }
}
```

Ist kein verifiziertes Bambu-Profil bekannt, statt `result.status: "mapped"`
bewusst `"unsupported"` mit Begründung setzen -- kein erfundener
`tray_info_idx`:

```json
{
  "id": "unsupported-my-material",
  "priority": 10,
  "match": {"material_exact": ["MY-MATERIAL"]},
  "result": {"status": "unsupported", "reason": "No verified Bambu profile for this material"}
}
```

**Regel-Felder:**

* `id`: eindeutiger, sprechender Bezeichner (Konvention: `generic-<slug>`
  für Basismaterialien, `support-for-<slug>` für Supportmaterialien,
  `bambu-<slug>`/`generic-<slug>-<variante>` für markenspezifische
  Varianten, `unsupported-<slug>` für bewusst nicht abgebildete Materialien).
* `priority`: höher gewinnt. Ein Basis-Fallback (z. B. reines `PLA`)
  bekommt eine niedrige Priorität (Konvention: `10`); eine spezifischere
  Regel, die zusätzlich `name`/`manufacturer` prüft, eine höhere
  (Konvention: `100`, oder `200` wenn zusätzlich der Hersteller geprüft
  wird). Treffen mehrere Regeln bei der **höchsten** gefundenen Priorität
  gleichzeitig zu, gilt das als Konfigurationsfehler
  (`reason=ambiguous_material_mapping`) -- nie "erste Regel gewinnt".
* `match`: mindestens eine der drei Bedingungen muss vorhanden sein.
  Mehrere Bedingungen (`material_exact`/`name_contains_any`/
  `manufacturer_exact`) werden UND-verknüpft; die Werte **innerhalb**
  einer Bedingung ODER-verknüpft. `material_exact`/`manufacturer_exact`
  vergleichen exakt (nach Normalisierung: getrimmt, Groß-/Kleinschreibung
  ignoriert, mehrere Leerzeichen zu einem zusammengefasst -- **keine**
  Trennzeichen-Aggressivität wie im alten Schema v1, `PLA`/`PLA-CF`/`PLA+`
  bleiben also unterscheidbar). `name_contains_any` prüft stattdessen, ob
  einer der Werte (normalisiert) im `name`-Feld **vorkommt** (Teilstring).
* `result.status`: `"mapped"` (dann `tray_info_idx`/`tray_type`/
  `nozzle_temp_min`/`nozzle_temp_max` alle Pflicht) oder `"unsupported"`
  (dann `reason` Pflicht, die anderen Felder werden ignoriert).

Danach:

1. JSON validieren (z. B. `pio test -e native-spoolman-tests -f
   test_bambu_material_catalog` oder ein beliebiger JSON-Validator).
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
