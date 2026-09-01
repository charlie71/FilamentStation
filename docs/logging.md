# Logging

FilamentStation verwendet für sämtliche eigenen Laufzeitmeldungen ausschließlich
die zentrale Logger-API in `src/services/Logger.h`. Direkte Ausgaben über
`Serial.print*`, `printf`, Arduino-`log_*` oder ESP-IDF-`ESP_LOG*` gehören nicht
in Anwendungscode.

## Kanonisches Format

Jeder Datensatz hat das Format:

```text
<LEVEL> [<COMPONENT>] <message> key=value
```

Beispiel:

```cpp
FS_LOGI(services::LogComponent::Nfc,
        "Tag detected tech=%s uid=%s", technology, uid);
```

## Level

Zulässige Level sind `E` (Error), `W` (Warn), `I` (Info), `D` (Debug) und `T`
(Trace), absteigend priorisiert (`services::LogLevel`, `Logger.h`). Der
maximale kompilierte Level wird global mit `FS_LOG_LEVEL` (1-5) festgelegt.

## Components

Die stabilen Komponenten (`services::LogComponent`, `Logger.h`) sind `APP`,
`RTOS`, `UI`, `DISPLAY`, `TOUCH`, `STORAGE`, `NET`, `SPOOLMAN`, `SCALE`,
`NFC`, `BAMBU`, `POWER` und `UPDATE`. Jede neue Komponente braucht einen
Eintrag im `switch` in `Logger::componentName()`/`componentText()`
(`LoggerFormat.cpp`); der `switch` besitzt bewusst kein `default:`-Label,
sodass ein vergessener Eintrag als Compilerwarnung (`-Wswitch`) auffaellt --
bei dem in diesem Projekt geltenden 0-Warnungen-Massstab also spaetestens
beim naechsten Build, nicht erst als "UNKNOWN" auf dem Geraet.

## Logger API

Anwendungscode ruft ausschliesslich die Makros `FS_LOGE`/`FS_LOGW`/
`FS_LOGI`/`FS_LOGD`/`FS_LOGT(component, format, ...)` auf (`Logger.h`). Jedes
Makro prueft `Logger::enabled(level, component)` vor der eigentlichen
Formatierung, sodass ein deaktiviertes Level keine `vsnprintf`-Kosten
verursacht. `Logger::log()` erzeugt daraus den festen Datensatz und legt ihn
in `logQueue` ab; `Logger::task()` ist der alleinige Konsument
(`LoggingTask`, siehe unten). `Logger::levelName()`/`componentName()`
liefern die kurzen Label-Strings fuer das Ausgabeformat.

Neben dem globalen `FS_LOG_LEVEL` erlaubt `Logger::componentMinimumLevel()`
eine zusaetzliche Pro-Komponenten-Obergrenze, um eine einzelne, besonders
gespraechige Komponente ohne globale Pegelabsenkung stummzuschalten --
aktuell nutzt das ausschliesslich `LogComponent::Ui`, deren Debug-Zeilen pro
UI-Kommando andernfalls Bambu-/Netzwerkdiagnosen auf demselben Monitor
verdraengen wuerden (auf `LogLevel::Error` begrenzt). Das ist ein reiner
Compile-Time-Schalter direkt im `switch` von `componentMinimumLevel()`, kein
Laufzeit-/Einstellungs-Wert.

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

## Debug/Release Level

Es existiert aktuell nur eine einzige Geraete-Build-Umgebung
(`[env:wt32-s3-wrover-n16r2]`, `platformio.ini`) -- keine separate
Debug-/Release-Umgebung mit unterschiedlichem `FS_LOG_LEVEL`. Ohne
`-D FS_LOG_LEVEL=...` in `build_flags` bleibt es beim Header-Default aus
`Logger.h` (`4`, also `D`/Debug). `-D CORE_DEBUG_LEVEL=0` in denselben
`build_flags` betrifft ausschliesslich das ESP-IDF-/Arduino-interne Logging
(`ESP_LOG*`, ausserhalb der Anwendungssteuerung) und ist von `FS_LOG_LEVEL`
unabhaengig -- es unterdrueckt nicht die eigenen `FS_LOG*`-Aufrufe. Ein
kuenftiger separater Release-Build (z. B. `T`/Trace generell aus, siehe
Kommentar bei `FS_LOG_LEVEL`) muesste als eigene `[env:...]`-Sektion mit
eigenem `-D FS_LOG_LEVEL=` angelegt werden.

## WiFiManager-Debug

`WiFiManager::setDebugOutput(false)` deaktiviert die eingebaute serielle
Debugausgabe der Bibliothek, damit sie nicht am zentralen Logger-Format
vorbei direkt auf `Serial` schreibt und Datensaetze verschachteln kann.

## Sensible Daten

Nicht protokolliert werden WLAN-Passwörter, Bambu-Zugangscodes, Tokens,
Schlüssel oder andere Secrets. NFC-UIDs dürfen auf `DEBUG` vollständig
erscheinen und auf `INFO` nur, wenn sie für einen Zuordnungsablauf relevant
sind.

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

Sie prüfen alle Level sowie `key=value`, eingebettete und lange Zeilen,
Trunkierung sowie genau ein Zeilenende anhand einer Auswahl der
Komponenten (`APP`, `RTOS`, `UI`, `DISPLAY`, `TOUCH`, `STORAGE`, `NET`,
`SPOOLMAN`, `SCALE`, `NFC`, `BAMBU`) -- `POWER` und `UPDATE` sind aktuell
nicht in dieser Auswahl vertreten (bekannte Testluecke, unkritisch: beide
nutzen exakt denselben generischen Formatierungscode wie alle anderen
Komponenten, `componentText()` ist bereits als vollstaendiger `switch` ohne
`default` gegen einen fehlenden Eintrag abgesichert, siehe Abschnitt
"Components"). Der Hardwaretest prüft zusätzlich
gleichzeitige Meldungen mehrerer Tasks, Monitor-Zeitstempel, Logdatei und die
unveränderte Ausgabe eines echten ESP32-Crash-Backtrace. Für letzteren wird kein
Absturz absichtlich ausgelöst; ein vorhandener Crash kann mit dem erzeugenden
ELF geprüft werden.
