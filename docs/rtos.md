# FreeRTOS-Grundarchitektur

Alle Service-Tasks blockieren auf ihrer Command-Queue. AppTask blockiert auf der
Event-Queue, UiTask auf der UI-Command-Queue. Es wird noch kein Core-Pinning
verwendet. Taskparameter und Queue-Laengen stehen in `src/config/TaskConfig.h`.

Der StorageTask verwendet mangels Card-Detect einen dokumentierten
Zwei-Sekunden-Timeout beim Warten auf seiner Queue. Nur nach diesem Timeout
erfolgt eine kurze SD-Erreichbarkeitsprobe. Das ist der langsamste praktikable
Fallback zur Erkennung einer Kartenentfernung ohne schnelle Polling-Schleife.
