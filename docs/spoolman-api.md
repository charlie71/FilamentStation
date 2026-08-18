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
