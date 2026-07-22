# FilamentStation

Grundgeruest einer eigenstaendigen Filamentverwaltungsstation auf Basis eines
ESP32-S3 und Arduino/FreeRTOS. Phase 0 und 1 enthalten nur die RTOS- und
Kommunikationsinfrastruktur; Hardwaredienste sind noch nicht implementiert.

## Build

Voraussetzung ist eine aktuelle PlatformIO-Installation. Im Projektverzeichnis:

```text
pio run
```

Das konfigurierte Environment ist `wt32-s3-wrover-n16r2`. Es verwendet das
generische ESP32-S3-DevKitC als Toolchain-Grundlage und ist fuer das verbaute
WT32-S3-WROVER-N16R2 mit 16 MB QIO-Flash und 2 MB QSPI-PSRAM konfiguriert.
