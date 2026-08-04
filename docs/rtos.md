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
In Phase 3.3 blockiert der UiTask zwischen den notwendigen
`lv_timer_handler()`-Aufrufen bis zu 10 ms auf der `uiCommandQueue`. Die Nutzung
des von LVGL gemeldeten naechsten Ausfuehrungszeitpunkts und ein moeglicher
Touch-IRQ werden in Phase 3.4
bewertet.
