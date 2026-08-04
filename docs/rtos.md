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

Der StorageTask verwendet mangels Card-Detect einen dokumentierten
Zwei-Sekunden-Timeout beim Warten auf seiner Queue. Nur nach diesem Timeout
erfolgt eine kurze SD-Erreichbarkeitsprobe. Das ist der langsamste praktikable
Fallback zur Erkennung einer Kartenentfernung ohne schnelle Polling-Schleife.
