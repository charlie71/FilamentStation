# Originale Bambu-Lab-RFID-Tags

Stand: 2026-08-11

## Quellen und Vertrauensgrenze

Es existiert keine hier verwendete offizielle Bambu-Lab-Formatspezifikation.
Die Implementierung basiert auf der öffentlich dokumentierten Reverse-
Engineering-Arbeit der Bambu Research Group:

- https://github.com/Bambu-Research-Group/RFID-Tag-Guide
- https://github.com/Bambu-Research-Group/RFID-Tag-Guide/blob/main/BambuLabRfid.md

Unbekannte oder nur vermutete Felder werden nicht interpretiert.

## Technik

- ISO/IEC 14443-A, MIFARE Classic 1K (SAK `0x08`)
- UID: 4 Byte, ohne Authentifizierung lesbar
- 16 Sektoren mit je vier 16-Byte-Blöcken
- sektorbezogene Key-A-Authentifizierung
- die 16 sechs Byte langen Schlüssel werden per HKDF-SHA256 aus der UID
  abgeleitet; Salt und Kontext entsprechen der veröffentlichten KDF
- Blöcke 40 bis 63 enthalten eine RSA-2048-Signatur

FilamentStation behandelt originale Bambu-Tags immer als read-only. Es gibt
keinen Schreib-, Lösch- oder Klonpfad und keine Signaturerzeugung.

## Verwendete dokumentierte Felder

| Block | Feld | Normalisierung |
|---:|---|---|
| 2 | Filamenttyp, ASCII | `TagDefinition.material` |
| 4 | detaillierter Filamenttyp, ASCII | `TagDefinition.filamentName` |
| 5, Byte 0..3 | RGBA | RGB als `TagDefinition.colorCode` |
| 5, Byte 4..5 | Nenngewicht in g, uint16 LE | `nominalFilamentWeightG` |
| 6, Byte 8..9 | maximale Hotendtemperatur, uint16 LE | `nozzleTempMaxC` |
| 6, Byte 10..11 | minimale Hotendtemperatur, uint16 LE | `nozzleTempMinC` |

Der Hersteller wird für einen erfolgreich authentifizierten und vollständig
gelesenen Bambu-Datensatz auf `Bambu Lab` normalisiert. Ein Farbname ist im
verwendeten öffentlichen Blockformat nicht dokumentiert und bleibt leer.

## Nicht verwendete Daten

- unbekannte Blöcke und reservierte Bytes
- vermutete Düsen-, Längen- oder Datumsfelder
- Signaturdaten
- Schlüssel und entschlüsselte Rohblöcke außerhalb des flüchtigen Lesevorgangs

Ein MIFARE-Classic-Tag wird nur dann als Bambu-Definition klassifiziert, wenn
die erforderlichen Blöcke 2, 4, 5 und 6 erfolgreich authentifiziert und gelesen
wurden. Die UID allein genügt nicht zur Bambu-Klassifikation.
