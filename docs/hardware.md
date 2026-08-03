# Hardware

## Controller-Modul

Verbaut ist ein WT32-S3-WROVER-N16R2 mit 16 MB Flash und 2 MB PSRAM.

Der Hardwaretest vom 2026-08-03 bestaetigte zwei CPU-Kerne und 2 MB nutzbares
PSRAM. Die native USB-CDC-Schnittstelle benoetigt nach `Serial.begin()` eine
Wartezeit von 2500 ms, damit Windows COM4 nach einem Reset fertig anmelden kann,
bevor die Startdiagnose gesendet wird.

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
