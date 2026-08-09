# Tarierung und Kalibrierung

Der AppTask sendet `Tare`, `StartCalibration`, `ResetCalibration` und
`ApplyCalibration` ausschliesslich ueber die `scaleCommandQueue`. Nach dem
Queue-Senden weckt eine Task Notification den ScaleTask; sie ersetzt keine
Command-Daten und dient nur als Wecksignal.

Tarieren uebernimmt den letzten gefilterten HX711-Rohwert als
`tareOffsetCounts`. Die Referenzkalibrierung berechnet:

```text
factorCountsPerGram = (Messwert - tareOffsetCounts) / ReferenzgewichtInGramm
```

Der Faktor darf negativ sein, falls die Waegezelle in umgekehrter Richtung
angeschlossen ist, aber nicht null. Eine Kalibrierung benoetigt einen aktuellen
Messwert und ein positives Referenzgewicht.

Der ScaleTask sendet das Ergebnis an den AppTask. Nur der AppTask erstellt
daraus einen begrenzten JSON-Payload; ausschliesslich der StorageTask speichert
ihn atomar unter `/config/scale.json`. Nach `SdMounted` fordert der AppTask die
Datei beim StorageTask an und uebergibt die geladenen Werte wieder per
ScaleCommand an den ScaleTask.

Zuruecksetzen stellt `calibrated: false`, Offset `0` und Faktor `1.0` her und
speichert diesen Zustand ebenfalls ueber den StorageTask. Die Bedienung des
Kalibrierworkflows und die Anzeige in Gramm folgen in Phase 4.5.
