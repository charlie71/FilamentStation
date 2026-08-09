# Waagenfilter

Der `ScaleFilter` verarbeitet in Phase 4.3 noch vorzeichenbehaftete
HX711-Rohwerte. Die Umrechnung in Gramm und die Kalibrierung folgen in Phase
4.4. Alle aktuellen Grenzwerte sind deshalb vorlaeufige Rohwertparameter in
`src/config/ScaleConfig.h` und muessen nach der Kalibrierung anhand der realen
Waage validiert werden.

Die Verarbeitung erfolgt in dieser Reihenfolge:

1. kleine negative Werte innerhalb der konfigurierten Schwelle auf null setzen,
2. einzelne Ausreisser gegen den letzten akzeptierten Wert erkennen,
3. einen dauerhaften Sprung nach mehreren aehnlichen Samples akzeptieren,
4. gleitenden Mittelwert ueber ein fest begrenztes Ringarray bilden,
5. Tiefpass erster Ordnung anwenden,
6. Stabilitaet erst nach Ablauf der Stabilitaetszeit innerhalb eines
   Toleranzbandes melden.

Ein verworfener Einzelwert veraendert weder Mittelwert noch Tiefpass. Dadurch
kann ein einzelner Stoerimpuls die stabile Anzeige nicht verschieben. Ein realer
Lastwechsel wird hingegen akzeptiert, sobald die konfigurierte Anzahl
konsistenter Folgesamples erreicht ist.

Der ScaleTask sendet jeden gefilterten Rohwert als `ScaleMeasurement`. Wechsel
des Stabilitaetszustands werden separat als `ScaleStable` beziehungsweise
`ScaleUnstable` an den AppTask gemeldet. Eine Darstellung in Gramm oder eine
Freigabe der Gewichtsspeicherung erfolgt in dieser Phase noch nicht.
