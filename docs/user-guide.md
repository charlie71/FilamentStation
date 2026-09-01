# Benutzeranleitung

Diese Anleitung richtet sich an Nutzer des fertigen Geräts, nicht an
Entwickler. Technische Hintergründe stehen in den übrigen `docs/*.md`-Dateien
und werden hier nur verlinkt, wenn sie für die Bedienung wichtig sind.

## Installation

FilamentStation benötigt für den Betrieb eine eingesetzte SD-Karte. Beim
ersten Start werden die benötigten Verzeichnisse und Konfigurationsdateien
automatisch angelegt -- es ist keine manuelle Vorbereitung der Karte nötig
(Details: `docs/storage.md`).

Nach dem Einschalten zeigt das Display kurz den Start-/Diagnosebildschirm.
Sind noch keine WLAN-Zugangsdaten gespeichert, öffnet sich anschließend
automatisch das WLAN-Konfigurationsportal (siehe nächster Abschnitt) -- das
ist bei einem neuen oder zurückgesetzten Gerät normal und kein Fehler.

Empfohlene Ersteinrichtungsreihenfolge:

1. WLAN verbinden.
2. Spoolman-Server hinterlegen und testen.
3. Waage tarieren (und bei Bedarf kalibrieren).
4. Mindestens einen Drucker anlegen, falls Bambu-Drucker im Einsatz sind.

## WLAN

**Erste Verbindung:** Solange kein WLAN gespeichert ist, spannt das Gerät
selbst ein Netzwerk auf, dessen Name (SSID) mit `FilamentStation-` beginnt,
gefolgt von sechs geräteindividuellen Zeichen (z. B.
`FilamentStation-A1B2C3`). Das zugehörige Passwort beginnt entsprechend mit
`FS-` (z. B. `FS-A1B2C3`). Mit einem Smartphone oder Laptop mit diesem
Netzwerk verbinden -- es öffnet sich (oder muss im Browser aufgerufen
werden) eine Konfigurationsseite, auf der das eigentliche Heim-WLAN und
dessen Passwort eingegeben werden. Nach erfolgreicher Verbindung übernimmt
das Gerät die neuen Zugangsdaten dauerhaft.

**Später ändern:** Einstellungen → WLAN zeigt den aktuellen Status (SSID,
IP-Adresse, Signal). Zwei Aktionen stehen zur Verfügung:

* **Neu konfigurieren** -- öffnet erneut das Konfigurationsportal, ohne die
  bisherigen Zugangsdaten zu löschen (z. B. um ein zusätzliches/anderes WLAN
  einzurichten).
* **Zugangsdaten löschen** -- entfernt die gespeicherten Zugangsdaten
  vollständig; das Gerät öffnet beim nächsten Start wieder automatisch das
  Portal wie bei der Ersteinrichtung.

## Spoolman

Einstellungen → Spoolman zeigt die Serververbindung (Name, Protokoll, Host,
Port, Basispfad, Timeout). Nach dem Eintragen der eigenen Spoolman-Adresse:

1. **Testen** antippen -- prüft die Erreichbarkeit, ohne etwas zu speichern.
2. **Speichern** antippen -- übernimmt die Einstellung dauerhaft.

Der Status darunter zeigt "online"/"offline" sowie die erkannte
Spoolman-Serverversion. Solange Spoolman offline ist, bleiben alle
Funktionen gesperrt, die eine Online-Verbindung benötigen (Tag zuordnen,
Spule suchen, wiegen, Slot konfigurieren, Firmware-Import) -- ein Versuch
zeigt dazu direkt einen Hinweisdialog. Rein lokale Funktionen (Navigation,
WLAN-/Waagen-/Drucker-Einstellungen) bleiben davon unberührt.

## Extra-Feld `tag`

FilamentStation speichert jede Tag-Zuordnung ausschließlich als Textwert im
Spoolman-Extrafeld `tag` der jeweiligen Spule -- es gibt keine eigene
Zuordnungsdatenbank auf dem Gerät. Dieses Feld muss nicht manuell in
Spoolman angelegt werden: sobald die Serververbindung erfolgreich getestet
wird, legt FilamentStation das Feld bei Bedarf automatisch an.

Eine manuelle Korrektur in Spoolman ist nur in einem Fall nötig: existiert
in der eigenen Spoolman-Installation bereits ein Extrafeld namens `tag` mit
einem anderen Typ als "Text" (z. B. aus einer anderen Anwendung), meldet
FilamentStation das als Fehler ("Spoolman extra field 'tag' must have type
text") -- das bestehende Feld muss dann in Spoolman selbst in den Typ "Text"
geändert oder umbenannt werden, bevor Tag-Zuordnungen funktionieren.

## Waage

Einstellungen → Waage zeigt das aktuelle Gewicht live sowie den
Kalibrierstatus. Drei Aktionen:

* **Tarieren** -- setzt den aktuellen Nullpunkt (leere Waage, bevor eine
  Spule aufgelegt wird).
* **Kalibrieren** -- ein bekanntes Referenzgewicht auf die Waage legen, den
  Wert in Gramm über die eingeblendete Zahlentastatur eingeben und
  bestätigen. Je genauer das Referenzgewicht bekannt ist, desto genauer
  werden alle späteren Messungen.
* **Zurücksetzen** -- verwirft eine vorhandene Kalibrierung wieder.

## NFC

Ein Tag einfach an das Gerät halten -- FilamentStation erkennt Technologie
und Format automatisch und zeigt das Ergebnis ohne weiteres Zutun an. Je
nach erkanntem Format erscheint einer von mehreren Bildschirmen:

* Ein **eigener FilamentStation-Tag** (bereits zugeordnet oder nicht) öffnet
  die Aktionsauswahl zum Zuordnen/Entfernen.
* Ein **Bambu-, OpenPrintTag- oder OpenTag3D-Tag** zeigt entweder die
  bereits bestehende Zuordnung oder (falls neu) die vom Tag gelesenen
  Filamentdaten zur Übernahme nach Spoolman an. Diese Tags werden von
  FilamentStation grundsätzlich nie beschrieben oder gelöscht.
* Ein **Legacy-Tag** (älteres `spool:<id>`-Format) zeigt die bekannte
  Spoolman-ID und bietet optional die Migration auf das aktuelle Format an.
* Ein **unbekanntes Tag** zeigt nur die technischen Rohdaten (Technologie,
  UID, Schreibfähigkeit) -- eine Zuordnung per UID ist trotzdem möglich,
  auch wenn Format/Inhalt nicht interpretiert werden können.

## Tag zuordnen

Auf dem Ergebnisbildschirm eines Tags **Tag zuordnen** antippen und die
gewünschte Spule wählen (zuletzt verwendete Spule oder Suche). Bei einem
eigenen, sicher beschreibbaren FilamentStation-Tag (NTAG213/215/216) wird
dabei zusätzlich der native Tag-Inhalt geschrieben und geprüft; bei allen
anderen Formaten (Bambu, OpenPrintTag, OpenTag3D, Legacy-Erhalt) bleibt der
physische Tag unverändert -- nur die Zuordnung in Spoolman wird gesetzt.

## Tag-Zuordnung entfernen

**Tag-Zuordnung entfernen** auf demselben Bildschirm hebt die Verbindung
zwischen Tag und Spule wieder auf. Bei einem eigenen, sicher beschreibbaren
Tag wird dabei zusätzlich der native Inhalt vom Tag gelöscht; bei allen
anderen Formaten bleibt der physische Tag unangetastet, es wird nur die
Spoolman-Zuordnung entfernt.

## Bambu importieren

Ein aufgelegter original Bambu-Lab-RFID-Tag (in einer originalen
Bambu-Spule) wird immer nur gelesen, nie beschrieben. Ist die enthaltene
Tray-Kennung noch keiner Spoolman-Spule zugeordnet, zeigt FilamentStation
die vom Tag gelesenen Daten (Hersteller, Filament, Material, Farbe,
Nenngewicht) zur Übernahme an. Nach **importieren** fragt ein zusätzlicher
Bildschirm nach dem Leergewicht der Spule (niedrig-/hochtemperatur-typisch
oder manuell), da Bambu-Tags dieses Feld selbst nicht enthalten -- danach
wird in Spoolman eine neue Spule angelegt.

## Drucker

Einstellungen → Drucker verwaltet die hinterlegten Bambu-Drucker:

* **+ Neu** legt einen neuen Eintrag an: Anzeigename, Host/IP-Adresse,
  Seriennummer und LAN-Access-Code. Host/IP, Seriennummer und der
  LAN-Access-Code stehen in den Netzwerkeinstellungen des Druckers selbst.
* **Testen** prüft die Verbindung, bevor gespeichert wird.
* **Aktiv** markiert den Drucker, mit dem gerade gearbeitet wird
  (Druckerwechsel ist jederzeit über die Druckerauswahl auf der Startseite
  möglich, ohne dass Staging oder die Daten anderer Drucker verloren gehen).
* **Ein/Aus** blendet einen Drucker aus der Auswahl aus, ohne ihn zu löschen.
* **Standard** legt fest, welcher Drucker nach einem Neustart automatisch
  aktiv ist.
* **Ändern**/**Löschen** bearbeiten bzw. entfernen einen Eintrag.

**Wichtige Voraussetzung am Drucker:** Damit FilamentStation Slots
tatsächlich beschreiben kann (nicht nur lesen), muss am Drucker selbst der
**Developer Mode** aktiviert sein (Drucker-Touchscreen: WLAN-/LAN-
Einstellungen → LAN Only/LAN Mode → Developer Mode). Ohne aktivierten
Developer Mode lehnt aktuelle Bambu-Firmware Schreibkommandos wie eine
Slot-Zuordnung kryptografisch ab, auch wenn Verbindung und Access-Code
korrekt sind (per Hardwaretest bestätigt, siehe `docs/bambu-protocol.md`).
Reines Lesen des Status funktioniert auch ohne Developer Mode.

## AMS

Die Startseite zeigt für den aktiven Drucker eine Übersicht aller AMS-Einheiten
und Trays (inkl. eines externen Slots für manuell eingelegtes Filament ohne
AMS). Ein Tray antippen öffnet die Detailansicht (Material, Farbe,
Spoolman-Zuordnung, Restgewicht, sofern zuordenbar); von dort aus stehen die
Slot-Aktionen zur Verfügung:

* **Slot konfigurieren** -- die aktuell angelegte Spule (Staging) diesem
  Slot zuweisen.
* **Erneut anwenden** -- dieselbe, dem Slot bereits bekannte Spule erneut
  zuweisen (z. B. nach einer Unterbrechung).
* **Zuordnung entfernen** -- nur die Spoolman-Verknüpfung lösen, der vom
  Drucker gemeldete physische Inhalt (Material/Farbe) bleibt unverändert.
* **Slot zurücksetzen** -- den physischen Slot am Drucker leeren.
* **Slot aktualisieren** -- den aktuellen Zustand ohne Änderung neu abrufen.

Eine gespeicherte Zuordnung gilt nur so lange als sicher, wie Material und
Farbe, die der Drucker aktuell meldet, noch zu dem passen, was bei der
Zuordnung erfasst wurde -- wurde die physische Spule am Drucker außerhalb
von FilamentStation gewechselt, zeigt der Slot die Zuordnung als unbekannt
("?") an, statt eine veraltete Zuordnung weiter anzuzeigen.

## Firmware

Einstellungen → Firmware zeigt die installierte Version. **Nach Update
suchen** antippen:

* Ist bereits die neueste Version installiert, erscheint ein entsprechender
  Hinweis.
* Ist eine neuere Version verfügbar, wird das angezeigt; erneutes Antippen
  öffnet den Installationsdialog.
* **Bestätigen** startet Download und Installation mit Fortschrittsanzeige.
  Die heruntergeladene Firmware wird vor der Installation per Prüfsumme
  verifiziert -- schlägt das fehl, wird nichts installiert.
* Nach erfolgreicher Installation erscheint eine Neustart-Bestätigung; erst
  nach dem Neustart läuft die neue Version tatsächlich.

Eine ausführliche Schritt-für-Schritt-Anleitung zum Veröffentlichen und
Testen eines Updates (für Entwickler, nicht für den Endnutzer) steht in
`TASKS.md`, Phase 13.8.
