# Hardware

## Controller-Modul

Verbaut ist ein WT32-S3-WROVER-N16R2 mit 16 MB Flash und 2 MB PSRAM.

Der Hardwaretest vom 2026-08-03 bestaetigte zwei CPU-Kerne und 2 MB nutzbares
PSRAM. Die native USB-CDC-Schnittstelle meldet COM4 nach einem Reset neu bei
Windows an. Der Arduino-Setup-Task wartet deshalb nach `Serial.begin()` fest
fuenf Sekunden, bevor er die Startdiagnose ausgibt.

## SD-Karte

Die SD-Karte ist ueber SPI angebunden:

| Signal | GPIO |
|---|---:|
| CS | 41 |
| DI / MOSI | 40 |
| CLK / SCK | 39 |
| DO / MISO | 38 |

Ein Card-Detect-Signal ist nicht verfuegbar. Deshalb prueft ausschliesslich der
StorageTask die Erreichbarkeit der Karte in einem langsamen Intervall von zwei
Sekunden. Nach einer erkannten Entfernung wird `EVENT_SD_READY` geloescht und
der Fehler bis zum Neustart verriegelt. Eine wieder eingesetzte Karte wird
gemeldet, aber im laufenden Betrieb nicht wieder freigegeben.

Der Mount der SD-Karte ueber diese Pinbelegung wurde am 2026-08-03 auf der
Zielhardware erfolgreich protokolliert. Am selben Tag wurde die Karte im
laufenden Betrieb entfernt und wieder eingesetzt. Der StorageTask erkannte
beide Zustandswechsel, loeschte beim Entfernen `EVENT_SD_READY` und behielt den
geforderten Neustartzustand auch nach dem Wiedereinsetzen bei.

Da kein Card-Detect-Signal existiert, erzeugen die Zugriffsproben waehrend die
Karte fehlt erwartungsgemaess Fehlermeldungen des Arduino-ESP32-SD-Treibers.

## Display und Touch

Die Hardware ist das WT32-SC01 Plus mit dem Displaymodul
ZX3D50CE08S-USRC-4832. Das Herstellerdatenblatt V1.5 nennt einen ST7796UI mit
480 x 320 Pixeln, RGB565 und paralleler 8-Bit-MCU8080-Schnittstelle. Der
kapazitive Single-Touch-Controller ist ein FT6336U am I2C-Bus.

Quellen:

* [WT32-SC01 Plus Datasheet V1.5](https://docs.makehub.tw/wt32-sc01plus/WT32-SC01%2BPLUS%2BDatasheet-V1.5%2BEN.pdf)
* [RIOT-OS Boarddefinition WT32-SC01 Plus](https://doxygen.riot-os.org/group__boards__esp32s3__wt32__sc01__plus.html)

### Interne Pinbelegung

| Funktion | Signal | GPIO | Hinweis |
|---|---|---:|---|
| Backlight | BL_PWM | 45 | aktiv high, PWM-faehig |
| Display/Touch | RESET | 4 | gemeinsam genutzt |
| Display | RS / D-C | 0 | zugleich BOOT-Strapping-Pin |
| Display | WR | 47 | Schreibimpuls |
| Display | TE | 48 | Frame-Sync, optional nutzbar |
| Display | DB0 | 9 | MCU8080 Datenbit 0 |
| Display | DB1 | 46 | MCU8080 Datenbit 1 |
| Display | DB2 | 3 | MCU8080 Datenbit 2 |
| Display | DB3 | 8 | MCU8080 Datenbit 3 |
| Display | DB4 | 18 | MCU8080 Datenbit 4 |
| Display | DB5 | 17 | MCU8080 Datenbit 5 |
| Display | DB6 | 16 | MCU8080 Datenbit 6 |
| Display | DB7 | 15 | MCU8080 Datenbit 7 |
| Touch | INT | 7 | Interrupt, spaetere Nutzung zu pruefen |
| Touch | SDA | 6 | I2C-Daten |
| Touch | SCL | 5 | I2C-Takt |

### GPIO-Konfliktpruefung

Display/Touch und SD verwenden keine gemeinsamen Daten- oder Buspins. GPIO4
ist hardwareseitig absichtlich der gemeinsame Reset von ST7796UI und FT6336U;
beide Treiber muessen diesen Reset koordiniert behandeln. GPIO0 ist zugleich
LCD-RS und BOOT-Strapping-Pin und darf beim Start nicht extern in einen
ungueltigen Pegel gezwungen werden.

Die herausgefuehrten EXT-Pins GPIO10, GPIO11, GPIO12, GPIO13, GPIO14 und GPIO21
kollidieren nicht mit Display, Touch oder SD. Audio belegt GPIO35 bis GPIO37,
RS485 GPIO1, GPIO2 und GPIO42. Diese Pins gelten deshalb nicht als frei fuer
HX711 oder PN532, solange die jeweilige Onboard-Funktion verwendet werden soll.
Konkrete externe GPIO-Zuweisungen werden erst nach Pruefung der angeschlossenen
Module festgelegt.

## HX711-Anschluss

Fuer den HX711 werden zwei dokumentierte EXT-Leitungen verwendet:

| HX711-Signal | Boardanschluss | GPIO | Richtung |
|---|---|---:|---|
| DOUT | EXT_IO2 | 11 | Eingang, fallende Flanke |
| SCK | EXT_IO1 | 10 | Ausgang, Startpegel Low |

GPIO10 und GPIO11 sind beim WT32-SC01-Plus als 0-3,3-V-EXT-I/O
herausgefuehrt. Der ESP32-S3 beschreibt beide als normale Ein-/Ausgaenge;
GPIO11 kann ueber den GPIO-ISR-Service eine fallende Flanke ausloesen. Die
Pins kollidieren nicht mit Display, Touch, SD, USB, Audio oder RS485.

Quellen:

* [WT32-SC01 Plus Datasheet V1.5](https://docs.makehub.tw/wt32-sc01plus/WT32-SC01%2BPLUS%2BDatasheet-V1.5%2BEN.pdf)
* [ESP32-S3 GPIO API](https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32s3/api-reference/peripherals/gpio.html)
* [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

Der GPIO-Interrupt ist mit einem `IRAM_ATTR`-Handler registriert. Die ISR
sendet ausschliesslich eine FreeRTOS-Task-Notification an den ScaleTask und
fordert bei Bedarf einen Kontextwechsel an. Sie liest keine HX711-Daten und
fuehrt keine Taktimpulse aus. Ein Hardwaretest mit angeschlossenem HX711 steht
noch aus.

Der ScaleTask liest nach der Notification den vorzeichenbehafteten 24-Bit-
Rohwert und sendet mit dem 25. Taktimpuls die Auswahl fuer Kanal A/Gain 128.
Bleibt DOUT 1,5 Sekunden ohne fallende Flanke, wird `EVENT_SCALE_READY`
geloescht und einmalig ein `ScaleError` an den AppTask gemeldet. Nach dem
naechsten gueltigen Rohwert wird der Ready-Zustand wieder gesetzt. Die
Umrechnung in Gramm, Filteralgorithmen und Stabilitaetserkennung sind noch
nicht Bestandteil dieser Phase.

## PN532-Anschluss

Der PN532 wird ueber seine HSU/UART-Schnittstelle mit 115200 Baud und 8N1
angebunden. Die Verbindung verwendet zwei weitere dokumentierte EXT-Leitungen:

| Richtung | Boardanschluss | GPIO | PN532-Signal |
|---|---|---:|---|
| ESP32 -> PN532 | EXT_IO3 | 12 | RX |
| PN532 -> ESP32 | EXT_IO4 | 13 | TX |

GPIO12 und GPIO13 sind normale Ein-/Ausgaenge des ESP32-S3. Sie sind am
WT32-SC01-Plus herausgefuehrt und kollidieren nicht mit Display, Touch, SD,
USB oder den HX711-Leitungen GPIO10/GPIO11.

Ein zusaetzlicher externer PN532-IRQ wird im HSU-Betrieb nicht verwendet. Der
P70/IRQ-Pin des PN532 ist waehrend des Resets zugleich an der Modusauswahl
beteiligt und wird von Breakout-Modulen nicht einheitlich herausgefuehrt. Im
UART-Betrieb signalisiert der PN532 seine Antwort ohnehin ueber RX-Daten. Der
ESP32-UART-Treiber empfaengt diese interruptgesteuert und stellt dem NfcTask
ein Queue-Ereignis bereit. Die Anwendungssoftware besitzt deshalb keine eigene
NFC-ISR und fuehrt insbesondere keine PN532-Protokollkommunikation in einer ISR
aus.

Der NfcTask blockiert gemeinsam auf seiner Command-Queue und der UART-
Event-Queue. Phase 5.1 initialisiert nur den Transport; der PN532 wird noch
nicht abgefragt und NFC-Daten werden erst in Phase 5.3 verarbeitet.

Quellen:

* [PN532 User Manual UM0701-02](https://www.nxp.com/docs/en/user-guide/141520.pdf)
* [ESP32-S3 GPIO API](https://docs.espressif.com/projects/esp-idf/en/release-v5.0/esp32s3/api-reference/peripherals/gpio.html)
* [ESP32-S3 UART API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/uart.html)

### Hintergrundbeleuchtung

Das Datenblatt bestaetigt GPIO45 als aktiv-hohen PWM-Eingang. Bei maximaler
Helligkeit nennt es fuer das Gesamtgeraet typisch 175 mA bei 5 V. Ein visueller
PWM-/Helligkeitstest auf der konkreten Hardware steht noch aus und wird nicht
als bestanden gewertet.

## LovyanGFX-Grundtreiber

LovyanGFX ist in Version 1.2.25 festgelegt. `DisplayDriver` konfiguriert den
ST7796UI mit 20 MHz Schreibfrequenz als MCU8080-8-Bit-Panel. Die physische
Panelgeometrie ist 320 x 480; Rotation 3 erzeugt die um 180 Grad gedrehte
logische Anwendungsgeometrie 480 x 320. Das Backlight verwendet GPIO45 mit einem
44,1-kHz-PWM-Signal und startet bei 192 von 255.

Der FT6336U wird ueber LovyanGFXs `Touch_FT5x06` an I2C-Port 1, Adresse `0x38`,
GPIO6/GPIO5 und 400 kHz eingerichtet. LovyanGFX schaltet GPIO4 nur ueber den
Panel-Reset, da derselbe physische Reset zugleich zum Touchcontroller fuehrt.

Nur der UiTask greift auf die LovyanGFX-Instanz zu. Bis LVGL integriert ist,
blockiert er jeweils bis zu 50 ms auf der `uiCommandQueue` und prueft danach den
Touchzustand. Beruehrungen werden als gelbe Punkte gezeichnet und nur bei
Beruehrungsbeginn oder einer Koordinatenaenderung von mindestens acht Pixeln
protokolliert.

Der Hardwaretest vom 2026-08-04 bestaetigte die Farbreihenfolge Rot, Gruen,
Blau, Weiss und Schwarz, die um 180 Grad gedrehte Landschaftsausrichtung sowie
passend rotierte Touchkoordinaten. Beruehrungen wurden an der erwarteten Stelle
als gelbe Punkte dargestellt. Die geometrisch exakten Eckwerte `x=0, y=0` und
`x=479, y=319` waren wegen der nicht bis zum aeussersten Panelrand erreichbaren
kapazitiven Randzone nicht antippbar; Achsen, Richtung und nutzbare Flaeche
waren korrekt.

## LVGL-Hardwareanbindung

LVGL 9.5.0 rendert in RGB565 mit zwei partiellen 40-Zeilen-Puffern im PSRAM.
Der Hardwarestart vom 2026-08-04 bestaetigte die erfolgreiche direkte
PSRAM-Zeigerpruefung, fortgesetzte UiTask-Ausfuehrung und fehlerfreie
Queue-Kommunikation nach der LVGL-Initialisierung. Der Testscreen enthaelt eine
Titelzeile, einen mittigen `Touch test`-Button und eine Statuszeile. Die
abschliessende visuelle Beurteilung von Farben und Button-Reaktion erfolgt am
Panel; Initialisierung und Callback-Pipeline erzeugten keinen Fatal-Error oder
Neustart.
Eine versuchsweise Reduktion des 8-Bit-Displaybusses von 20 auf 10 MHz brachte
keine sichtbare Verbesserung der kleinen LVGL-Testschrift und wurde deshalb
zurueckgenommen. Nach Korrektur der Byte-Reihenfolge verwendet der Testscreen
Montserrat mit 24 Pixeln fuer den Titel, 20 Pixeln fuer den Button und 18
Pixeln fuer Status und Farbfeldbeschriftungen.

Schwarze beziehungsweise weisse Konturen an entgegengesetzt gefaerbter Schrift
zeigten anschliessend, dass insbesondere die gemischten RGB565-Farbwerte der
Kantenglaettung betroffen waren. Der LVGL-Puffer wird deshalb im
LovyanGFX-Flush-Callback als natives `rgb565_t` statt als `swap565_t`
interpretiert. Schwarze und weisse Vollpixel allein konnten diesen
Byte-Reihenfolgefehler nicht sichtbar machen.

Zur visuellen Kontrolle zeigt der LVGL-Testscreen sechs RGB-Testfelder fuer
Rot, Gruen, Blau, Cyan, Weiss und Schwarz. Kontrastierende Buchstaben in den
Feldern machen zugleich fehlerhafte Mischfarben an geglaetteten Schriftkanten
sichtbar.
