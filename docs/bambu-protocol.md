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
{"pushing":{"sequence_id":"0","command":"pushall"}}
```

Wird bei `verbinden` und bei `RequestStatus` auf das Request-Topic
veroeffentlicht.

### AMS-/Slot-Filamentdaten schreiben ("ams_filament_setting")

```json
{
  "print": {
    "sequence_id": "0",
    "command": "ams_filament_setting",
    "ams_id": 0,
    "tray_id": 0,
    "tray_type": "PLA",
    "tray_color": "FFFFFFFF",
    "nozzle_temp_min": 190,
    "nozzle_temp_max": 240
  }
}
```

`tray_color` ist ein 8-stelliger Hex-String (RRGGBBAA). Diese Nutzlast wird
von `AssignTray`/"Slotdaten schreiben" gesendet; die Zuordnung
Spoolman-Spule -> Filamenttyp/-farbe erfolgt in einer spaeteren Phase
(8.5, AMS-Zuordnung) und wird `BambuTask` fertig aufbereitet uebergeben.

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
* `print.vt_tray`: externer/manueller Slot (kein AMS), gleiche Feldstruktur
  wie ein Tray-Eintrag. Wird auf `PrinterState::externalSlot` abgebildet.

**Bewusst nicht ausgewertet:** `spoolId` je Slot. Der Drucker kennt keine
Spoolman-IDs; welche Spoolman-Spule einem Slot zugeordnet ist, verwaltet die
Anwendung selbst (Phase 8.5). `bambuApplyReport()` aendert `spoolId` in
`PrinterSlotStateData` daher nie.

**Nicht implementiert** (ausserhalb des Funktionsumfangs von Phase 8.3):
Druckfortschritt, Kamera/AI-Erkennung, Temperaturen, Firmwareversion,
Fehlercodes des Druckers. Kann bei Bedarf in `bambuApplyReport()` ergaenzt
werden, sobald die entsprechenden Felder verifiziert sind.

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
