# OpenPrintTag – Lesesupport in FilamentStation

Stand der geprüften Primärquelle: 2026-08-11.

## Primärquellen

- Spezifikation und Referenzimplementierung: <https://github.com/OpenPrintTag/openprinttag-specification>
- Datenformat: `docs_src/nfc_data_format.md`
- Hardware: `docs_src/physical_spec.md`
- Felddefinitionen: `data/main_fields.yaml` und `data/meta_fields.yaml`
- Testvektor: `tests/encode_decode/01_data.bin` mit `01_input.yaml`

## Erkennung und Datenformat

Ein Tag wird ausschließlich dann als OpenPrintTag erkannt, wenn seine
NDEF-Nachricht einen nicht gechunkten MIME-Record mit dem exakten Typ
`application/vnd.openprinttag` enthält. Eine bloße Textübereinstimmung oder
eine bestimmte UID genügt nicht.

Der MIME-Payload besteht aus einer CBOR-Meta-Map und einer CBOR-Main-Map.
Ganzzahlige Feldschlüssel dürfen in beliebiger Reihenfolge auftreten. Der
Parser überspringt unbekannte Schlüssel und auch verschachtelte unbekannte
Werte, wie von der Spezifikation verlangt. Fehlerhafte bekannte Felder oder
ein ungültiger CBOR-Aufbau führen zu einer ungültigen, read-only Erkennung.

Abgebildete Main-Felder:

| Schlüssel | OpenPrintTag | `TagDefinition` |
|---:|---|---|
| 9 | `material_type` | `material` gemäß offizieller Enum-Abkürzung |
| 10 | `material_name` | `filamentName` |
| 11 | `brand_name` | `vendor` |
| 16/17 | nominales/tatsächliches Nettovollgewicht | `nominalFilamentWeightG`; tatsächliches Gewicht hat Vorrang |
| 18 | Leergewicht des Behälters | `emptySpoolWeightG` |
| 19 | primäre RGB(A)-Farbe | `colorCode` als `#RRGGBB` |
| 34/35 | minimale/maximale Drucktemperatur | `nozzleTempMinC` / `nozzleTempMaxC` |

Es wird kein Farbname erfunden. Optionale, nicht abgebildete Felder bleiben
uninterpretiert.

## Tagtechnologie und Hardwaregrenze

Die aktuelle OpenPrintTag-MK1-Physikspezifikation schreibt ISO/IEC 15693-3
(NFC-V) vor und nennt ICODE SLIX2 mit rund 320 Byte als Referenz. Der im Gerät
verbaute PN532 unterstützt ISO 14443A, aber kein ISO 15693. Daher kann der
implementierte Parser offizielle Speicherabbilder und bereits bereitgestellte
NDEF-Daten verarbeiten; ein reales OpenPrintTag-MK1 kann mit der aktuellen
PN532-Hardware jedoch nicht eingelesen werden.

Für einen späteren Hardwaretest ist zusätzlich ein NFC-V-fähiger Reader nötig.
Die Auswahl und GPIO-Belegung sind keine Aufgabe von Phase 5.7 und werden hier
nicht erfunden.

## Schreibschutz

FilamentStation Version 1 behandelt OpenPrintTag vollständig read-only. Es
werden weder Schreib-, Lösch- noch Reinitialisierungskommandos erzeugt.
