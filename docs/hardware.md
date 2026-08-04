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

### Hintergrundbeleuchtung

Das Datenblatt bestaetigt GPIO45 als aktiv-hohen PWM-Eingang. Bei maximaler
Helligkeit nennt es fuer das Gesamtgeraet typisch 175 mA bei 5 V. Ein visueller
PWM-/Helligkeitstest auf der konkreten Hardware steht noch aus und wird nicht
als bestanden gewertet.
