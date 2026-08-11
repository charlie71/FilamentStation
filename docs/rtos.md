# FreeRTOS-Grundarchitektur

Alle Service-Tasks blockieren auf ihrer Command-Queue. AppTask blockiert auf der
Event-Queue, UiTask auf der UI-Command-Queue. Es wird noch kein Core-Pinning
verwendet. Taskparameter und Queue-Laengen stehen in `src/config/TaskConfig.h`.

Die `storageCommandQueue` besitzt acht FIFO-Plaetze. Jede
`StorageCommand`-Nachricht ist ein trivial kopierbarer Werttyp mit `requestId`,
Dokumenttyp, einem auf 96 Byte begrenzten Pfad und einem auf 768 Byte begrenzten
JSON-Puffer. Dadurch befinden sich weder lokale Stackzeiger noch dynamische
Arduino-`String`-Objekte in der Queue. Der feste Puffer ist nur fuer kleine
Konfigurationsnachrichten vorgesehen; groessere Dokumente benoetigen spaeter
typisierte Datenmodelle oder kontrollierte externe Puffer.

Der `StorageTask` nimmt immer genau den aeltesten Queue-Eintrag entgegen und
bearbeitet ihn vollstaendig, bevor er den naechsten empfaengt. Antworten tragen
dieselbe `requestId` und laufen ausschliesslich ueber die `appEventQueue`.

Der StorageTask verwendet 8192 Byte Stack. Der Hardware-Backtrace vom
2026-08-04 zeigte mit 4096 Byte einen Stack-Canary-Fehler in der tiefen
FATFS-/SPI-Aufrufkette waehrend der Verzeichnispruefung. Der etwa 0,9 KiB grosse
Queue-Empfangspuffer liegt deshalb statisch im task-exklusiven Speicher und
nicht zusaetzlich auf dem Taskstack.

Der StorageTask verwendet mangels Card-Detect einen dokumentierten
Zwei-Sekunden-Timeout beim Warten auf seiner Queue. Nur nach diesem Timeout
erfolgt eine kurze SD-Erreichbarkeitsprobe. Das ist der langsamste praktikable
Fallback zur Erkennung einer Kartenentfernung ohne schnelle Polling-Schleife.

Der UiTask verwendet fuer LovyanGFX, LVGL und die Display-/Touch-Treiber 8192
Byte Stack. Display- und LVGL-Zugriffe erfolgen ausschliesslich in diesem Task.
Seit Phase 3.4 verwendet der UiTask den von `lv_timer_handler()` gemeldeten
naechsten Ausfuehrungszeitpunkt direkt als Timeout der `uiCommandQueue`. Nur
ein von LVGL angeforderter Nullabstand wird auf eine Millisekunde angehoben,
damit auch bei unmittelbar erneut faelligen Timern kein Busy Waiting entsteht.
Nach dem ersten empfangenen UI-Kommando wird die bereits gefuellte Queue ohne
weitere Wartezeit geleert. LVGL und alle UI-Aenderungen bleiben dabei im
UiTask; andere Tasks transportieren ausschliesslich `UiCommand`-Werte.

Der vorhandene FT6336U-Interrupt liegt auf GPIO7 und ist im LovyanGFX-Treiber
konfiguriert. Eine eigene ISR wurde in Phase 3.4 bewusst nicht registriert:
Der UiTask muss gleichzeitig auf der `uiCommandQueue` und auf LVGL-Timer warten,
waehrend eine Task Notification nicht gemeinsam mit einer Queue blockierend
abgewartet werden kann. Zudem muss LVGL den Touchzustand bis zum Loslassen
weiter lesen. Eine Queue-Set- oder IRQ-Loesung wird erst sinnvoll, wenn das
exakte elektrische Interruptverhalten auf der Zielhardware vermessen ist.

## Touch- und PN532-Busse

Touch und PN532 teilen keinen Bus:

* Der FT6336U wird ausschliesslich vom UiTask ueber I2C auf GPIO6/GPIO5
  angesprochen.
* Der PN532 wird ausschliesslich vom NfcTask ueber UART1 auf GPIO12/GPIO13
  angesprochen.

Deshalb wird fuer Phase 5.2 bewusst kein gemeinsamer I2C-Mutex angelegt. Der
PN532 kann den Touch-Bus weder blockieren noch dessen Taktfrequenz oder
Transaktionen beeinflussen. Umgekehrt greift der UiTask nicht auf UART1 zu.
Compile-Time-Pruefungen im NfcTask verhindern eine versehentliche Belegung der
Touch-I2C- oder HX711-Pins durch den PN532-UART.

Da keine gemeinsame Bussperre existiert, betraegt deren maximale Haltezeit
null und es gibt keine Lock-Reihenfolge zwischen UiTask und NfcTask. Der
NfcTask wartet blockierend mit einem 250-ms-Timeout auf seiner Command-Queue.
Nur nach dem Timeout startet er eine einzelne NFC-Suche; auf PN532-Antworten
wartet er blockierend im interruptgesteuerten UART-Treiber. Er haelt dabei
keinen Mutex. Damit kann zwischen Touch- und NFC-Verarbeitung kein
Bus-Mutex-Deadlock entstehen.

Falls der PN532 spaeter von UART auf I2C umgestellt wird, ist diese Entscheidung
ungueltig: Dann muessen Busbesitz, maximale Haltezeit und Lock-Reihenfolge vor
der Umstellung neu festgelegt und getestet werden.
