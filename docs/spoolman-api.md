# Spoolman API

## Konfiguration und Verbindungstest

Die normalisierte Basis-URL und das HTTP-Timeout werden in
`/config/spoolman.json` gespeichert. Der SpoolmanTask fuehrt den
Verbindungstest im Network-Kontext aus und fragt nacheinander
`/api/v1/health` sowie `/api/v1/info` ab. Nur erfolgreiche, als JSON lesbare
Antworten setzen `EVENT_SPOOLMAN_READY`; die Versionsnummer aus `info` wird in
der GUI angezeigt.

## Spulen lesen und suchen

Der SpoolmanTask liest einzelne Spulen ueber `GET /api/v1/spool/{id}` und
sucht Spulen ueber `GET /api/v1/spool`. Die Suche unterstuetzt Filamentname,
Material und Hersteller; eine rein numerische Eingabe kann als direkte
Spulen-ID verwendet werden. Archivierte Spulen werden standardmaessig nicht
angezeigt.

Der `CMP_SPOOL_PICKER` fordert die Daten ausschliesslich ueber UiAction,
AppTask und SpoolmanTask an. Pro Suche werden hoechstens 20 kompakte
Treffer mit Spulen-ID, Hersteller, Material, Filamentname und Restgewicht an
den UiTask uebertragen. Es werden weder HTTP-Aufrufe im UiTask ausgefuehrt
noch grosse JSON-Dokumente durch FreeRTOS-Queues gesendet.

Der Picker verwendet eine bildschirmfuellende Ergebnisansicht und eine davon
getrennte Tastatureingabe. Der Filter wird ueber ein Dropdown gewaehlt. `OK`
auf der Tastatur startet die Suche; eine zusaetzliche Suchschaltflaeche gibt
es nicht. Nach dem Abschluss der Eingabe verschwindet die Tastatur, bevor die
scrollbare Trefferliste angezeigt wird. Sie kann direkt per Wischgeste oder
ueber zwei Scrollschaltflaechen bewegt werden. Ein weiteres Nachladen ist
vorerst nicht vorgesehen. Gueltige `color_hex`- beziehungsweise
`multi_color_hexes`-Werte faerben die gesamte Ergebnisschaltflaeche mit bis zu
drei Farbsegmenten. Die Beschriftung liegt zur sicheren Lesbarkeit auf einer
kontrastreichen halbtransparenten Flaeche. Fehlende oder ungueltige Farben
werden neutral grau dargestellt.

Aenderungsoperationen an Spulen gehoeren zu spaeteren Teilaufgaben und sind
noch nicht implementiert.

## Hersteller und Filamente

Der SpoolmanTask kapselt die Katalogoperationen `SearchVendors`,
`CreateVendor`, `SearchFilaments` und `CreateFilament`. Verwendet werden die
REST-Endpunkte `GET/POST /api/v1/vendor` und `GET/POST /api/v1/filament`.
Suchergebnisse sind auf 20 Eintraege begrenzt und werden als kleine,
wertbasierte Queue-Nachrichten gemeldet.

Vor jedem Anlegen wird serverseitig nach einem exakten Namen gesucht und das
Ergebnis lokal normalisiert verglichen. Hersteller werden ohne Beachtung von
Gross-/Kleinschreibung und aeusseren Leerzeichen verglichen. Der
Filament-Dublettenkey besteht aus Hersteller-ID, Name, Material und Farbcode.
Ein vorhandener Datensatz wird zurueckgemeldet und nicht erneut angelegt.

Die lokale Validierung folgt dem Spoolman-Vertrag: Namen und Material duerfen
nicht leer sein, eine Hersteller-ID muss vorhanden sein, Dichte und
Durchmesser muessen groesser als null sein, Gewichte und Temperaturen duerfen
nicht negativ sein und Farbcodes bestehen aus sechs oder acht
Hexadezimalzeichen. Die in Phase 7.3 geschaffenen Kommandos sind die Grundlage
fuer den TagDefinition-Import in Phase 7.4; eine neue Katalog-GUI ist nicht
Bestandteil dieser Phase.

## TagDefinition-Import

Der Import wird ausschliesslich im `SpoolmanTask` ausgefuehrt:

1. `TagDefinition` validieren und auf Vendor, Filament und Spule abbilden.
2. Vendor anhand des normalisierten Namens suchen und wiederverwenden oder anlegen.
3. Filament anhand Vendor, Name, Material und Farbe suchen und wiederverwenden oder anlegen.
4. Eine neue Spule per `POST /spool` anlegen und deren ID an den AppTask zurueckgeben.

Vorhandene Katalogeintraege werden als Treffer verwendet; die Ergebnisanzeige
weist auf wiederverwendete Datensaetze hin. Eine neue physische Spule wird
bewusst immer angelegt, da zwei identische Rollen keine Dubletten sind.

Unterstuetzt werden Bambu Lab, OpenPrintTag, OpenTag3D und Legacy. Hersteller,
Filamentname, Material, Farbe und Nenngewicht muessen in der normalisierten
Tagdefinition vorhanden sein. Fuer bekannte Materialklassen nutzt die
Importabbildung eine zentral getestete Dichtetabelle. Die unterstuetzten
V1-Tagprofile werden als 1,75-mm-Filament abgebildet. Unbekannte Materialien
oder fehlende Pflichtfelder werden mit einer klaren Fehlermeldung abgelehnt.

Die Spulenerzeugung entspricht dem Spoolman-Vertrag: `filament_id` ist
erforderlich; `initial_weight` und `spool_weight` werden aus der Tagdefinition
uebernommen.
