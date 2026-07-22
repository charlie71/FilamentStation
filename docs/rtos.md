# FreeRTOS-Grundarchitektur

Alle Service-Tasks blockieren auf ihrer Command-Queue. AppTask blockiert auf der
Event-Queue, UiTask auf der UI-Command-Queue. Es wird noch kein Core-Pinning
verwendet. Taskparameter und Queue-Laengen stehen in `src/config/TaskConfig.h`.
