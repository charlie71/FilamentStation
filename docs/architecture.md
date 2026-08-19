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
