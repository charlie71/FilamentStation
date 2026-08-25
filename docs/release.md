# Release

## Lizenzen

FilamentStation selbst steht unter der MIT-Lizenz (`LICENSE`, siehe "Eigene
Lizenz" unten). Verwendete Drittanbieter-Abhängigkeiten, jeweils direkt aus
der bezogenen Bibliotheksversion geprüft (`LICENSE`/`license.txt`-Dateien
bzw. deklariertes `license`-Feld in `library.json`/`package.json`):

| Abhängigkeit | Version | Lizenz | Bezug |
|---|---|---|---|
| ArduinoJson | 7.4.3 | MIT | `lib_deps` |
| LovyanGFX | 1.2.25 | MIT AND BSD-2-Clause | `lib_deps` |
| lvgl | 9.5.0 | MIT | `lib_deps` |
| WiFiManager | 2.0.17 | MIT | `lib_deps` |
| PubSubClient | 2.8 | MIT | `lib_deps` |
| Arduino-ESP32-Framework (WiFi, WiFiClientSecure, HTTPClient, Update, SD, SPI, FS, Serial/HWCDC, `esp_ota_ops` u. a.) | über `platform = espressif32@6.10.0` | LGPL-2.1-or-later | Framework, kein separater `lib_deps`-Eintrag |

Alle genutzten Bibliotheken sind entweder permissiv (MIT/BSD-2-Clause) oder
LGPL-2.1 (nur das Arduino-ESP32-Framework selbst) -- als über den normalen
Arduino-/PlatformIO-Build separat kompilierte und gelinkte Komponente ist
das die in der Arduino-Welt allgemein übliche Nutzungsform von LGPL-Code.
Diese Aussage ist keine Rechtsberatung; bei kommerzieller Weiterverwendung
sollte das im Einzelfall geprüft werden.

## SpoolEase-Code nicht kopiert

Während der Bambu-Protokollentwicklung (TASKS.md, 2026-08-22) wurden zwei
externe Open-Source-Projekte als **Vergleichsreferenz** herangezogen, um
eine hartnäckige Slot-Zuordnungs-Regression einzugrenzen:
`Fire-Devils/filaman-bambulab-plugin` und `yanshay/spoolease`. In beiden
Fällen wurde ausschließlich die **Struktur des gesendeten MQTT-Kommandos**
Feld für Feld analysiert (vorhandene/fehlende Felder wie `slot_id`,
`tray_info_idx`, `sequence_id`, Adressierungskonvention für
`ams_id`/`tray_id`) und mit der eigenen, unabhängig geschriebenen
`BambuProtocol::bambuBuildAmsFilamentSetting()`-Implementierung verglichen
-- siehe `docs/bambu-protocol.md`, Abschnitt "Vergleich mit
FilaMan-System", für die vollständige Feld-für-Feld-Gegenüberstellung.
Übernommen wurde daraus ausschließlich die **Erkenntnis, welche Felder das
Bambu-eigene Protokoll erwartet** (eine durch den Drucker selbst
vorgegebene, nicht schutzfähige Tatsache) -- kein Quellcode der genannten
Projekte wurde kopiert, eingebunden oder adaptiert; keines der beiden
Projekte ist als Abhängigkeit eingebunden.

## Quellen

Externe Primärquellen, auf denen einzelne Implementierungsteile beruhen
(vollständige Angaben inkl. Kontext jeweils am Ort der Verwendung):

| Thema | Quelle | Dokumentiert in |
|---|---|---|
| NTAG213/215/216 | NXP-Produktseite/Datenblatt Rev. 3.2 | `docs/nfc-tags.md` |
| PN532 | NXP User Manual UM0701-02 | `docs/hardware.md` |
| Bambu-Lab-RFID (MIFARE-Blockformat) | Bambu Research Group, `RFID-Tag-Guide` (Reverse Engineering, Community) | `docs/bambu-rfid.md` |
| Bambu-LAN-MQTT-Protokoll | OrcaSlicer (`DeviceManager`/`MQTTClient`), Home-Assistant-Integration "Bambu Lab", `bambulabs_api` (Reverse Engineering, Community, unverifiziert) | `docs/bambu-protocol.md` |
| OpenPrintTag | `OpenPrintTag/openprinttag-specification` (offizielle Spezifikation) | `docs/openprinttag.md` |
| OpenTag3D | `opentag3d.info/spec.html` + `spec.json` (offizielle Spezifikation) | `docs/opentag3d.md` |
| WT32-SC01 Plus | Hersteller-Datenblatt V1.5, RIOT-OS-Boarddefinition | `docs/hardware.md` |
| ESP32-S3 GPIO/UART | Espressif ESP-IDF API-Referenz, ESP32-S3-Datenblatt | `docs/hardware.md` |

## Eigene Lizenz

MIT-Lizenz, siehe `LICENSE` im Projektwurzelverzeichnis (Nutzerentscheidung
2026-08-25). Gewählt wegen Kompatibilität mit allen verwendeten
Abhängigkeiten (siehe "Lizenzen" oben) und weil sie Forks/Weiterverwendung
ohne Copyleft-Pflicht erlaubt.

## Version

`config::kApplicationVersion` (`config/AppConfig.h`) ist die einzige
Versionskonstante der Firmware -- sie erscheint im Bootlog, auf
`SCR_SETTINGS_DEVICE`/`SCR_SETTINGS_FIRMWARE` und wird vom
Firmware-Update-Mechanismus als aktuelle Version verwendet. Format:
lockere Semantic-Versioning-Schreibweise (`X.Y.Z` oder `X.Y.Z-suffix`,
z. B. `0.1.0-dev`), geparst/verglichen über `services::SemVer`
(`services/SemVer.h`, siehe `docs/architecture.md`). GitHub-Release-Tags
folgen demselben Schema mit vorangestelltem `v` (z. B. `v0.1.1`) -- beides
wird von `parseSemVer()` akzeptiert.

Ein Anhang (`-dev`, `-rc1`, o. Ä.) gilt beim Vergleich als **älter** als
dieselbe Kernversion ohne Anhang (kein vollständiger
Prerelease-Identifier-Vergleich nach SemVer-Spezifikation, siehe
`test/test_semver/test_main.cpp` für die genauen Vergleichsregeln).

## Changelog

`CHANGELOG.md` im Projektwurzelverzeichnis, Format an
[Keep a Changelog](https://keepachangelog.com/) angelehnt. Bewusst
zusammenfassend statt vollständig chronologisch -- die lückenlose
Entwicklungshistorie mit Begründung jeder Einzelentscheidung steht bereits
in `TASKS.md` und würde hier nur dupliziert.

## Release

1. `config::kApplicationVersion` (`config/AppConfig.h`) auf die neue Version
   anheben (kein `-dev`-Anhang für einen echten Release).
2. `CHANGELOG.md` um einen neuen Abschnitt für die Version ergänzen.
3. Build (siehe `docs/developer-guide.md`, Abschnitt "Build") --
   0 Warnungen, alle vier nativen Testumgebungen grün.
4. Firmware-Binary und Prüfsumme erzeugen:
   ```text
   pio run -e wt32-s3-wrover-n16r2
   (Get-FileHash .pio/build/wt32-s3-wrover-n16r2/firmware.bin -Algorithm SHA256).Hash.ToLower() |
     Out-File -Encoding ascii firmware.bin.sha256
   ```
5. Git-Tag `vX.Y.Z` setzen und GitHub-Release mit genau diesen beiden
   Dateien veröffentlichen (Dateinamen exakt `firmware.bin`/
   `firmware.bin.sha256`, sonst findet der OTA-Mechanismus die Prüfsumme
   nicht, siehe `TASKS.md` Phase 13.3):
   ```text
   gh release create vX.Y.Z firmware.bin firmware.bin.sha256
   ```
6. Bestehende Geräte finden das neue Release automatisch über
   Einstellungen → Firmware → "Nach Update suchen" (GitHub-Releases-API,
   `config/UpdateConfig.h`); siehe `docs/user-guide.md`, Abschnitt
   "Firmware", für den Nutzerablauf.

## Reproduzierbarer Build

`platformio.ini` pinnt sowohl die Plattform (`platform =
espressif32@6.10.0`) als auch jede Bibliothek in `lib_deps` auf eine exakte
Version (siehe Tabelle unter "Lizenzen") -- ein `pio run -e
wt32-s3-wrover-n16r2` aus einem sauberen Checkout zieht damit
deterministisch dieselben Abhängigkeitsversionen, unabhängig davon, was
zwischenzeitlich neu veröffentlicht wurde. Anwendungscode selbst enthält
keine `__DATE__`/`__TIME__`- oder sonstigen build-zeitabhängigen Makros
(geprüft: kein Treffer in `src/`). Nicht geprüft/nicht zugesichert: ob der
zugrunde liegende Espressif-/GCC-Toolchain-Build selbst bit-identische
Binaries erzeugt (z. B. eingebettete absolute Pfade in Debug-Info) -- dafür
wäre ein direkter Zwei-Build-Diff nötig, der bisher nicht durchgeführt
wurde.

## Known Issues

Aus den unerledigten Checklistenpunkten in `TASKS.md` konsolidiert (Stand
2026-08-25):

* **Kein mehrstündiger Dauertest** (TASKS.md 10.7) -- Task-Verhalten,
  Speicher, UI, Dateizugriffe und Reconnect-Verhalten wurden jeweils
  einzeln, aber nicht über einen durchgängigen Langzeitlauf verifiziert.
* **Keine reale Strommessung je Energiesparstufe** (TASKS.md 11.7, Aktiv/
  Gedimmt/Sleep) -- die Statemachine selbst ist implementiert und
  hardwaregetestet, der tatsächliche Stromverbrauch je Stufe wurde nicht
  gemessen.
* **Wake-Zuverlässigkeit nicht breit getestet** (TASKS.md 11.7) -- Wake aus
  dem Light-Sleep wurde nicht systematisch über mehrere Touch-Positionen
  und im Dauerbetrieb geprüft.
* **Kein Druck-Aktiv-Signal** (TASKS.md 11.7, bewusste Einschränkung in
  Version 1) -- der Drucker liefert kein Signal "Druck läuft gerade"; das
  Energiesparen ist daher rein zeitbasiert (Inaktivität) und kann
  theoretisch während eines laufenden Drucks dimmen/schlafen, auch wenn der
  Nutzer gerade hinschaut.
* **OTA-Rollback-Test noch nicht durchgeführt** (TASKS.md 13.8) -- der
  normale Update-Testlauf wurde vom Nutzer erfolgreich bestätigt; der
  Rollback-Test mit einem absichtlich fehlerhaften Image (Firmware stürzt
  ab, Bootloader fällt automatisch auf die vorherige Partition zurück)
  steht noch aus.

## Kein Security-Key

Durchgängige Projektentscheidung: keine kryptografische
Schlüssel-/Signaturprüfung, weder für Bambu-MQTT noch für Firmware-Updates
-- stattdessen jeweils Transportsicherheit plus ein einfacheres, dem
jeweiligen Bedrohungsmodell angemessenes Verfahren:

* **Bambu-MQTT:** TLS-Verbindung ohne Zertifikatsprüfung
  (`WiFiClientSecure::setInsecure()`), da Bambu-Drucker ein
  selbstsigniertes, nicht öffentlich prüfbares Zertifikat verwenden. Der
  LAN-Access-Code dient als Shared Secret -- im lokalen Netz die in der
  Community übliche Vorgehensweise, siehe `docs/bambu-protocol.md`.
* **Firmware-Update:** kein Code-Signing/keine kryptografische
  Signaturprüfung des heruntergeladenen Images -- stattdessen HTTPS als
  Transportschutz plus SHA-256-Prüfsummenvergleich gegen eine separat
  veröffentlichte `.sha256`-Datei, siehe `TASKS.md` Phase 13.1 (Entscheidung)
  und Phase 13.4 (Umsetzung).

## Keine lokale NFC-Zuordnungsdatenbank

FilamentStation führt keine eigene Datenbank, die NFC-Tag-Identitäten auf
Spoolman-Spulen abbildet -- jede Zuordnung lebt ausschließlich als Wert im
Spoolman-Extrafeld `extra.tag` der jeweiligen Spule. Drei historische
lokale Mapping-Dateien (`/mappings/bambu-tags.json`,
`/mappings/nfc-spools.json`, `/mappings/open-tags.json`) existierten in
einer früheren Version und werden nur noch von einer einmaligen
Migrationsroutine gelesen, nie im normalen Betrieb -- siehe
`docs/legacy-and-unknown-tags.md` und `docs/tag-identity.md` für die volle
Begründung, inklusive der Abgrenzung zu `/mappings/printer-slots.json`
(das ist keine NFC-Zuordnung, sondern ein Drucker-Fach-Cache, siehe
`docs/storage.md`).
