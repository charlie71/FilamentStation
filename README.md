# FilamentStation

Grundgeruest einer eigenstaendigen Filamentverwaltungsstation auf Basis eines
ESP32-S3 und Arduino/FreeRTOS. Phase 0 und 1 enthalten nur die RTOS- und
Kommunikationsinfrastruktur; Hardwaredienste sind noch nicht implementiert.

## Build

Voraussetzung ist eine aktuelle PlatformIO-Installation. Im Projektverzeichnis:

```text
pio run
```

Das konfigurierte Ziel ist `esp32-s3-devkitc-1`. Flash- und PSRAM-Nutzung sind
fuer ein ESP32-S3-Modul mit OPI-PSRAM vorbereitet. Die konkrete WT32-SC01-Plus-
Revision sowie alle GPIOs muessen vor der Hardwareintegration verifiziert werden.

