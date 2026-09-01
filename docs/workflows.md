# UI-Workflows

Dieses Dokument beschreibt die Benutzerworkflows aus Sicht des `AppTask`
(zentraler Event-Router und Zustandsmaschine, siehe `docs/architecture.md`).
Formatspezifische Parser-/Sicherheitsdetails stehen in `docs/nfc-tags.md`,
`docs/bambu-rfid.md`, `docs/openprinttag.md`, `docs/opentag3d.md` und
`docs/legacy-and-unknown-tags.md`; die Identitäts-/Capability-Schicht in
`docs/tag-identity.md`; das Bambu-MQTT-Protokoll selbst in
`docs/bambu-protocol.md`. Hier geht es ausschließlich um das, was der Nutzer
auf dem Display sieht und antippt.

## Screens

`rtos::UiScreenId` (`rtos/Commands.h`) listet alle Bildschirme. Nur der
`UiTask` besitzt LVGL-Objekte; `AppTask` referenziert Screens ausschließlich
über diese Werte.

| Screen | Zweck |
|---|---|
| `Boot` | Startdiagnose, bis UI/Storage bereit sind |
| `Home` | Zentrale Übersicht: aktiver Drucker, AMS/Trays, Staging-Zugang |
| `PrinterSelect` | Aktiven Drucker wechseln |
| `SettingsHome` | Einstiegspunkt für alle Einstellungs-Unterseiten |
| `StagingDetails` | Status der aktuell "angelegten" Spule (siehe Abschnitt "Staging") |
| `StagingActions` | Aktionsmenü für die angelegte Spule (wiegen, Slot konfigurieren, Tag-Aktionen) |
| `TrayDetails` | Details eines einzelnen AMS-/externen Trays |
| `TrayActions` | Aktionsmenü für ein Tray (Slot konfigurieren, zurücksetzen, Zuordnung entfernen) |
| `TraySelect` | Zielauswahl (Drucker/AMS/Tray) für "Slot konfigurieren" |
| `SettingsSpoolman` | Spoolman-Serververbindung konfigurieren/testen |
| `SettingsPrinters` | Druckerliste verwalten (Mehrdrucker) |
| `SettingsPrinterEdit` | Einzelnen Drucker anlegen/bearbeiten |
| `SettingsWifi` | WLAN-Zugangsdaten, Portal, Zurücksetzen |
| `SettingsScale` | Waage: Tarieren, Kalibrierung |
| `SettingsDevice` | Geräteinfo, Neustart |
| `SettingsDiagnostics` | Task-/Systemdiagnose |
| `SettingsFirmware` | Firmware-Version, Update-Suche/-Installation (siehe TASKS.md Phase 13) |
| `TagActionSelect` | Aktionsauswahl für einen erkannten, zuordenbaren NFC-Tag |
| `TagReview` | Vorschau vor dem physischen Schreiben eines nativen Tags |
| `TagWrite` | Schreib-/Löschfortschritt eines nativen Tags |
| `TagResult` | Ergebnisanzeige nach einer Tag-Aktion |
| `TagDefinitionImport` | Gelesene Fremdformat-Definition (Bambu/OpenPrintTag/OpenTag3D/Legacy) vor dem Spoolman-Import |
| `TagLegacy` | Ergebnisanzeige für ein erkanntes Legacy-Format |
| `TagUnknown` | Ergebnisanzeige für ein nicht identifizierbares Tag/Format |
| `BambuSpoolType` | Leergewicht-Voreinstellung (niedrig/hoch/manuell) für einen importierten Bambu-Tag |

## Navigation

Es gibt keine generische Navigations-Historie (keinen Stack) -- `Zurück`
(`UiActionType::Back`) ist für jeden `currentScreen`-Wert einzeln als fester
Zielscreen kodiert (`AppTask.cpp`, `Back`-Fallbehandlung). Für die meisten
Screens ist das Ziel eine feste, naheliegende Elternseite (z. B.
`TrayActions` → `TrayDetails` → `Home`; jede `Settings*`-Unterseite →
`SettingsHome`). Einzelne Screens merken sich stattdessen ein einziges
`previousScreen`, wenn ihr Ziel vom Einstiegsweg abhängt:

* **Staging übersprungen:** Ist beim Öffnen von "Staging" keine Spule
  angelegt (`stagingSpoolId == 0`), überspringt der Einstieg `StagingDetails`
  und geht direkt zu `StagingActions` -- `Zurück` von dort geht dann
  ebenfalls direkt zu `Home` statt zur übersprungenen Statusseite.
* **Tag-Aktionen:** `TagReview`/`TagWrite` gehen zurück zu `StagingActions`
  oder `TagLegacy`, je nachdem von wo die Aktion gestartet wurde, sonst zu
  `TagActionSelect`.
* **Druckereinstellungen:** `SettingsPrinters` merkt sich in
  `printerSettingsReturnScreen`, ob es von `SettingsHome` oder von
  `PrinterSelect` (Mehrdrucker-Verwaltungslink) geöffnet wurde.

`Close` (Dialogschließen, `UiActionType::Close`) läuft durch dieselbe
Fallunterscheidung wie `Back`.

## Hauptworkflow

Der zentrale, wiederkehrende Ablauf ist: **Spule identifizieren → einem
Drucker-Slot zuordnen.** Zwei gleichwertige Einstiege führen zusammen:

1. **Über einen NFC-Tag:** Tag auflegen → `NfcTask` liest, `AppTask`
   bestimmt Format/Identität/Capabilities (`docs/tag-identity.md`) → je nach
   Format direkt `TagActionSelect`/`TagResult` (bereits zugeordnet) oder
   `TagDefinitionImport`/`TagLegacy`/`TagUnknown` (siehe Abschnitte unten).
2. **Über die Spulensuche:** `SearchSpool`/`SelectSpool` in Spoolman suchen,
   ohne einen physischen Tag aufzulegen.

Beide Wege münden in derselben angelegten Spule (`stagingSpoolId`, siehe
"Staging"). Von dort aus wählt der Nutzer über `StagingActions` entweder
"Slot konfigurieren" (Zuordnung zu einem Drucker-Tray, siehe "Slot") oder
eine Gewichtsmessung (Schnell-/Erweitert wiegen).

## Staging

"Staging" ist der Zwischenzustand für genau eine aktuell "angelegte" Spule
(`stagingSpoolId`, `AppTask.cpp`) -- unabhängig davon, welcher Drucker gerade
aktiv ist, und unabhängig von einem Druckerwechsel (siehe "Mehrdrucker":
Staging bleibt beim Wechsel des aktiven Druckers explizit erhalten). Es gibt
höchstens eine angelegte Spule gleichzeitig; eine neue Zuordnung ersetzt die
vorherige.

`stagingSpoolId == 0` bedeutet "nichts angelegt" -- `StagingDetails` hätte in
diesem Zustand nichts anzuzeigen, daher überspringt der Einstieg diesen
Screen zugunsten von `StagingActions` direkt (siehe "Navigation").
`ClearStaging` setzt `stagingSpoolId` explizit zurück auf 0.

## Slot

"Slot konfigurieren" ordnet die aktuell angelegte Spule (oder, bei
`ReapplySlot`, die bereits im Tray bekannte Spule) einem Drucker-AMS-Tray
zu:

1. `ConfigureSlot` (von `StagingActions`) navigiert nur zu `TraySelect` --
   noch kein Commit.
2. Antippen eines konkreten Slots dort löst `ConfigureSlotFromStaging` aus,
   das erst Drucker-ID, AMS/Tray-Grenzen und eine ausgewählte Spule prüft
   (jede Verletzung bricht mit einer spezifischen Fehlermeldung ab, bevor
   irgendetwas gesendet wird), dann die Spoolman-Spulendaten (Material/Farbe)
   erneut lädt -- `AppTask` hält aufgelöste Spulendaten nach dem Staging
   nicht vor -- und erst danach den `AssignTray`-Befehl an `BambuTask`
   sendet. Bestätigt wird ausschließlich über die Drucker-Telemetrie, nicht
   über den blossen MQTT-`publish()`-Erfolg (siehe
   `docs/bambu-protocol.md`).
3. Nach printerseitig bestätigter Zuordnung wird die Assoziation lokal in
   `/mappings/printer-slots.json` gespeichert (`docs/storage.md`).

`ReapplySlot` teilt sich denselben Commit-Pfad wie `ConfigureSlotFromStaging`
und unterscheidet sich nur in der Spulenquelle (bereits bekannte Tray-Spule
statt Staging). `ResetSlot` leert den physischen Slot am Drucker (Material,
Farbe und `spoolId` werden zurückgesetzt); `UntagSlot` entfernt dagegen nur
die lokale Spoolman-Zuordnung, der zuletzt vom Drucker gemeldete physische
Slot-Inhalt bleibt unverändert -- beide teilen sich denselben
Bestätigungspfad, unterscheiden sich nur im gesendeten Zielzustand.
`RefreshSlot` fragt den aktuellen Status ohne jede Zuordnungsänderung erneut
ab.

## Tag zuordnen

Der `AppTask` prüft TagIdentity und gewählte Spoolman-Spule, sucht eine
bestehende Zuordnung über `SpoolmanTask` und aktualisiert anschließend das
Spulenfeld `extra.tag`. Die `TagCapabilities` bestimmen den weiteren Ablauf
(Details zu Identität/Capabilities: `docs/tag-identity.md`):

- Native, sicher beschreibbare NTAG213/215/216 und explizit freigegebene
  Legacy-Tags: `extra.tag` setzen, `spoolman:<id>` schreiben und verifizieren.
- Bambu, OpenPrintTag, OpenTag3D, unbekannte sowie zu erhaltende Legacy-Tags:
  nur `extra.tag` setzen; der Originalinhalt bleibt unverändert.

Schlägt das Schreiben nach erfolgreicher Spoolman-Aktualisierung fehl, bleibt
`extra.tag` bestehen und die Ergebnisanzeige weist auf den Teilerfolg hin.

## Tag-Zuordnung entfernen

Zuerst wird die eindeutige Spule über `extra.tag` ermittelt und das Feld durch
den `SpoolmanTask` geleert. Nur wenn `canClearFilamentStationPayload` gesetzt
ist, entfernt der `NfcTask` danach den eigenen Payload und verifiziert den
leeren Zustand. Fremde Inhalte werden nie gelöscht. Schlägt die optionale
Tagbereinigung fehl, bleibt die Spoolman-Zuordnung entfernt und der Teilerfolg
wird angezeigt.

## Bambu / OpenPrintTag / OpenTag3D / Legacy / Unknown (Tag-Scan-Ergebnis)

Nach dem Lesen bestimmt `currentTag.format` (`models::TagFormat`), welcher
Screen erscheint -- eine bereits bekannte Spoolman-Zuordnung (über die
UID/Identität gefunden) hat dabei überall Vorrang vor der Definitionsanzeige:

| Format | Bereits zugeordnet | Nicht zugeordnet |
|---|---|---|
| FilamentStation (nativ) | `TagActionSelect` mit "Zugeordnet" | `TagActionSelect` mit "Nicht zugeordnet" |
| BambuLab | `TagResult` ("Bambu-Tag read-only: vorhandene Zuordnung verwendet") | `TagDefinitionImport` (Zusammenfassung Hersteller/Filament/Material/Farbe/Nenngewicht); bei Übernahme zusätzlich `BambuSpoolType` fürs Leergewicht, da Bambu-Tags dieses Feld nicht führen |
| OpenPrintTag / OpenTag3D | `TagResult` (Format-Name + Zuordnungshinweis) | `TagDefinitionImport` (zusätzlich Leer-/Nenngewicht, beide Formate führen es) |
| Legacy | -- (immer `TagLegacy`, mit oder ohne bekannte Spulen-ID im Text) | `TagLegacy` |
| Unbekannt | `TagResult` ("per UID mit Spule verbunden, Tag bleibt unverändert") | `TagUnknown` (Technologie/UID/NDEF/Schreibfähigkeit, keine Aktion möglich) |

Ein per `TagResult` angezeigter, bereits zugeordneter Fund lädt die
zugehörige Spule zusätzlich in Staging (`requestStagingSpool()`), damit ein
direkt von diesem Screen gestartetes Wiegen dieselbe Spule verwendet statt
einer zufällig zuvor angelegten. `TagDefinitionImport` selbst schreibt oder
löscht nie den physischen Tag (siehe die jeweiligen Format-Dokus für die
Read-only-Begründung); der Import geht ausschließlich über
`ImportTagDefinition` an `SpoolmanTask`, das daraus eine neue Spule anlegt.

## Mehrdrucker

`SettingsPrinters`/`SettingsPrinterEdit` verwalten die persistierte
Druckerliste (`AddPrinter`, `EditPrinter`, `DeletePrinter`,
`TogglePrinterEnabled`, `SetDefaultPrinter`, `SavePrinterSettings`,
`TestPrinterConnection`); `PrinterSelect` (`SelectManagedPrinter`) wechselt,
welcher Drucker aktiv ist.

`printerCollection` hält den zuletzt bekannten Zustand jedes konfigurierten
Druckers dauerhaft im RAM, unabhängig davon, welcher gerade aktiv ist -- ein
Wechsel markiert nur einen anderen Eintrag als aktiv, löscht oder
überschreibt nie die Daten eines anderen Druckers (`AppTask.cpp`,
`SelectPrinter`-Handler). `Connect` ist idempotent (`BambuTask` aktualisiert
statt neu zu verbinden, falls bereits verbunden) -- derselbe Codepfad deckt
sowohl "noch nie in dieser Sitzung verbunden" als auch "Wechsel zurück zu
einem bereits verbundenen Drucker" ab. Staging bleibt beim Druckerwechsel
absichtlich unverändert erhalten (siehe "Staging").

## Spoolman Offline Error Flow

Der Spoolman-Verbindungszustand (`docs/architecture.md`, Abschnitt
"Spoolman-Anwendungszustand") wird über zwei Kanäle sichtbar gemacht:

1. **Laufende Statusanzeige:** `UpdateSpoolmanState` sendet bei jeder
   Zustandsänderung Text und Serverversion ("Spoolman: online | NFC-Feld:
   bereit" / "... nicht verfügbar" / "Spoolman: offline") -- rein
   informativ, blockiert nichts.
2. **Zentrales Aktions-Gate:** Jede Aktion, für die
   `rtos::requiresOnlineSpoolman(type)` wahr ist (u. a. `AssignTag`,
   `RemoveTagAssignment`, `SearchSpool`, `SelectSpool`,
   `ImportTagDefinition`, `SaveMeasurement`, `QuickWeight`,
   `AdvancedWeight`, `ConfigureSlotFromStaging`, `ReapplySlot`), wird direkt
   am Anfang des Action-Dispatchers durch `requireSpoolman()` geprüft --
   **vor** jeder formatspezifischen Logik. Ist Spoolman nicht bereit, bricht
   die Aktion sofort mit demselben einheitlichen Fehlerdialog ab ("Spoolman
   nicht verbunden" / "Diese Funktion benötigt eine aktive
   Spoolman-Verbindung."), ohne dass jede einzelne Aktion die Prüfung selbst
   wiederholen muss.

Alle nicht-online-pflichtigen Aktionen (Navigation, Ansehen bereits geladener
Daten, Einstellungen außerhalb von Spoolman) bleiben in jedem der drei
Zustände uneingeschränkt nutzbar.

## Sicherheitsbedingungen

- Vor Schreiben und Löschen werden UID und Capabilities erneut geprüft.
- Tagentfernung und UID-Wechsel brechen technische Tagoperationen ab.
- Bambu-, OpenPrintTag- und OpenTag3D-Inhalte bleiben in Version 1 read-only.
- Der AppTask greift nicht auf NFC-Hardware oder SD zu.
- Nur der NfcTask führt PN532-Schreib-, Lösch- und Verifikationsbefehle aus.
- Der StorageTask ist an normalen Tag-Zuordnungen nicht beteiligt.
