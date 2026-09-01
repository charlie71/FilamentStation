# Bambu-LAN-MQTT-Protokoll (Community-Wissen, unverifiziert)

## Status

**Nicht gegen echte Hardware verifiziert.** Bambu Lab veroeffentlicht keine
offizielle Spezifikation fuer den lokalen LAN-Modus. Die folgenden Angaben
stammen aus oeffentlich dokumentiertem Reverse-Engineering der Community
(u. a. OrcaSlicer `DeviceManager`/`MQTTClient`, die Home-Assistant-Integration
"Bambu Lab", `bambulabs_api`). Vor dem ersten Produktiveinsatz muss ein realer
Report-Mitschnitt (z. B. per `mosquitto_sub`) mit den hier getroffenen
Annahmen abgeglichen werden; Abweichungen sind wahrscheinlich und muessen in
`src/services/BambuProtocol.cpp` nachgezogen werden.

`BambuTask` und `BambuProtocol` sind bewusst defensiv geschrieben: unbekannte
oder fehlende Felder werden ignoriert statt einen Fehler auszuloesen.

## Verbindungsaufbau

* Transport: MQTT ueber TLS (MQTTS), Port `8883`.
* Broker: die vom Benutzer hinterlegte `host`-Adresse des Druckers
  (`bambu.json`, Phase 8.2), i. d. R. dessen lokale IP-Adresse.
* Zertifikat: selbstsigniert, vom Drucker selbst ausgestellt. Es existiert
  keine oeffentlich pruefbare Zertifikatskette; die Verbindung wird ohne
  Zertifikatspruefung aufgebaut (`WiFiClientSecure::setInsecure()`). Das ist
  im reinen LAN-Kontext (Access Code als Shared Secret) die in der Community
  uebliche Vorgehensweise.
* Benutzername: `bblp` (fest, siehe `config::kBambuMqttUsername`).
* Passwort: der in den Druckereinstellungen angezeigte LAN-Access-Code
  (`bambu.json`-Feld `accessCode`, Phase 8.2).
* Client-ID: durch `BambuTask` generiert, an Chip-ID und `printerId`
  gebunden.

## Topics

* Report (Abonnement, Drucker -> Client): `device/{serialNumber}/report`
* Request (Veroeffentlichung, Client -> Drucker): `device/{serialNumber}/request`

`{serialNumber}` ist die in `bambu.json` hinterlegte Druckerseriennummer.

## Anfragen (Client -> Drucker)

### Vollstaendigen Status anfordern ("pushall")

```json
{"pushing":{"sequence_id":"1","command":"pushall"}}
```

Wird bei `verbinden` und bei `RequestStatus` auf das Request-Topic
veroeffentlicht. `sequence_id` beginnt bei 1 je (Re-)Connect und steigt mit
jedem Kommando (siehe naechster Abschnitt).

### AMS-/Slot-Filamentdaten schreiben ("ams_filament_setting")

```json
{
  "print": {
    "sequence_id": "42",
    "command": "ams_filament_setting",
    "ams_id": 0,
    "tray_id": 0,
    "slot_id": 0,
    "tray_info_idx": "GFL99",
    "tray_type": "PLA",
    "tray_color": "FFFFFFFF",
    "nozzle_temp_min": 190,
    "nozzle_temp_max": 240
  }
}
```

`slot_id` dupliziert `tray_id` (Slot-Index innerhalb dieser AMS-Einheit).
Community-Referenz `yanshay/spoolease` (siehe unten) sendet beide Felder auf
diesem Kommando.

`sequence_id` wird pro `PrinterConnection` fortlaufend gezaehlt (Start bei 1,
zurueckgesetzt bei jedem (Re-)Connect -- eine frische MQTT-Session) und bei
jedem gesendeten Kommando (auch "pushall") um 1 erhoeht, statt wie zuvor
immer als fixe `"0"` gesendet zu werden. Grund: OpenBambuAPI (community-
Referenz) dokumentiert das Feld explizit als "incremented by 1 on each
command"; nachdem selbst nach dem `tray_info_idx`-Fix ein bereits belegter
AMS-Slot die Zuordnung nach 3-5 Sekunden wieder zuruecksetzte (siehe unten),
ist ein durchgaengig gleichbleibendes `sequence_id` ein weiterer Kandidat --
noch unverifiziert, ob es die eigentliche Ursache ist.

`tray_color` ist ein 8-stelliger Hex-String (RRGGBBAA, Alpha immer `FF`).
Spoolmans `color_hex` liefert nur 6-stelliges RRGGBB ohne Alpha;
`BambuProtocol::bambuBuildAmsFilamentSetting()` haengt das Alpha-Byte `FF`
automatisch an ein 6-stelliges `trayColorHex` an, statt einen laut
Spezifikation ungueltigen 6-stelligen Wert zu senden.

Diese Nutzlast wird von `AssignTray`/"Slotdaten schreiben" gesendet; die
Zuordnung Spoolman-Spule -> Filamenttyp/-farbe erfolgt in einer spaeteren
Phase (8.5, AMS-Zuordnung) und wird `BambuTask` fertig aufbereitet
uebergeben.

`tray_id_name` ist kein Bambu-Standardfeld mit fester Bedeutung -- der
Versuch war eine eigene Konvention dieses Projekts:
`bambuBuildAmsFilamentSetting()` schreibt bei jeder Zuordnung
`"SM<spoolmanId>"` hinein (leerer String beim Leeren eines Slots), in der
Hoffnung, die Spoolman-Zuordnung eines Slots so aus dem Drucker selbst
rekonstruieren zu koennen, statt sie nur lokal im ESP32-RAM zu verwalten
(das eine Neuverbindung/einen Neustart nicht ueberlebt). Urspruenglich mit
Doppelpunkt (`"SM:<spoolmanId>"`) -- auf Nutzerwunsch (2026-08-23) ohne
Trennzeichen umgestellt, als Test, ob der Doppelpunkt der Grund fuer das
folgend beschriebene Verwerfen des Werts war (noch nicht erneut auf
Hardware verifiziert).

**Per Hardwaretest widerlegt (2026-08-23):** der Drucker nimmt den Wert
beim Schreiben klaglos an (`ams_filament_setting` wird nicht abgelehnt,
der Wert wird sogar in der Kommando-Bestaetigung echot), gibt das Feld in
Statusberichten aber immer leer zurueck. Zunaechst beobachtet in den
regulaeren periodischen `push_status`-Nachrichten -- die enthalten in der
Praxis aber meist **gar kein** `ams`-Feld (nur Telemetrie wie
`bed_temper`/`wifi_signal`); `ams.ams[]` (inkl. `tray_id_name`) wird
offenbar nur bei einem vollen Pushall mitgeliefert. Deshalb gezielt
nachgetestet: Zuordnung gesetzt, Bestaetigung abgewartet
(`AssignTray confirmed`), danach ueber den "Aktualisieren"-Button auf dem
Tray-Details-Screen (`UiActionType::RefreshSlot` ->
`BambuCommandType::RequestStatus` -> frisches `pushall`) explizit ein
neuer voller Statusabruf ausgeloest -- **auch dessen Antwort zeigt
`tray_id_name` weiterhin leer** (`[PLA:""] [PLA:""] [PLA:""]` fuer alle drei
belegten Faecher). Damit ist ausgeschlossen, dass der falsche
Abfragebefehl die Ursache war (Nutzer-Verdacht) -- der Drucker speichert
das Feld nachweislich nirgends, unabhaengig davon, wie/wann man nachfragt.
Der Wert existiert offenbar nur als Echo der Kommando-Bestaetigung, nicht
als AMS-Zustand.

Die Lesevariante (`bambuApplyReport()` parst `tray_id_name` zurueck in
`spoolId`) wurde deshalb zunaechst wieder entfernt -- sie haette sonst eine
gerade erst per `checkPendingTrayAssignment()` lokal bestaetigte Zuordnung
Sekunden spaeter wieder auf "unbekannt" zurueckgesetzt, ein aktiver
Rueckschritt gegenueber dem vorherigen (rein lokalen) Verhalten.

**Ansatz komplett verworfen (2026-08-24, Nutzerwunsch):** nach dem obigen
Befund ergibt auch das weitere Schreiben von `tray_id_name` keinen Sinn
mehr -- `BambuTrayFilament::spoolmanId` und die `tray_id_name`-Kodierung in
`bambuBuildAmsFilamentSetting()` wurden vollstaendig entfernt.
`PrinterSlotStateData` hat seitdem bewusst **kein** `spoolId`-Feld mehr
(weder von `bambuApplyReport()` noch von `BambuTask` befuellt) -- die
gesamte Idee, die Zuordnung ueber den Drucker selbst zu spiegeln, ist vom
Tisch.

Stattdessen: ein lokal auf der SD-Karte persistierter Cache
(`/mappings/printer-slots.json`, `models/TraySpoolCache.h`,
`rtos::StorageDocumentType::TraySpoolCache`), verwaltet komplett in
`AppTask`. Jeder Eintrag haelt `printerId`/`amsId`/`trayId` ->
`spoolId` **plus** `material`/`colorHex`, wie sie der Drucker im Moment der
Bestaetigung meldete. Beim Anzeigen (`AppTask::syncAmsToUi()` ->
`resolveTraySpoolCacheSpoolId()`) wird das *aktuelle* `material`/`colorHex`
gegen den Cache-Eintrag geprueft: stimmt es nicht mehr ueberein (Spule
physisch ausgetauscht, ohne dass diese App davon weiss), gilt die
Zuordnung als unbekannt (`spoolId` 0, UI zeigt "?") statt einer
womoeglich falschen Nummer. Ueberlebt Neuverbindung/Neustart, da rein
lokal auf der SD-Karte, unabhaengig vom Drucker.

**Nachtrag (2026-08-28, Nutzerwunsch -- Temperaturhandling vereinfacht,
Material-Mapping erweitert):** `nozzle_temp_min`/`nozzle_temp_max` stammen
seitdem **nicht mehr** aus Spoolman-Filament-Extra-Feldern, sondern aus der
statischen, im Quellcode hinterlegten Tabelle
`BambuProtocol::kBambuMaterialMappings[]` (`resolveBambuMaterial()`) --
indiziert ueber den freien Spoolman-Materialtext (z. B. `PLA`, `PETG`,
`PLA-CF`), exaktes Matching nach Normalisierung (Gross-/Kleinschreibung und
Trennzeichen `-`/` `/keins werden ignoriert), **kein** Praefix-Matching mehr
(damit z. B. `PLA-CF` nie faelschlich als `PLA` erkannt wird). Jeder
Tabelleneintrag liefert `tray_info_idx`, `tray_type` und die zugehoerige
AMS-Duesentemperaturspanne in einem Schritt. Ist das Material nicht in der
Tabelle enthalten, wird die gesamte `AssignTray`-Anfrage abgelehnt
(`AppEventType::BambuError`, Log `reason=no_material_mapping`) statt mit
einer erfundenen oder unvollstaendigen Temperatur fortzufahren. Grund fuer
die Umstellung: die zuvor genutzten Spoolman-Felder
`bambu_temp_min`/`bambu_temp_max` sind Inventar-/Hersteller-Metadaten des
Filaments, nicht zwingend die vom Drucker fuer dieses Material erwarteten
Werte; die statische Tabelle liefert stattdessen konsistent dieselben
Werte, die auch Bambu Studio fuer sein generisches Profil verwendet. Die
Spoolman-Felder `bambu_temp_min`/`bambu_temp_max` selbst wurden **nicht**
entfernt und bleiben fuer andere Zwecke (z. B. Anzeige) nutzbar -- sie
fliessen nur nicht mehr in den `ams_filament_setting`-Payload ein. Ebenso
bewusst **nicht** angefasst: `tray_info_idx` wird weiterhin ausschliesslich
ueber diese Tabelle aufgeloest, es wird an keiner Stelle dieses Ablaufs ein
Bambu-`setting_id`-Feld resolved oder gesendet.

Damit entfaellt auch der bisherige Zwischenschritt, extra fuer
`AssignTray` ein `GET /filament/{filamentId}` abzufragen
(`SlotAssignmentStage::LoadingFilament`, siehe Nachtrag unten) -- die
Materialzuordnung braucht nur noch den bereits aus `LoadSpool` bekannten
Materialtext, `SlotAssignmentStage` kennt seitdem nur noch `None ->
SelectingSpool -> LoadingSpool -> WritingSlot`. Die Home-Tray-Karten-Anzeige
(`AppTask::resolveTraySpoolDetails()`, K-Faktor/Restgewicht) ist von dieser
Aenderung nicht betroffen und fragt `GET /filament/{filamentId}` weiterhin
wie zuvor ab.

**Nachtrag (2026-08-28, Fortsetzung, Nutzerwunsch -- Material-Mapping von
der SD-Karte laden, aus dem Repository herunterladen, SHA-256-validiert):**
Die oben beschriebene `kBambuMaterialMappings[]`-Tabelle war zu diesem
Zeitpunkt noch fest im Quellcode kompiliert. Direkt im Anschluss wurde sie
durch eine **zur Laufzeit von der SD-Karte geladene JSON-Datei**
(`/config/bambu_materials.json`) ersetzt, damit neue Materialien ohne
Firmware-Neukompilierung ergaenzt werden koennen -- Architektur (Resolver,
Exact-Match nach Normalisierung, kein `setting_id`, kein Fallback auf
Spoolman-Temperaturen) bleibt dabei unveraendert, nur die Datenquelle
aendert sich.

* **Schema** (`services::BambuMaterialCatalog`,
  `services/BambuMaterialCatalog.h/.cpp`, reine Parser-/Validierungslogik
  ohne Datei-/Netzwerkzugriff): oberste Ebene `{"schema_version": 1,
  "materials": [...]}`, je Eintrag `material`/`tray_info_idx`/`tray_type`
  (Pflicht, nicht-leer) und `nozzle_temp_min`/`nozzle_temp_max` (Pflicht,
  `0 < min <= 400`, `min <= max`), optional `aliases` (Array
  nicht-leerer Strings, alternative Schreibweisen). Nach Normalisierung
  (identische Regeln wie `services::sameMaterialKey()`) muessen `material`
  und alle `aliases` projektweit eindeutig sein -- eine Kollision verwirft
  die **komplette** Datei beim Laden (kein Teilerfolg), ebenso jeder
  sonstige Parse-/Validierungsfehler. Quelle im Repository:
  `data/bambu-materials/bambu_materials.json` (1:1-Migration der
  vorherigen 36 Tabelleneintraege, ergaenzt um 14 `Support For ...`-
  Supportmaterialien sowie `PAHT-CF`/`PC-CF`/`BAMBU-PVA`/`TPU 95A`, siehe
  `data/bambu-materials/README.md`) + `.sha256`-Pruefsummendatei, beide
  bei jedem `scripts/release.ps1`-Release automatisch mit neu erzeugt und
  als GitHub-Release-Assets veroeffentlicht (wie `firmware.bin`).
* **RAM-Cache statt Queue-Transport:** Die geladene Tabelle
  (`models::BambuMaterialMappingTable`, bis zu 96 Eintraege, ca. 18 KiB)
  wird **nicht** durch `rtos::AppEvent` geschickt -- `AppEvent` wird als
  Wert in eine 16 Elemente tiefe FreeRTOS-Queue in knappem internem RAM
  kopiert (unabhaengig vom tatsaechlichen `AppEventType`, da kein
  `union`), ein zusaetzliches Feld dieser Groesse waere dort 16-fach so
  teuer. Stattdessen haelt `rtos::RtosContext` ein neues Feld
  `std::atomic<const models::BambuMaterialMappingTable*>
  bambuMaterialMappings` (`nullptr` = "keine gueltige Tabelle geladen").
  `StorageTask` ist der einzige Schreiber: es haelt zwei
  PSRAM-allokierte Pufferinstanzen (`services::allocatePsramInstance()`,
  Doppelpufferung), schreibt eine frisch geparste Tabelle in die gerade
  inaktive Instanz und veroeffentlicht sie danach mit genau einem
  atomaren `store(..., std::memory_order_release)` -- kein Mutex noetig,
  jeder Leser bekommt entweder die alte oder die neue, nie eine halb
  geschriebene Tabelle. `BambuTask::handleAssignTray()` liest den Zeiger
  direkt (`load(std::memory_order_acquire)`) -- das ist kein SD-Zugriff
  (reines Lesen eines bereits validierten, unveraenderlichen Snapshots im
  RAM) und verletzt damit nicht die Regel "keine SD-Zugriffe ausserhalb
  StorageTask" (`AGENTS.md`); die im vorigen Nachtrag getroffene
  Entscheidung, dass `BambuTask` selbst aufloest, bleibt dadurch
  bestehen, `rtos::BambuCommand` aendert sich nicht.
* **Laden beim Boot** (`StorageTask.cpp::loadBambuMaterialCatalog()`):
  direkt nach `ensureInitialDocuments()`, aber **kein**
  `InitialDocument` -- es gibt bewusst **keine** automatisch erzeugte
  Default-Datei (dieses Projekt hat keinen bestehenden
  Bundled-Asset-Copy-Mechanismus, der dafuer wiederverwendet werden
  koennte). Fehlt `/config/bambu_materials.json` oder ist sie ungueltig,
  wird ein **gueltiges** `.bak.json` als Fallback probiert (3-Wege-
  Wiederherstellung wie `services::JsonStorage::recoverAtomicSave()`,
  aber mit `parseBambuMaterialCatalog()` statt `JsonStorage::load()` als
  Gueltigkeitspruefung); eine unvollstaendige `.tmp.json` (Rest eines
  unterbrochenen Downloads) wird nie als aktiv betrachtet. Schlaegt auch
  das fehl, bleibt `bambuMaterialMappings` auf `nullptr` -- es gibt
  bewusst **keinen** Fallback auf eine fest kompilierte Tabelle (das
  wuerde den Zweck der Auslagerung unterlaufen). `BambuTask::
  handleAssignTray()` lehnt `AssignTray` in diesem Fall mit
  `reason=material_mapping_unavailable` ab, ohne abzustuerzen.
* **Download ueber denselben GitHub-Release-Mechanismus wie die
  Firmware** (`UpdateCommandType::DownloadBambuMaterials`,
  `UiActionType::UpdateBambuMaterials` -- aktuell nur als interne
  API/Command angebunden, noch kein eigener GUI-Button): `UpdateTask`
  fragt dieselbe `releases/latest`-Antwort ab wie `downloadUpdate()`,
  sucht darin nach den exakten Asset-Namen `bambu_materials.json` und
  `bambu_materials.json.sha256` (beide muessen vorhanden sein, sonst
  Ablehnung, `reason=missing_sha256`) und streamt die Bytes in
  `kStorageJsonPayloadCapacity`-grossen Haeppchen (768 Byte je Nachricht,
  keine Vergroesserung der bestehenden `StorageCommand`-Queue) an
  `StorageTask` (`StorageCommandType::BeginBambuMaterialDownload` /
  `WriteBambuMaterialChunk` / `CommitBambuMaterialDownload` /
  `AbortBambuMaterialDownload`) -- `UpdateTask` schreibt selbst **nie**
  auf die SD-Karte, das darf ausschliesslich `StorageTask`. `StorageTask`
  ist die alleinige Autoritaet ueber Aktivierung: nach vollstaendigem
  Empfang wird die geschriebene `/config/bambu_materials.tmp.json`
  **selbst neu geoeffnet und gehasht** (`mbedtls_sha256_*`, identisches
  Muster wie beim Firmware-Download) und mit dem von `UpdateTask`
  mitgelieferten erwarteten Hash verglichen -- unabhaengig davon, was
  `UpdateTask` waehrend des Streamens schon gesehen hat. Nur bei
  Uebereinstimmung **und** erfolgreicher JSON-Validierung wird die Datei
  atomar aktiviert (Backup-/Umbenennungs-Sequenz analog zu
  `JsonStorage::atomicSave()`, aber eigenstaendig implementiert -- dieses
  Dokument nutzt ein anderes Schema/Envelope als `JsonStorage` versteht)
  und der RAM-Cache atomar getauscht. Bei jedem Fehler (Netzwerk, SHA-256-
  Mismatch, ungueltiges JSON, zu grosse Datei) bleiben aktive Datei und
  RAM-Cache unveraendert; die temporaere Datei wird verworfen. Vertrauens-
  basis fuer den erwarteten Hash: derselbe `.sha256`-Sidecar-Mechanismus
  wie beim Firmware-Update -- **HTTPS-Transport plus Integritaetspruefung,
  keine Signatur-/Authentizitaetspruefung**, identische, bereits
  dokumentierte Einschraenkung wie in `docs/release.md` ("Kein
  Security-Key"), keine neue Schwaeche.
* **Nachtrag (2026-08-28, Fortsetzung, Nutzerwunsch: mit der Firmware
  zusammen aktualisieren):** `UpdateTask::downloadUpdate()` (Firmware-
  Installation) sucht in derselben bereits abgerufenen `releases/latest`-
  Asset-Liste zusaetzlich nach `bambu_materials.json`/`.sha256` (kein
  zweiter API-Aufruf noetig). Sind beide vorhanden, wird die
  Material-Zuordnung **automatisch direkt im Anschluss** an eine
  erfolgreiche Firmware-Installation heruntergeladen und aktiviert --
  ueber denselben `streamBambuMaterialsFromUrls()`-Kern wie der
  eigenstaendige Weg (`UpdateCommandType::DownloadBambuMaterials`), nur
  mit `reportEvents=false`: kein eigenes Fortschritts-Overlay/Ergebnis-
  Dialog, der mit dem bereits gezeigten "Update installiert, jetzt neu
  starten?"-Dialog der Firmware kollidieren wuerde -- der Ausgang wird
  stattdessen nur geloggt (`[BAMBU] Material mapping ...`-Zeilen, siehe
  oben). Ein Fehlschlag hier aendert **nie** das bereits gemeldete
  Firmware-Ergebnis (das wird zuerst, unabhaengig davon gemeldet).
  Veroeffentlicht ein Release die Material-Mapping-Assets nicht (z. B.
  aeltere Releases vor dieser Funktion), wird dieser Teil stillschweigend
  uebersprungen -- kein Fehler. Der eigenstaendige Weg
  (`UiActionType::UpdateBambuMaterials`) bleibt zusaetzlich bestehen, fuer
  ein Nachziehen der Material-Zuordnung unabhaengig von einem
  Firmware-Release.
* **Nachtrag (2026-08-28, Nutzerbericht: Ger\xC3\xA4t bootete nicht mehr, SD-
  Karte musste repariert werden -- Frage: wird die Temp-Datei korrekt
  geschlossen?):** Codepruefung bestaetigte, dass die offene Temp-Datei in
  jedem regulaer erreichbaren Codepfad (Commit, Abort, jeder Fehlerfall in
  Begin/WriteChunk) korrekt geflusht/geschlossen wird, bevor irgendeine
  weitere SD-Operation folgt. Zwei enge Luecken trotzdem gefunden und
  behoben: (1) `UpdateTask::sendStorageCommand()` pr\xC3\xBCft jetzt seinen
  eigenen Erfolg -- schl\xC3\xA4gt ausgerechnet der `Commit`-Versand fehl
  (Queue voll), schickt `streamBambuMaterialsFromUrls()` sofort ein
  `AbortBambuMaterialDownload` nach, statt die bereits offene Temp-Datei
  bei `StorageTask` unbegrenzt offen zu lassen. (2) Neues, unabh\xC3\xA4ngiges
  Sicherheitsnetz direkt in `StorageTask`: eine offene Download-Datei, die
  laenger als `config::kBambuMaterialDownloadStaleTimeoutMs` (30 s) ohne
  `WriteChunk`/`Commit`/`Abort` daliegt, wird von der Haupt-Loop selbst
  geschlossen/verworfen -- deckt auch einen Absturz/Reboot von `UpdateTask`
  selbst zwischen `Begin` und `Commit` ab. Wahrscheinlichste tats\xC3\xA4chliche
  Ursache der gemeldeten SD-Besch\xC3\xA4digung bleibt trotzdem eine der bereits
  oben dokumentierten, mittlerweile behobenen GDMA/TLS/SD-Abst\xC3\xBCrze
  waehrend eines aktiven Schreibvorgangs (SPI-Transaktion mitten im Ablauf
  unterbrochen -- ein haerterer Mechanismus als ein lediglich offen
  gebliebenes, aber inaktives Handle). Details/Code-Kommentare siehe
  `TASKS.md`.
* **Offener Verifikationspunkt:** `PAHT-CF` (`GFN96`, aus der
  Aufgabenbeschreibung uebernommen) teilt sich seinen `tray_info_idx` mit
  dem bereits vorhandenen, verifizierten Eintrag `PPA-GF` (ebenfalls
  `GFN96`) -- `services::BambuMaterialCatalog` erzwingt keine
  `tray_info_idx`-Eindeutigkeit (mehrere Spoolman-Materialnamen koennen
  legitim auf dasselbe Bambu-Profil zeigen), trotzdem noch nicht gegen
  echte Bambu-Studio-Profile verifiziert, siehe
  `data/bambu-materials/README.md`.

**Nachtrag (2026-08-30, Projektänderung -- Resolver auf regelbasiertes
Schema v2 umgestellt):** Der bisherige flache Materialkatalog (Schema v1:
`material` -> genau ein Bambu-Profil, plus Aliase) konnte Bambu-eigene
Spezialprofile (z. B. "Generic PLA Silk", "Generic PLA High Speed",
"Generic PETG HF", "Bambu PLA Basic") nicht abbilden, da Spoolman diese
Unterscheidung nicht im `material`-Feld trägt, sondern im Filament-
`name`/Hersteller. Ersetzt durch ein priorisiertes Regelwerk:

* **Schema v2** (`{"schema_version": 2, "rules": [...]}` statt
  `{"schema_version": 1, "materials": [...]}`; Schema v1 wird von der
  Firmware seitdem **nicht mehr** akzeptiert, `reason=
  unsupported_schema_version` -- bewusste Entscheidung gegen einen
  dauerhaft doppelt gepflegten Resolver). Jede Regel (`models::
  BambuMaterialRule`) trägt eine eindeutige `id`, eine `priority`, ein
  `match`-Objekt (`material_exact`/`name_contains_any`/
  `manufacturer_exact`, verschiedene Kategorien UND-, Werte **innerhalb**
  einer Kategorie ODER-verknüpft) und ein `result` (`status: "mapped"`
  mit `tray_info_idx`/`tray_type`/`nozzle_temp_min`/`nozzle_temp_max`,
  oder `status: "unsupported"` mit `reason`). Details/Feldreferenz:
  `data/bambu-materials/README.md`.
* **Normalisierung bewusst milder als das alte `sameMaterialKey()`**
  (`services::BambuMaterialCatalog.cpp`s neue, dateiinterne
  `normalizeMatchText()`): trimmt, vergleicht ohne Groß-/Kleinschreibung,
  fasst mehrere Leerzeichen zu einem zusammen -- entfernt aber **keine**
  Trennzeichen mehr, damit `PLA`/`PLA-CF`/`PLA+` weiterhin unterscheidbar
  bleiben (das alte Schema-v1-Matching hätte `PLA CF` mangels dieser
  Unterscheidung als Alias von `PLA-CF` behandelt, was für Schema v2s
  exakte `material_exact`-Werte nicht mehr passend wäre).
* **Resolver** (`services::resolveBambuMaterialRule()`, ersetzt das alte
  `services::resolveBambuMaterial()`/`sameMaterialKey()`-Paar aus
  `BambuProtocol.{h,cpp}` -- beide vollständig entfernt, jetzt in
  `BambuMaterialCatalog.{h,cpp}` beheimatet, da eng an das neue
  Regel-Schema gekoppelt): sammelt alle passenden Regeln, ermittelt die
  höchste zutreffende `priority`, und liefert genau dann `Mapped`/
  `Unsupported`, wenn an dieser Priorität **genau eine** Regel zutrifft.
  Mehrere Regeln an derselben (höchsten) Priorität ergeben `Ambiguous`
  (`reason=ambiguous_material_mapping`) -- nie durch JSON-Reihenfolge
  entschieden. Kein Treffer ergibt `NoMatch`
  (`reason=no_material_mapping`), beides führt in `BambuTask::
  handleAssignTray()` zur Ablehnung ohne MQTT-Traffic, exakt wie zuvor
  bei einem unbekannten Material.
* **`rtos::BambuCommand`/`AppTask::PendingSlotAssignment`** tragen jetzt
  zusätzlich zu `trayType` (Spoolmans `material`) auch `name`
  (Spoolmans Filament-Produktname, `SpoolmanSpool::filament`) und
  `manufacturer` (`SpoolmanSpool::vendor`) -- beide Felder waren bereits
  Teil der bestehenden `LoadSpool`-Antwort und wurden bisher nur
  verworfen, kein zusätzlicher HTTP-Request nötig.
* **33 aktuelle SpoolmanDB-Materialtypen** bewusst behandelt (14 auf ein
  bestehendes/neues generisches Profil abgebildet -- u. a. `PLA+`/`ABS+`/
  `ABS-T` fallen auf die generische PLA-/ABS-Regel zurück, `Nylon`/
  `Flexible (TPU)`/`Polycarbonate (PC)`/`Polypropylene (PP)` sind
  zusätzliche `material_exact`-Werte der bestehenden PA-/TPU-/PC-/PP-
  Regeln --, 19 explizit `unsupported` mit Begründung, z. B. `Wood`/
  `Carbon Fiber` ("Base polymer is unknown"), `PC/ABS`/`PC/PBT`
  ("No verified Bambu profile for this blend")). Alle 54 bisherigen
  Schema-v1-Einträge (40 Basismaterialien + 14 Supportmaterialien) 1:1
  migriert, keiner entfernt.
* **Neue Spezialprofile** (Priorität 100, je zusätzlich zu `material`
  auf `name_contains_any` geprüft): `generic-pla-silk` (`GFL96`),
  `generic-pla-high-speed` (`GFL95`), `generic-petg-hf` (`GFG96`); dazu
  `bambu-pla-basic` (Priorität 200, zusätzlich `manufacturer_exact:
  ["Bambu Lab"]`, `GFA00`) -- ein fremdes Produkt namens "PLA Basic" fällt
  mangels Hersteller-Treffer auf die generische PLA-Regel zurück statt
  fälschlich `GFA00` zu erhalten.
* **Support-Regeln** (Priorität 100, unverändert gegenüber Schema v1
  migriert) gewinnen weiterhin gegenüber der generischen Basisregel
  (Priorität 10) für dasselbe Material, z. B. `Support For PLA` (Material
  bereits der Support-Produktname selbst, wie in Spoolman üblich) ->
  `GFS02`, nicht die generische PLA-Regel `GFL99`.
* **Neu entdeckte, bereits vor dieser Änderung bestehende
  Ungereimtheit:** der migrierte Schema-v1-Eintrag `TPU 95A` zeigt
  ebenfalls auf `GFL95` -- denselben `tray_info_idx`, den diese Änderung
  jetzt explizit für "Generic PLA High Speed" verwendet (Vorgabe der
  Aufgabenbeschreibung). Da `services::BambuMaterialCatalog`
  `tray_info_idx`-Eindeutigkeit bewusst nicht erzwingt (siehe den
  `PAHT-CF`/`PPA-GF`-Verifikationspunkt oben), ist das keine Ablehnung,
  aber ein weiterer offener Verifikationspunkt: mindestens einer der
  beiden `GFL95`-Werte (aus unterschiedlichen, beide unverifizierten
  Community-Quellen übernommen) dürfte falsch sein.
* **Tests:** `test_bambu_protocol`s bisherige `resolveBambuMaterial`-Tests
  entfernt (Funktion existiert nicht mehr); `test_bambu_material_catalog`
  komplett auf Schema v2 umgestellt (Parser-Validierung + Resolver,
  inkl. Prioritäts-/Mehrdeutigkeits-/Support-/Unsupported-Fälle) und um
  drei Tests erweitert, die die **echte** Repository-Datei
  (`data/bambu-materials/bambu_materials.json`) laden und alle 33
  SpoolmanDB-Typen sowie alle Spezial-/Support-Regeln gegenprüfen --
  fängt künftig eine kaputte/mehrdeutige Katalog-Änderung bereits im
  native Testlauf ab, nicht erst auf echter Hardware.
* **`config::kBambuMaterialsMaxFileSize`** von 16 auf 48 KiB angehoben:
  die migrierte, weiterhin von Hand editierbar formatierte (eingerückte)
  Datei ist mit 77 Regeln bereits ca. 26 KiB groß, Schema v2s Match-/
  Result-Objekte sind deutlich verboser als Schema v1s flache Einträge.
  Download-/SHA-256-/Aktivierungs-Infrastruktur (`StorageTask.cpp`)
  unverändert wiederverwendet, nur der Größen-Deckel geändert.

**Nutzerhinweis (2026-08-24):** `bambu_temp_min`/`bambu_temp_max` (und das
neue, Anzeige-only `flow_dynamics_k_factor`, siehe unten) sind
Spoolman-Eigenschaften des **Filaments**, nicht der Spule. Urspruenglich
wurden sie aus dem in einer Spool-Antwort (`GET /spool/{id}`)
verschachtelten `filament`-Objekt gelesen
(`spoolResponse.filament.extra.bambu_temp_min`) -- strukturell zwar schon
auf Filament-Ebene, aber implizit auf die Vollstaendigkeit dieses
eingebetteten Objekts angewiesen. Auf Nutzerwunsch umgestellt: `AppTask`
fragt jetzt nach einer erfolgreichen `LoadSpool`-Antwort (fuer
`remainingWeightGrams` und um `filamentId` zu erfahren) explizit
`GET /filament/{filamentId}` ab (`SpoolmanCommandType::LoadFilament`,
`SpoolmanTask::loadFilamentDetails()`/`parseFilament()`) und liest
`bambu_temp_min`/`bambu_temp_max`/`flow_dynamics_k_factor` von dort --
`extra` liegt in dieser Antwort auf Root-Ebene, nicht mehr verschachtelt.
Betrifft sowohl den AssignTray-Ablauf (`SlotAssignmentStage::LoadingFilament`,
neuer Zwischenschritt zwischen `LoadingSpool` und `WritingSlot`) als auch
die Home-Tray-Karten-Anzeige (`AppTask::resolveTraySpoolDetails()`).
Schlaegt der Filament-Fetch fehl (Netzwerkfehler), wird die Zuordnung
trotzdem abgeschlossen, nur ohne Temperatur/K-Faktor -- dieselbe
Nutzerfreundlichkeit wie bei fehlenden/ungueltigen Extra-Feldern, siehe
oben.

`flow_dynamics_k_factor` (K-Faktor, Flow-Dynamics-Kalibrierung) ist ein
weiteres projektspezifisches Filament-Extra-Feld, keine Plausibilitaetsspanne
wie bei den Temperaturen (nur "> 0" gefordert). Der urspruenglich von mir
angenommene Feldname `bambu_k_factor` war falsch; per Hardware-Test
(2026-08-24) vom Nutzer korrigiert. Urspruenglich (2026-08-24) rein fuer die
Anzeige auf der Home-Tray-Karte gedacht, ohne Einfluss auf ein an den
Drucker gesendetes Kommando -- seit 2026-08-28 wird er zusaetzlich an den
Drucker hochgeladen, siehe "K-Faktor-Upload" weiter unten.

**Nachtrag (2026-08-24, Hardware-Test zeigte `nozzle_temp_min=0
nozzle_temp_max=0` trotz konfigurierter Extra-Felder):** neben der
Feldnamen-Korrektur oben hatte `getJson()`/`loadFilamentDetails()`
(`SpoolmanTask.cpp`) bis dahin ueberhaupt kein Logging -- weder Erfolg
noch Fehlschlag des `GET /filament/{id}`-Abrufs war sichtbar. Ergaenzt:
`getJson()` loggt jetzt jeden Fehlschlag (`FS_LOGE`, URL + Fehlertext,
inkl. JSON-Parse-Fehler) und jeden Erfolg (`FS_LOGT`, URL);
`loadFilamentDetails()` loggt zusaetzlich die tatsaechlich aus der Antwort
extrahierten Werte (`FS_LOGD`: filament_id,
temp_fields_present/valid, temp_min/max, kfactor_present/valid/wert).

**Nachtrag (2026-08-24, eigentliche Ursache gefunden -- Spoolman-
Listenabschluss-Marker wird mit der LoadFilament-Antwort verwechselt):**
Die neuen Logs (siehe oben) zeigten das raetselhafte Muster direkt: das
`[APP] Sending ...`-Log mit `kfactor_valid=0`/`temp=0` erschien **vor**
dem `[SPOOLMAN] Filament loaded ... valid=1`-Log derselben Anfrage-ID --
der falsche Wert wurde also verschickt, *bevor* die eigentliche Antwort
überhaupt eintraf. Ursache: `SpoolmanTask::loadSpools()` schickt nach
*jeder* `LoadSpool`/Such-Anfrage zusaetzlich zur eigentlichen Spule ein
Abschluss-Event (`"N Spulen gefunden"`, `value=-1`, leeres `spool`/
`filament`), das dieselbe `requestId` traegt wie die Anfrage selbst --
gedacht dafuer, dass der Spulen-Picker weiss, wann eine Suche fertig ist.
Jede Stelle, die nach einer `LoadSpool`-Antwort einen `LoadFilament`-
Folge-Request unter *derselben* `requestId` startet (Nutzerhinweis
2026-08-24: `AssignTray`s `SlotAssignmentStage::LoadingFilament`,
`AppTask::sendStagingUpdate()`s `PendingStagingFilamentLoad` fuers
Staging, und `resolveTraySpoolDetails()`s `TraySpoolDetailsEntry` fuer
die AMS-Tray-Karten) bekam dieses Abschluss-Event faelschlich als
"Filament-Antwort" serviert, weil es strukturell *immer* vor der
echten (HTTP-Roundtrip-gebundenen) Filament-Antwort in AppTasks eigener
Queue ankommt -- direkt nach der `LoadSpool`-Antwort, noch bevor der
`LoadFilament`-Request ueberhaupt beim Server war. Die betroffenen
Handler lasen daraus `value=-1` (`< 0`) und werteten das faelschlich als
"Fetch fehlgeschlagen/keine Daten", loeschten dabei aber ihren
Pending-State -- die echte, kurz danach eintreffende Antwort landete
dann in keinem Handler mehr und wurde stillschweigend verworfen. Fix in
allen drei Stellen: die Bedingung prueft jetzt zusaetzlich
`event.filament.id != 0` (nur eine echte, erfolgreich geparste
Filament-Antwort hat dieses Feld gesetzt; der Abschluss-Marker nie) --
das Abschluss-Event faellt jetzt unbehandelt durch, der Pending-State
bleibt bestehen, bis die echte Antwort eintrifft. Erklaert rueckwirkend
sowohl das urspruengliche `nozzle_temp_min=0 nozzle_temp_max=0`-Symptom
als auch den spaeteren "K-Faktor wird korrekt geladen, aber nicht
angezeigt"-Bug beim Staging (und vermutlich denselben Fehler,
unbemerkt, bei den AMS-Tray-Karten). Build (0 Warnungen), 51 native
Tests gruen, geflasht -- Hardware-Test steht noch aus.

`tray_info_idx` referenziert Bambus intern hinterlegte Filament-Profil-ID
("setting_id"). Ein erster Hardwaretest ohne dieses Feld (nur tray_type/
tray_color/nozzle_temp_*) hat den physischen Slot-Inhalt eines bereits
belegten Slots **nicht** geaendert, obwohl der Befehl mit korrekter
Adressierung ankam -- ein starkes Indiz, dass echte Firmware das Feld fuer
diesen Vorgang braucht. `BambuProtocol::resolveBambuMaterial()` bildet den
freien Spoolman-Materialtext (z. B. `PLA`, `PETG`, `PLA-CF`) ueber die zur
Laufzeit von der SD-Karte geladene Tabelle
(`models::BambuMaterialMappingTable`, siehe Nachtrag 2026-08-28 oben) auf
Bambu Studios eingebaute *generische* (nicht markenspezifische) Profil-IDs
ab -- community-dokumentiert ueber `Bambu-Research-Group/RFID-Tag-Guide`
und die WolfWithSword Home-Assistant-Bambu-Lab-Integration, nicht von
Bambu Lab selbst. Bekannte Zuordnungen (Auszug): PLA→GFL99, PLA-CF→GFL98,
PETG→GFG99, ASA→GFB98, ABS→GFB99, TPU→GFU99, PVA→GFS99, PC→GFC99,
PA→GFN99, PA-CF→GFN98 (vollstaendige, 54 Eintraege umfassende Tabelle
inkl. PETG-CF/PA-GF/PP-CF/PP-GF/PPA-CF/PPA-GF/PLA-AERO/ASA-CF sowie 14
`Support For ...`-Supportmaterialien, siehe
`data/bambu-materials/bambu_materials.json`). Das Matching ist
**exakt** (nach Normalisierung von Gross-/Kleinschreibung und Trennzeichen),
nicht praefixbasiert -- ein reines Praefix-Matching haette z. B. `PLA-CF`
faelschlich schon bei `PLA` treffen lassen. Ein nicht in der Tabelle
enthaltenes Material liefert `nullptr` statt eines erfundenen oder leeren
Werts; `BambuTask::handleAssignTray()` lehnt die gesamte Zuordnung in
diesem Fall ab, statt mit einem unvollstaendigen `tray_info_idx: ""` zu
senden (siehe Nachtrag 2026-08-28 oben).

### Unter Untersuchung: Ueberschreiben eines bereits belegten Slots wird zurueckgesetzt

Mit `tray_info_idx` und gueltiger Duesentemperatur, aber weiterhin fixem
`sequence_id: "0"` und 6-stelligem `tray_color` (ohne Alpha), nimmt der
Drucker die Zuordnung sichtbar an -- das neue Filament erscheint fuer ca.
3-5 Sekunden im AMS-Status --, wird danach aber automatisch wieder auf den
vorherigen Wert zurueckgesetzt. Per Hardwaretest bestaetigt (2026-08-22):
derselbe Effekt trat zu diesem Zeitpunkt identisch in Bambu Studio
(offizieller Client) auf. Das deutete zunaechst auf eine reine Drucker-/
AMS-Firmware-Ursache hin -- **aber** derselbe Test hatte zwei inzwischen
behobene Payload-Abweichungen von der Spezifikation: `sequence_id` wurde
nie erhoeht (OpenBambuAPI dokumentiert "incremented by 1 on each command")
und `tray_color` war nur 6-stellig statt der spezifizierten 8-stelligen
RRGGBBAA-Form. Ob eine dieser Abweichungen die eigentliche Ursache war, ist
nach diesen Fixes noch nicht erneut auf echter Hardware getestet. Ein
leerer Slot kann nicht getestet werden, da der Drucker fuer eine
Filamentanzeige eine physisch eingelegte Spule voraussetzt.

Weiterer Verdacht (Nutzer-Diagnose, 2026-08-22): `AppTask` hat bisher nach
jedem erfolgreichen `AssignTray` sofort ein `RequestStatus`/`pushall`
ausgeloest. Das fragt den Drucker ab, noch bevor dieser die neuen
AMS-Werte intern fertig verarbeitet und gespeichert hat -- der Drucker
sendet nach einer erfolgreichen Parameteraenderung ueblicherweise von sich
aus ein Status-Update an alle Clients, ein sofortiges Nachfragen ist daher
unnoetig und koennte die interne Verarbeitung stoeren. Fix: dieses
sofortige `RequestStatus` wurde ersatzlos entfernt; Home aktualisiert sich
stattdessen ueber den naechsten ohnehin periodisch eintreffenden
Statusbericht. Noch nicht auf echter Hardware verifiziert, ob dies (statt
oder zusaetzlich zu `sequence_id`/`tray_color`) die eigentliche Ursache
des Zuruecksetzens war.

### Vergleich mit FilaMan-System (2026-08-22)

Auf Nutzerwunsch wurde das vergleichbare Open-Source-Projekt
[Fire-Devils/filaman-system](https://github.com/Fire-Devils/filaman-system)
untersucht, konkret dessen tatsaechlicher Bambu-MQTT-Treiber im separaten
Repo `Fire-Devils/filaman-bambulab-plugin` (`bambulab/driver.py`). Dieser
Treiber sendet `ams_filament_setting` nicht selbst von Hand, sondern
delegiert an die Drittanbieter-Bibliothek `BambuTools/bambulabs_api`
(`mqtt_client.py::set_printer_filament()`), die in Produktivsystemen
nachweislich funktioniert. Feld-fuer-Feld-Vergleich mit
`BambuProtocol::bambuBuildAmsFilamentSetting()`:

* **Kein `sequence_id`-Feld.** `bambulabs_api` sendet bei
  `ams_filament_setting` (und auch bei `pushall`) ueberhaupt kein
  `sequence_id` im ausgehenden Payload -- `get_sequence_id()` liest den
  Wert dort nur aus eingehenden Statusberichten des Druckers, schreibt ihn
  nie zurueck. Das relativiert die eigene `sequence_id`-Theorie: eine
  nachweislich funktionierende Implementierung kommt komplett ohne dieses
  Feld aus. Unser fortlaufender Zaehler bleibt vorerst bestehen (zusaetzes
  Feld, vermutlich vom Drucker ignoriert), gilt aber nicht mehr als
  wahrscheinliche Ursache.
* **`tray_color`**: identisch -- 6-stelliger RRGGBB-Input, Bibliothek
  haengt `FF` an (`f"{colour.upper()}FF"`), genau wie unser Fix.
  `assert len(colour) == 6` bestaetigt zusaetzlich, dass der Drucker den
  8-stelligen Wert erwartet.
* **`tray_info_idx`**: identisch enthalten, kommt aus einer eigenen
  Materialtabelle (`AMSFilamentSettings`/`Filament`-Enum), analog zu
  unserer von der SD-Karte geladenen Tabelle/`resolveBambuMaterial()`.
* **`ams_id`/`tray_id`**: als einfache Ganzzahlen 0-basiert (Default fuer
  den externen Slot: `ams_id=255`, `tray_id=254`) -- keine zusaetzliche
  Adress-Transformation, deckt sich mit unserer 0-basierten Zaehlung.
* **Keine weiteren Felder**: kein `cali_idx`, kein `setting_id` im
  ausgehenden Kommando (beide werden in FilaMan nur beim *Auswerten*
  eingehender Statusberichte verwendet, nie beim Senden).
* **Kein Folgebefehl.** Der manuelle Zuweisungspfad
  (`send_filament_to_tray()`) sendet ausschliesslich
  `ams_filament_setting` -- kein `pushall`, kein `ams_change_filament`
  ("Filament laden") danach. Das deckt sich mit unserem bereits
  umgesetzten, aber noch nicht auf Hardware verifizierten
  Pushall-Entfernungs-Fix oben.

**Ergebnis:** Der aktuelle FilamentStation-Payload (nach den bisherigen
Fixes: `tray_info_idx`, temperaturen, 8-stelliges `tray_color`, entfernter
Pushall) stimmt strukturell mit einer nachweislich funktionierenden
Referenzimplementierung ueberein -- bis auf das zusaetzliche, vermutlich
harmlose `sequence_id`-Feld liefert der Vergleich **keinen Hinweis auf ein
fehlendes oder falsches Feld**.

Ein Hardwaretest (2026-08-22, nach diesem Vergleich) bestaetigte jedoch:
der Drucker sendet **zu keinem Zeitpunkt** -- auch nicht kurzzeitig -- einen
Report mit dem neuen Material; jeder Report ab 567ms nach dem Senden zeigt
durchgehend den alten Wert. Bambu Studio zeigt bei der gleichen Aktion das
neue Filament fuer 3-5 Sekunden an; da der P1S kein eigenes Display besitzt
und Bambu Studio seine Anzeige nachweislich aus MQTT-Reports des Druckers
bezieht (keine lokale Optimistic-UI), muss der Drucker Bambu Studios
Kommando **anders** verarbeiten als unseres.

### Zusätzlich behobene Payload-Lücke (2026-08-22): fehlendes `extrusion_cali_sel`-Folgekommando

(War zum Zeitpunkt dieses Fixes der aussichtsreichste Kandidat, hat das
Problem allein aber noch nicht gelöst -- die tatsächliche Ursache war
Developer Mode, siehe weiter unten. Das Kommando bleibt trotzdem korrekt
und nötig, siehe Referenzimplementierung.)

Zweiter Projektvergleich, auf Nutzerhinweis: `yanshay/spoolease`, eine
weitere ESP32-Firmware (Rust), die das gleiche LAN-Mode-Protokoll direkt
implementiert (kein Umweg ueber eine Drittanbieter-Bibliothek wie bei
FilaMan). Deren `core/src/bambu.rs::set_tray_filament()` sendet fuer eine
manuelle Slot-Zuweisung **zwei** Kommandos nacheinander, nicht nur eins:

1. `ams_filament_setting` (wie oben, inkl. `slot_id`).
2. **`extrusion_cali_sel`** ("Extrusions-Kalibrierung ausw\xC3\xA4hlen"):

```json
{
  "print": {
    "command": "extrusion_cali_sel",
    "cali_idx": -1,
    "filament_id": "GFL99",
    "nozzle_diameter": "0.4",
    "ams_id": 0,
    "tray_id": 0,
    "slot_id": 0,
    "sequence_id": "2"
  }
}
```

Quellcode-Kommentar in spoolease dazu (sinngem\xC3\xA4\xC3\x9F): das Setzen der
Filamentinfo allein reicht nicht aus, damit der Drucker sie \xC3\xBCbernimmt --
`extrusion_cali_sel` ist die zus\xC3\xA4tzlich erforderliche Best\xC3\xA4tigung/Auswahl
der zugeh\xC3\xB6rigen Extrusions-Kalibrierung (Flow-Ratio/Pressure-Advance) f\xC3\xBCr
diesen Slot. Ohne dieses Kommando bleibt die \xC3\x84nderung offenbar provisorisch
und wird vom Drucker nach kurzer Zeit verworfen -- exakt das beobachtete
Verhalten (Report zeigt nie den neuen Wert, weder transient noch dauerhaft).

Felder:

* `cali_idx`: Index eines bekannten Kalibrierungsprofils; `-1` wenn keines
  bekannt ist (spoolease-Kommentar: "If we don't [have one] we send None
  and it seems to work" -- der Standardfall, da FilamentStation aktuell
  keine Spoolman-Kalibrierungsdaten je Spule verfolgt).
* `filament_id`: derselbe Wert wie `tray_info_idx` im ersten Kommando
  (z. B. `"GFL99"`), hier nur unter anderem Feldnamen.
* `nozzle_diameter`: String (z. B. `"0.4"`), aus dem Statusbericht-Feld
  `print.nozzle_diameter` des Druckers selbst -- neu in `PrinterState`
  (`nozzleDiameter`) nachgezogen. Ist noch kein Bericht mit diesem Feld
  eingetroffen, wird der Standardwert `"0.4"` verwendet (Standarddüse aller
  Bambu-AMS-kompatiblen Modelle) statt eine leere Zeichenkette zu senden.
* `ams_id`/`tray_id`/`slot_id`: identisch zum vorausgehenden
  `ams_filament_setting`.

Implementiert in `BambuProtocol::bambuBuildExtrusionCaliSel()`; wird von
`BambuTask::handleAssignTray()` unmittelbar nach einem erfolgreich
gesendeten `ams_filament_setting` ausgel\xC3\xB6st (gleiche fortlaufende
`sequence_id`).

### Tatsächliche Ursache (2026-08-22): Developer Mode / MQTT Command Verification

Der obige `extrusion_cali_sel`-Fix allein löste das Problem noch nicht.
Externe Zweitmeinung fand die tatsächliche Ursache: aktuelle P1S-Firmware
prüft Steuerkommandos (nicht Lesezugriffe) kryptografisch, sobald der
Drucker regulär mit der Bambu-Cloud gekoppelt ist. Ein reiner
`bblp`+Access-Code-Login reicht dann zum Abonnieren/Lesen, aber nicht mehr
zum Schreiben -- der Drucker lehnt das Kommando ab (`HMS_0500_0500_0001_0007`,
"MQTT command verification failed"), ohne dass unser bisheriger Report-Parser
das sichtbar gemacht hätte (siehe unten). **Fix: Developer Mode am Drucker
selbst aktivieren** (WLAN/LAN-Einstellungen -> LAN Only/LAN Mode -> Developer
Mode); danach akzeptiert der Drucker normale LAN-Mode-MQTT-Schreibkommandos
wieder. Kein ESP32-seitiger Fix möglich/nötig -- eine Signaturmechanik
nachzubauen würde private Slicer-Zertifikate erfordern. **Bestätigt durch
Hardwaretest: mit aktiviertem Developer Mode funktioniert die
Slot-Zuordnung.**

### Nachträglich behobene Diagnose-/Adressierungsfehler (2026-08-22)

Zwei weitere, von der externen Zweitmeinung gefundene Punkte wurden
unabhängig vom eigentlichen Fix behoben:

* **Fehlendes Diagnose-Logging**: `bambuApplyReport()` wertet nur `ams`,
  `vt_tray` und `nozzle_diameter` aus; `command`/`result`/`reason`/
  `err_code` einer Kommando-Antwort (z. B. die obige Ablehnung) wurden
  bisher nirgends geloggt -- der Report wurde trotzdem klaglos als
  "Statusbericht empfangen" verarbeitet. `BambuTask::handleReportPayload()`
  loggt jetzt zusätzlich den kompletten rohen Eingangs-Payload (`%.*s`,
  Debug-Level) sowie, falls `print.command` gesetzt ist,
  `command`/`result`/`reason`/`err_code` separat (Info-Level) -- noch bevor
  `bambuApplyReport()` aufgerufen wird.
* **`extrusion_cali_sel`-Adressierungsfehler**: das Feld `tray_id` ist bei
  diesem Kommando (anders als bei `ams_filament_setting`) der **globale**
  Tray-Index über alle AMS-Einheiten (`ams_id * kSlotsPerAms + lokaler
  Slot-Index`), nicht der lokale Index innerhalb der AMS-Einheit. Verifiziert
  gegen `yanshay/spoolease`s `get_quad_for_set_filament_from_tray_id()`, die
  für `ExtrusionCaliSelCommand::new()` explizit den ungeteilten globalen
  Index übergibt, für `AmsFilamentSettingCommand::new()` dagegen den lokalen.
  Wirkt sich bei einer einzelnen AMS-Einheit (`ams_id=0`) nicht aus (global
  == lokal), war für Mehr-AMS-Setups aber falsch. `slot_id` bleibt der
  lokale Wert.

### AssignTray-Best\xC3\xA4tigung ueber Drucker-Telemetrie statt `publish()==true` (2026-08-22)

Ein dritter, von derselben Zweitmeinung aufgezeigter Punkt: `publish()`
liefert nur zurueck, ob der MQTT-Broker das Paket angenommen hat, nicht ob
der Drucker es tatsaechlich angewendet hat -- eine Ablehnung (z. B. die oben
beschriebene Command-Verification ohne Developer Mode) sieht auf
MQTT-Ebene identisch aus wie eine Annahme. `BambuTask::handleAssignTray()`
meldete bisher direkt nach erfolgreichem `publish()` beider Kommandos
Erfolg an `AppTask` ("Slotdaten gesendet") und uebernahm die
Spoolman-Zuordnung (`spoolId`) sofort ins eigene `PrinterState` --
unabhaengig davon, ob der Drucker die \xC3\x84nderung je best\xC3\xA4tigte.

Neuer Ablauf: `PrinterConnection::pending` (`PendingTrayAssignment`) merkt
sich nach dem Senden die erwarteten Werte (`expectedTrayType`,
`expectedColorHex` -- ueber dieselbe `bambuNormalizeTrayColorHex()`-Funktion
normalisiert, die auch der Payload-Builder nutzt, damit beide exakt
denselben Wert vergleichen) statt sofort Erfolg zu melden.

* `checkPendingTrayAssignment()` (aufgerufen aus `handleReportPayload()`
  nach jedem erfolgreich angewendeten Report) vergleicht die
  Drucker-Telemetrie fuer den betroffenen Slot (`material`/`colorHex`)
  gegen die erwarteten Werte. Bei einer Uebereinstimmung wird die
  Spoolman-Zuordnung erst jetzt committet und `AppEventType::BambuUpdate`
  mit der urspruenglichen `requestId` an `AppTask` gemeldet ("Slot vom
  Drucker best\xC3\xA4tigt").
* Bleibt die Best\xC3\xA4tigung nach `kBambuAssignConfirmTimeoutMs` (8000\xC2\xA0ms,
  `config/BambuConfig.h`) aus, meldet `serviceConnections()`
  `AppEventType::BambuError` ("Drucker hat die Slot\xC3\xA4nderung nicht
  best\xC3\xA4tigt") -- sichtbar als Fehlerdialog statt eines falschen
  Erfolgs.
* Ein noch offenes Pending wird zusaetzlich explizit fehlgeschlagen
  (`failPendingAssignment()`), statt kommentarlos verworfen zu werden, bei:
  Verbindungsabbruch (`disconnectPrinter()`, jetzt mit `RtosContext&`-
  Parameter), Verbindungsverlust (`serviceConnections()`), und einer neuen
  `AssignTray` waehrend eine vorherige Best\xC3\xA4tigung noch aussteht --
  ansonsten wuerde die Fortschrittsanzeige in der UI unter der jeweiligen
  `requestId` unbegrenzt haengen bleiben.
* Best\xC3\xA4tigungskriterium ist bewusst nur `tray_type`/`tray_color` (das,
  was `bambuApplyReport()` bereits aus Reports parst), nicht
  `tray_info_idx` oder Duesentemperaturen -- Letztere werden aktuell nicht
  aus Reports geparst; eine Erweiterung waere bei Bedarf in
  `BambuProtocol::bambuApplyReport()`/`PrinterSlotStateData` nachzuziehen.

### Fortschrittsanzeige mit Countdown fuer die Best\xC3\xA4tigungswartezeit (2026-08-22)

Die 8-Sekunden-Wartezeit auf die Drucker-Best\xC3\xA4tigung (siehe oben) war
zuvor nur ein statischer "Wird an den Drucker \xC3\xBCbertragen"-Text ohne
Zeitangabe. `BambuTask::serviceConnections()` sendet jetzt zusaetzlich, auf
einmal pro Sekunde gedrosselt (`PendingTrayAssignment::
lastReportedRemainingSeconds`), ein `AppEventType::BambuAssignProgress`
mit der verbleibenden Zeit in Millisekunden (`AppEvent::value`).
`AppTask` leitet das nur weiter, solange `pendingSlotAssignment.stage ==
WritingSlot` und die `requestId` noch aktuell ist, als neuen
`UiCommandType::UpdateProgress` (Prozentwert + Text "Warte auf
Best\xC3\xA4tigung vom Drucker \xE2\x80\x93 noch N s") an `UiTask`.
`UiBridge::processUiCommand()` aktualisiert damit den bereits vorhandenen,
zuvor nie tats\xC3\xA4chlich bewegten `overlayProgress`-Balken (fr\xC3\xBCher
dekorativ fix auf 60 %) -- nur solange der Dialog noch fuer dieselbe
`requestId` offen ist.

## Statusberichte (Drucker -> Client)

Berichte auf dem Report-Topic sind JSON-Objekte mit einem `print`-Schluessel.
Nachrichten ohne `print`-Objekt werden ignoriert (andere Nachrichtentypen,
z. B. `system`, werden derzeit nicht ausgewertet).

Von `BambuProtocol::bambuApplyReport()` ausgewertete Pfade:

* `print.ams.ams[]`: Liste der AMS-Einheiten.
  * `id`: AMS-Index (0-basiert). Bambu liefert numerische IDs haeufig als
    JSON-String (`"0"` statt `0`); beide Formen werden akzeptiert.
  * `tray[]`: vier Slots je AMS-Einheit, ebenfalls mit `id`.
    * `tray_type`: Materialkuerzel (z. B. `"PLA"`), leer oder fehlend bei
      leerem Slot. Wird nach `PrinterSlotStateData::material` uebernommen.
    * `tray_color`: RRGGBBAA-Hex-String. Wird unveraendert (als String, keine
      Farbkonvertierung) nach `PrinterSlotStateData::colorHex` uebernommen.
  * **Nachtrag (2026-08-24, physisch entfernte AMS-Einheit, Robustheit/
    Diagnose TASKS.md 10.5):** `ams.ams[]` erscheint laut obiger Beobachtung
    (Abschnitt "Vollstaendigen Status anfordern") nur bei einem vollen
    `pushall`, nicht beim regulaeren periodischen `push_status` -- wenn das
    Array in einem Bericht vorkommt, gilt es daher als vollstaendige,
    massgebliche Momentaufnahme aller tatsaechlich angeschlossenen
    AMS-Einheiten (unverifizierte Annahme, community-basiert wie der Rest
    dieser Datei). `bambuApplyReport()` merkt sich deshalb bei jedem
    vorhandenen `ams.ams[]`, welche `id`s darin vorkommen, und setzt fuer
    jede zuvor als `present` bekannte, jetzt fehlende Einheit
    `present=false`/`connectionState=Offline` (plus neu berechnetes
    `amsCount`) -- vorher wurde `present` nur je gesetzt, nie zurueckgesetzt,
    eine abgezogene AMS-Einheit blieb dadurch fuer die restliche Sitzung
    faelschlich als angeschlossen sichtbar. Ein Bericht *ohne* `ams.ams[]`
    (regulaeres `push_status`) aendert an der Anwesenheit bewusst nichts
    (gleiches Merge-Verhalten wie der Rest dieser Funktion).
* `print.vt_tray`: externer/manueller Slot (kein AMS), gleiche Feldstruktur
  wie ein Tray-Eintrag. Wird auf `PrinterState::externalSlot` abgebildet.
* `print.ams.tray_now`: welches Fach gerade in der Duese aktiv ist
  (Nutzerwunsch 2026-08-24), Zahl oder String (beide Formen akzeptiert wie
  bei `id` oben). Globale Adressierung ueber alle AMS-Einheiten hinweg:
  `0..15` = `amsId * kSlotsPerAms + trayId` (`0..3` = AMS 0 Fach 0..3,
  `4..7` = AMS 1, usw.), `254` = externe Spule/`vt_tray`
  (`models::kActiveTrayNowExternal`), `255` = kein Fach aktiv
  (`models::kActiveTrayNowNone`). Wird nach `PrinterState::activeTrayNow`
  uebernommen; fehlt das Feld in einem Bericht, bleibt der zuletzt bekannte
  Wert stehen (gleiches Merge-Verhalten wie der Rest der Funktion). Treibt
  die Duesen-Icon-Anzeige der Home-Tray-Karten (`UiBridge.cpp`s
  `updateTrayButton()`) -- vorher eine Mockformel ("erstes belegtes Fach je
  AMS gilt als aktiv").
* `print.nozzle_diameter`: String (z. B. `"0.4"`), wird nach
  `PrinterState::nozzleDiameter` uebernommen -- benoetigt fuer das
  `extrusion_cali_sel`-Kommando (siehe oben).
* `print.nozzle_type` (2026-08-28, K-Faktor-Upload): String (z. B.
  `"hardened_steel"`), wird nach `PrinterState::nozzleType` uebernommen --
  **unverifiziert, ob/in welcher Form dieser Drucker es sendet** (nur aus
  Community-Dokumentation uebernommen, nie an echter Hardware bestaetigt).
  Solange dieses Feld nie eintrifft, bleibt `PrinterState::nozzleType` leer
  und der K-Faktor-Upload faellt fuer diesen Drucker dauerhaft aus
  (fail-closed, siehe "K-Faktor-Upload" weiter unten) -- alles andere
  bleibt davon unberuehrt.

**Bewusst nicht ausgewertet:** `tray_id_name`. Ein Versuch, eine Spoolman-
`spoolId` daraus aufzuloesen ("SM:<spoolmanId>", siehe "ams_filament_setting"
oben), wurde per Hardwaretest widerlegt (2026-08-23) -- der Drucker gibt
dieses Feld immer leer zurueck, auch innerhalb derselben Session kurz nach
dem Schreiben. `PrinterSlotStateData` hat seitdem bewusst kein `spoolId`-Feld
mehr (siehe oben, "Ansatz komplett verworfen"); die Spoolman-Zuordnung wird
stattdessen komplett ausserhalb dieser Datei verwaltet, siehe
`models/TraySpoolCache.h` und `AppTask::resolveTraySpoolCacheSpoolId()`.

**Nicht implementiert** (ausserhalb des Funktionsumfangs von Phase 8.3):
Druckfortschritt, Kamera/AI-Erkennung, Temperaturen, Firmwareversion,
Fehlercodes des Druckers. Kann bei Bedarf in `bambuApplyReport()` ergaenzt
werden, sobald die entsprechenden Felder verifiziert sind.

### K-Faktor-Upload (2026-08-28): `extrusion_cali_set` -> `extrusion_cali_get` -> `extrusion_cali_sel`

Bisher (siehe oben, Nutzerwunsch 2026-08-24) wurde der Spoolman-K-Faktor
(`flow_dynamics_k_factor`) nur auf der Home-Tray-Karte angezeigt, nie an
den Drucker gesendet -- eine bewusste Scope-Entscheidung, die dieser
Nachtrag jetzt umsetzt. **Hardware-unverifiziert**, wie der Rest dieser
Datei -- community-reverse-engineert aus `yanshay/spoolease`.

Das bereits implementierte `extrusion_cali_sel` (siehe oben) kann selbst
keinen K-Wert transportieren, es waehlt nur ein bereits auf dem Drucker
vorhandenes Kalibrierungsprofil per `cali_idx` aus (`-1` = keines). Ein
neues Profil mit einem frei waehlbaren K-Wert anzulegen erfordert zwei
zusaetzliche Kommandos:

1. **`extrusion_cali_set`**: legt ein neues Kalibrierungsprofil an.

```json
{
  "print": {
    "command": "extrusion_cali_set",
    "filaments": [{
      "ams_id": 0, "extruder_id": 0, "filament_id": "GFL99",
      "k_value": "0.123000", "n_coef": "0.000000",
      "name": "FilamentStation #1234", "nozzle_diameter": "0.4",
      "nozzle_id": "hardened_steel-0.4", "setting_id": "FS000004D2",
      "slot_id": 2, "tray_id": -1
    }],
    "nozzle_diameter": "0.4", "sequence_id": "10"
  }
}
```

* `setting_id`: **selbst vergeben**, nicht vom Drucker (das
  `yanshay/spoolease`-Beispiel, aus dem dieser Payload uebernommen wurde,
  liefert diesen Wert selbst auf dem Create-Request mit). Deterministisch
  pro Spule gebildet (`"FS" + 8 Hex-Ziffern der Spoolman-`spoolId`), damit
  eine erneute Zuweisung derselben Spule dasselbe Profil trifft statt
  verwaiste Duplikate auf dem Drucker anzuhaeufen.
* `nozzle_id`: best-effort aus `PrinterState::nozzleType` +
  `PrinterState::nozzleDiameter` zusammengesetzt (`"<nozzle_type>-<nozzle_diameter>"`).
  **Keine verifizierte Typ-Code-Zuordnung bekannt** (das
  `yanshay/spoolease`-Beispiel zeigt `"HS00-0.4"`, also vermutlich einen
  Kurzcode statt des rohen Telemetrie-Strings) -- absichtlich trotzdem so
  gewaehlt statt eine geratene Codetabelle zu erfinden, siehe "Bekannte
  Risiken" oben.
* `n_coef`: immer `"0.000000"`, kein entsprechendes Spoolman-Filament-Feld
  vorhanden.
* `filaments[0].tray_id`: immer `-1` -- das Profil wird durch dieses
  Kommando noch keinem Slot zugewiesen, das macht erst Schritt 3.

2. **`extrusion_cali_get`**: fragt die komplette Liste der auf dem Drucker
   vorhandenen Kalibrierungsprofile fuer einen Duesendurchmesser ab
   (`print.filament_id` bleibt dabei immer leer, ungefiltert). Wird
   unmittelbar nach `extrusion_cali_set` gesendet, ohne auf ein Echo zu
   warten (gleiche Annahme wie beim bestehenden
   `ams_filament_setting`+`extrusion_cali_sel`-Paar: MQTT/TCP erhaelt die
   Reihenfolge). Die Antwort kommt asynchron auf dem Report-Topic zurueck
   (`print.command == "extrusion_cali_get"`); `services::
   bambuFindCalibrationBySettingId()` durchsucht `print.filaments[]` nach
   dem selbst vergebenen `setting_id` und liest bei Treffer `cali_idx`
   aus. **Das Feld `print.filaments[]` in der Antwort ist ebenfalls nur
   aus `yanshay/spoolease`s Antwortverarbeitung uebernommen, nie an einer
   echten Antwort dieses Projekts verifiziert** -- der wahrscheinlichste
   Punkt, der nach dem ersten Hardwaretest korrigiert werden muss.

3. **`extrusion_cali_sel`** (siehe oben) mit dem so gefundenen `cali_idx`
   statt `-1` -- identisches Kommando, nur mit dem echten Index.

Implementiert in `BambuTask::handleAssignTray()` (Schritte 1+2, plus
`PendingCalibrationAssignment`-Tracking) und
`BambuTask::handleReportPayload()` (Schritt 3, sobald die
`extrusion_cali_get`-Antwort eintrifft). **Vollstaendig entkoppelt** von
der bereits mehrfach hardware-validierten AssignTray-Bestaetigung
(`PendingTrayAssignment`): fehlt der K-Faktor, hat der Drucker noch nie
`print.nozzle_type` gemeldet, oder laeuft die Kalibrierungssuche nach
`kBambuCalibrationTimeoutMs` (8 s, wie `kBambuAssignConfirmTimeoutMs`) in
den Timeout, wird nur eine `FS_LOGW`-Zeile geschrieben -- kein Fehler an
die UI, kein Einfluss auf die normale Slotzuweisung (Material/Farbe), die
in diesem Fall unveraendert `extrusion_cali_sel(cali_idx=-1)` sendet wie
vor dieser Funktion.

Der K-Faktor wird ueber denselben `LoadSpool`->`LoadFilament`-Weg geholt
wie die Home-Tray-Karten-Anzeige (`SpoolmanCommandType::LoadFilament`,
`event->filament.bambuKFactorValid`/`bambuKFactor`) -- `AppTask`s
`SlotAssignmentStage` bekam dafuer eine neue Zwischenstufe
`LoadingFilament` zwischen `LoadingSpool` und `WritingSlot` (dieselbe
`event.filament.id != 0`-Guard-Falle wie bei `pendingStagingFilamentLoad`
beachtet, siehe Nachtrag 2026-08-24 oben zum Spoolman-Listenabschluss-
Marker).

**Kein Hardware-Test dieser Funktion in der Einfuehrungssitzung
moeglich.**

## Bekannte Risiken

* Feldnamen und Nummerierungen (insbesondere die spezielle ID des externen
  Trays) sind nicht an echter Hardware verifiziert.
* Die Groesse eines vollstaendigen Reports (mehrere KB bei vier bestueckten
  AMS-Einheiten) kann `MQTT_MAX_PACKET_SIZE` ueberschreiten, falls der reale
  Payload groesser als angenommen ist; ggf. muss der Wert in
  `platformio.ini` erhoeht werden.
* `setInsecure()` verzichtet auf Transportsicherheit ueber die reine
  TLS-Verschluesselung hinaus (kein Server-Identitaetsnachweis). Das ist im
  vertrauenswuerdigen LAN mit Access-Code-Auth ein akzeptierter Kompromiss,
  aber kein Schutz gegen einen bereits kompromittierten LAN-Teilnehmer.
* Der K-Faktor-Upload (`extrusion_cali_set`/`extrusion_cali_get`, siehe
  "K-Faktor-Upload" weiter unten) ist zum Zeitpunkt seiner Einfuehrung
  (2026-08-28) an **keiner** echten Hardware getestet worden. Insbesondere
  das `nozzle_id`-Feld (best-effort aus dem unverifizierten
  `print.nozzle_type`-Telemetriewert zusammengesetzt) und die genaue
  Feldstruktur der `extrusion_cali_get`-Antwort (`print.filaments[]`,
  nur aus `yanshay/spoolease`s Quellcode uebernommen, nie an einer echten
  Antwort dieses Projekts verifiziert) sind die wahrscheinlichsten Punkte,
  die nach dem ersten echten Test korrigiert werden muessen.
