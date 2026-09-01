# UI-Designsystem

Die zentralen Design-Tokens stehen in `src/ui/UiDesignSystem.h`. Die Palette
definiert Hintergrund, Oberflächen, Primärfarbe, Verbindungszustände, Fehler,
deaktivierte Elemente sowie primäre und gedämpfte Textfarben.

Die Typografiestufen sind Status 16 px, Fließtext 18 px, Titel 24 px und
Gewicht 36 px. Abstände verwenden ausschließlich 4, 8, 12 oder 16 px.

Interaktive Inhaltsflächen sind mindestens 48 px hoch, bevorzugt 56 px. Die
40-px-Druckerleiste ist die durch das Standardlayout vorgegebene Ausnahme; ihr
Settings-Ziel wird als eigener mindestens 48 px breiter Bereich ausgeführt.

Das bestehende EEZ-Projekt enthält die wiederverwendbaren User-Widgets mit dem
Präfix `CMP_`. Sie definieren Struktur und Mindestgeometrie. Zustandsdaten und
Aktionen werden außerhalb des generierten Codes durch den UiTask gebunden.
