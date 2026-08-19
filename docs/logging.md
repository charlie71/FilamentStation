# Logging

FilamentStation verwendet für sämtliche eigenen Laufzeitmeldungen ausschließlich
die zentrale Logger-API in `src/services/Logger.h`. Direkte Ausgaben über
`Serial.print*`, `printf`, Arduino-`log_*` oder ESP-IDF-`ESP_LOG*` gehören nicht
in Anwendungscode.

## Format und Komponenten

Jeder Datensatz hat das Format:

```text
<LEVEL> [<COMPONENT>] <message> key=value
```

Zulässige Level sind `E`, `W`, `I`, `D` und `T`. Die stabilen Komponenten sind
`APP`, `RTOS`, `UI`, `DISPLAY`, `TOUCH`, `STORAGE`, `NET`, `SPOOLMAN`, `SCALE`,
`NFC` und `BAMBU`.

Beispiel:

```cpp
FS_LOGI(services::LogComponent::Nfc,
        "Tag detected tech=%s uid=%s", technology, uid);
```

Der maximale kompilierte Level wird mit `FS_LOG_LEVEL` von 1 (`E`) bis 5 (`T`)
festgelegt. Der Entwicklungs-Build verwendet standardmäßig `D`. `T` wird nur
gezielt aktiviert.

## Atomare Ausgabe

Ein Logger-Aufruf erzeugt einen vollständigen, festen Datensatz. Eingebettete
Zeilenumbrüche und Tabs werden durch Leerzeichen ersetzt; jeder Datensatz endet
auf dem Gerät genau einmal mit `LF`. Der PlatformIO-`default`-Filter bildet das
auf dem Windows-Host korrekt auf ein einzelnes `CRLF` ab. Nach dem RTOS-Start legt jeder Produzent seinen
Datensatz als Wert in `logQueue`. Nur `LoggingTask` schreibt seriell. Dadurch
können Ausgaben paralleler Tasks nicht ineinander verschachtelt werden.

Vor dem Start von `LoggingTask` darf der Logger Startdiagnosen direkt ausgeben.
Das ist sicher, weil zu diesem Zeitpunkt noch keine konkurrierenden
Anwendungs-Tasks loggen. Logging aus Interrupts wird verworfen; eine ISR muss
stattdessen eine Task benachrichtigen.

## Sensible Daten

Nicht protokolliert werden WLAN-Passwörter, Bambu-Zugangscodes, Tokens,
Schlüssel oder andere Secrets. NFC-UIDs dürfen auf `DEBUG` vollständig
erscheinen und auf `INFO` nur, wenn sie für einen Zuordnungsablauf relevant
sind. WiFiManager-Debug ist über `setDebugOutput(false)` deaktiviert.

## PlatformIO-Monitor

Die Geräteumgebung verwendet 115200 Baud und folgende Filter:

```ini
monitor_filters =
    default
    esp32_exception_decoder
    time
    log2file
```

Start:

```text
pio device monitor -e wt32-s3-wrover-n16r2
```

`time` ergänzt den Zeitstempel auf dem Host. `log2file` schreibt denselben
Monitorstrom in eine Datei. Der Logger verändert Bootloader-, Panic- und
Backtrace-Ausgaben nicht; deshalb kann `esp32_exception_decoder` diese weiterhin
auswerten.

## Prüfung

Die nativen Format- und Filtertests laufen mit:

```text
pio test -e native-logger-tests
```

Sie prüfen alle Level und Komponenten, `key=value`, eingebettete und lange
Zeilen, Trunkierung sowie genau ein Zeilenende. Der Hardwaretest prüft zusätzlich
gleichzeitige Meldungen mehrerer Tasks, Monitor-Zeitstempel, Logdatei und die
unveränderte Ausgabe eines echten ESP32-Crash-Backtrace. Für letzteren wird kein
Absturz absichtlich ausgelöst; ein vorhandener Crash kann mit dem erzeugenden
ELF geprüft werden.
