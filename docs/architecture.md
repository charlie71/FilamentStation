# Architektur

## LVGL-Eigentuemerschaft

LVGL 9.5.0 gehoert ausschliesslich dem `UiTask`. Dieser Task initialisiert
LovyanGFX, ruft `lv_init()` auf, erzeugt Display und Eingabegeraet und fuehrt
`lv_timer_handler()` aus. Die Flush- und Touch-Callbacks laufen synchron im
Kontext desselben Tasks. Andere Tasks erhalten weder LVGL-Objektzeiger noch
rufen sie LVGL-Funktionen auf; spaetere UI-Aenderungen werden ausschliesslich
als Werte ueber die `uiCommandQueue` transportiert.

`UiBridge` kapselt die LVGL-Hardwareanbindung. Der Display-Callback uebergibt
partielle RGB565-Flaechen synchron an LovyanGFX und bestaetigt danach mit
`lv_display_flush_ready()`. Der Pointer-Callback liest den bereits durch
LovyanGFX auf 480 x 320 rotierten FT6336U-Zustand.

Der UiTask wartet mit dem von LVGL berechneten naechsten Timerzeitpunkt auf der
`uiCommandQueue`. Eingehende Kommandos werden ausschliesslich durch
`UiBridge::processUiCommand()` in LVGL-Aenderungen uebersetzt. Dadurch erhalten
andere Tasks weder LVGL-Zeiger noch eine direkte UI-Schnittstelle.

## Renderpuffer

LVGL verwendet zwei partielle Puffer mit jeweils 480 x 40 RGB565-Pixeln. Das
entspricht 38.400 Byte je Puffer und 76.800 Byte insgesamt. Beide Puffer werden
explizit mit `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` reserviert. Beim Start wird
mit `esp_ptr_external_ram()` geprueft, dass beide Adressen tatsaechlich im PSRAM
liegen. Eine fehlgeschlagene Allokation oder Pruefung setzt den fatalen
Systemzustand und startet keine scheinbar funktionsfaehige UI.

## Spoolman-Anwendungszustand

Der AppTask bildet die Event-Group-Bits auf genau drei fachliche Zustände ab:

- `SpoolmanUnavailable`: Server nicht verfügbar; alle abhängigen Online-
  Aktionen sind gesperrt.
- `SpoolmanReady`: Server und Textfeld `extra.tag` sind bereit; einschließlich
  NFC-Zuordnung sind alle zulässigen Spoolman-Aktionen verfügbar.
- `TagFieldUnavailable`: Server ist erreichbar, aber `extra.tag` fehlt oder
  besitzt einen inkompatiblen Typ; normale Online-Aktionen bleiben möglich,
  Tag-Zuordnungen sind gesperrt.

Der Zustand wird wertbasiert über die `uiCommandQueue` an den UiTask gesendet.
Nur der UiTask ändert daraus LVGL-Statusanzeigen und Buttonzustände. WLAN-
Verlust verwirft die beiden Spoolman-Eventbits unmittelbar. Die Einstellungs-
Screens bleiben in allen drei Zuständen erreichbar.

## Tasks

Jeder Task ist ein eigenstaendiger fachlicher Zustaendigkeitsbereich (eine
Task pro Peripherie/Dienst), kommuniziert ausschliesslich ueber wertbasierte
Queue-Nachrichten und blockiert dabei jeweils auf genau einer Queue (siehe
`docs/rtos.md` fuer die Wartestrategie je Task). Kein Core-Pinning
(`kNoCoreAffinity` fuer alle Tasks); Groessenordnungen fuer Stack/Prioritaet
stehen mit ihrer jeweiligen Begruendung als Kommentar direkt bei den
Konstanten in `src/config/TaskConfig.h`.

| Task | Zweck | Prio | Stack (Byte) |
|---|---|---:|---:|
| `AppTask` | Zentraler Event-Router: empfaengt alle `AppEvent`s, haelt UI-/Domain-Zustand, sendet `UiCommand`s und Befehle an die Service-Tasks | 3 | 12288 |
| `UiTask` | Alleiniger LVGL-/LovyanGFX-Eigentuemer, Display-Flush, Touch-Callback | 2 | 16384 |
| `ScaleTask` | HX711-Rohwerte, Filterung, Tara/Kalibrierung | 2 | 8192 |
| `NfcTask` | PN532-Polling, Tag-Lesen/-Schreiben, Bambu-/OpenTag-Erkennung | 2 | 16384 |
| `StorageTask` | SD-Karte: JSON laden/speichern/loeschen, Erreichbarkeitspruefung | 2 | 8192 |
| `NetworkTask` | WiFiManager, Captive Portal, Verbindungsstatus | 1 | 8192 |
| `SpoolmanTask` | Spoolman-REST-API (HTTPS/JSON) | 1 | 10240 |
| `BambuTask` | Bambu-MQTT (bis zu vier TLS-Verbindungen gleichzeitig) | 1 | 8192 |
| `PowerTask` | Energiesparen-Statemachine (Aktiv/Gedimmt/Sleep) | 1 | 4096 |
| `UpdateTask` | Firmware-Update: GitHub-Releases-Abfrage, Download, Flash | 1 | 8192 |
| `LoggingTask` | Einziger Zugriff auf USB-CDC `Serial`, serialisiert alle Logzeilen | 1 | 2048 |

### Prioritäten

Nur zwei Stufen sind in Benutzung: `AppTask` steht als zentraler
Event-Router allein auf Prioritaet 3, da er niemals durch einen der
Service-Tasks verzoegert werden darf -- jede Verzoegerung dort verzoegert
sofort auch alle anderen Tasks, die auf eine Antwort/ein UI-Update warten.
Prioritaet 2 tragen die Tasks mit direkter Wirkung auf wahrnehmbare
Reaktionszeit oder Zeitkritikalitaet der zugrunde liegenden Hardware
(Display/Touch, Waagen-Abtastung, NFC-Polling, SD-Zugriff). Prioritaet 1
tragen alle Tasks, die ganz ueberwiegend blockierend auf Netzwerk-I/O oder
eine eigene Queue warten (WLAN, Spoolman, Bambu, Firmware-Update,
Energiesparen, Logging) -- sie muessen die zeitkritischeren Tasks nicht
verdraengen koennen, duerfen aber auch nicht verhungern, da FreeRTOS auf dem
ESP32 preemptiv zwischen gleich priorisierten Tasks per Time-Slicing
wechselt.

## Queues

Alle Queues transportieren ausschliesslich trivial kopierbare Werttypen
(kein `String`, kein Zeiger auf Task-lokalen Stack) -- durchgesetzt per
`static_assert(std::is_trivially_copyable_v<...>)` in `rtos/Messages.h`.

| Queue | Laenge | Nachrichtentyp | Richtung |
|---|---:|---|---|
| `appEventQueue` | 16 | `AppEvent` | alle Service-Tasks → `AppTask` |
| `uiCommandQueue` | 8 | `UiCommand` | `AppTask` → `UiTask` |
| `scaleCommandQueue` | 8 | `ScaleCommand` | `AppTask` → `ScaleTask` |
| `nfcCommandQueue` | 8 | `NfcCommand` | `AppTask` → `NfcTask` |
| `storageCommandQueue` | 8 | `StorageCommand` | `AppTask` → `StorageTask` |
| `networkCommandQueue` | 8 | `NetworkCommand` | `AppTask` → `NetworkTask` |
| `wifiEventQueue` (+ `networkQueueSet`) | 8 | `std::uint8_t` (WiFiManager-Ereignis) | intern `NetworkTask`, per Queue-Set gemeinsam mit `networkCommandQueue` abgewartet |
| `spoolmanCommandQueue` | 8 | `SpoolmanCommand` | `AppTask` → `SpoolmanTask` |
| `bambuCommandQueue` | 8 | `BambuCommand` | `AppTask` → `BambuTask` |
| `powerCommandQueue` | 8 | `PowerCommand` | `UiTask`/`ScaleTask`/`NfcTask`/`NetworkTask` → `PowerTask` |
| `updateCommandQueue` | 8 | `UpdateCommand` | `AppTask` → `UpdateTask` |
| `logQueue` | 32 | `LogMessage` (192 Byte fest) | alle Tasks → `LoggingTask` |

Alle Service-Command-Queues (Laenge 8) verwenden dieselbe Konstante
`kServiceCommandQueueLength`; `storageCommandQueue` und `logQueue` haben
wegen abweichender Lastprofile eigene Konstanten (`src/config/
TaskConfig.h`). Antworten laufen fuer alle Service-Tasks einheitlich als
`AppEvent` ueber die `appEventQueue` zurueck zum `AppTask` und tragen
dieselbe `requestId` wie der ausloesende Befehl, sodass der `AppTask`
Anfrage und Antwort zuordnen kann, ohne selbst blockieren zu muessen.

## Events

`rtos::AppEventType` (`rtos/Events.h`) ist der einzige Nachrichtentyp auf der
`appEventQueue` und deckt alle fachlichen Ereignisse aller Service-Tasks ab.
Jede Variante nutzt dieselben drei generischen Felder (`requestId`, `value`,
`text`) mit variantenspezifischer Bedeutung -- die genaue Belegung steht
direkt als Kommentar bei den neueren, weniger selbsterklaerenden Varianten
im Header (z. B. `UpdateCheckResult`, `BambuAssignProgress`). Fachliche
Gruppen:

| Gruppe | Beispiele |
|---|---|
| UI/Boot | `UiAction`, `UiCommunicationTest` |
| Waage | `ScaleReady`, `ScaleMeasurement`, `ScaleStable`, `ScaleUnstable`, `ScaleTared`, `ScaleCalibrated`, `ScaleCalibrationReset`, `ScaleError` |
| NFC | `NfcInitialized`, `NfcTagDetected`, `NfcTagRemoved`, `NfcTagRead`, `NfcTagWritten`, `NfcTagErased`, `NfcError` |
| SD/Storage | `SdMounted`, `SdRemoved`, `SdReinserted`, `SdError`, `StorageReadCompleted`, `StorageWriteCompleted`, `StorageRequestError` |
| WLAN | `WifiStationConnected`, `WifiGotIp`, `WifiDisconnected`, `WifiLostIp`, `WifiConfigPortalStarted`, `WifiConfigPortalStopped`, `WifiConfigPortalTimedOut`, `WifiCredentialsCleared` |
| Spoolman | `SpoolmanConnected`, `SpoolmanTagFieldReady`, `SpoolmanTagLookup`, `SpoolmanTagDuplicate`, `SpoolmanTagUpdated`, `SpoolmanResponse`, `SpoolmanVendorResult`, `SpoolmanFilamentResult`, `SpoolmanCatalogCreated`, `SpoolmanCatalogDuplicate`, `SpoolmanImportCompleted`, `SpoolmanWeightUpdated`, `SpoolmanError` |
| Bambu | `BambuConnected`, `BambuDisconnected`, `BambuUpdate`, `BambuTestResult`, `BambuError`, `BambuAssignProgress` |
| Firmware-Update | `UpdateCheckResult`, `UpdateDownloadProgress`, `UpdateDownloadResult` |

Getrennt davon signalisiert das globale `systemEventGroup` (FreeRTOS Event
Group) reine Bereitschafts-/Fehlerzustaende als Bits, die mehrere Tasks ohne
eigene Anfrage abfragen koennen (`EVENT_UI_READY`, `EVENT_SD_READY`,
`EVENT_SCALE_READY`, `EVENT_NFC_READY`, `EVENT_WIFI_CONNECTED`,
`EVENT_SPOOLMAN_READY`, `EVENT_BAMBU_READY`, `EVENT_SPOOLMAN_TAG_FIELD_READY`,
`EVENT_FATAL_ERROR`) -- z. B. wartet `AppTask::showHomeWhenStartupReady()`
blockierend auf `EVENT_UI_READY | EVENT_SD_READY`, ohne dass UiTask/
StorageTask dafuer eine eigene Nachricht senden muessten.

## IRQ

Es existiert genau eine anwendungseigene Interrupt-Service-Routine: der
HX711-Datenbereitschafts-Interrupt (`ScaleTask.cpp`,
`hx711DataReadyIsr()`), registriert per `gpio_isr_handler_add()` auf GPIO11
(fallende Flanke, `GPIO_INTR_NEGEDGE`) mit `gpio_install_isr_service
(ESP_INTR_FLAG_IRAM)`. Die ISR liest keine HX711-Daten und fuehrt keine
Taktimpulse aus -- sie ruft ausschliesslich `vTaskNotifyGiveFromISR()` auf
den `ScaleTask` und fordert bei Bedarf per `portYIELD_FROM_ISR()` einen
Kontextwechsel an. Der eigentliche 24-Bit-Rohwertsread samt Taktimpulsen
laeuft danach im Taskkontext von `ScaleTask`.

Alle anderen Hardwareschnittstellen verzichten bewusst auf eine eigene ISR:

* **Touch (FT6336U):** physischer INT-Pin (GPIO7) vorhanden, aber nicht als
  Interrupt genutzt -- LovyanGFX/LVGL fragt den Touchzustand per I2C aus dem
  `UiTask` heraus ab (Begruendung: `UiTask` muss gleichzeitig auf der
  `uiCommandQueue` und auf LVGL-Timern warten, was sich mit einer separaten
  Task-Notification nicht gemeinsam blockierend abwarten laesst, siehe
  `docs/rtos.md`).
* **PN532 (UART/HSU):** keine eigene ISR; der ESP32-Arduino-UART-Treiber
  empfaengt Antwortbytes bereits interruptgesteuert und weckt den darin
  blockierenden `NfcTask` transparent.
* **SD-Karte:** kein Card-Detect-Signal vorhanden, daher kein Interrupt
  moeglich -- stattdessen ein 2-Sekunden-Polling-Fallback im `StorageTask`
  (siehe `docs/hardware.md`).
