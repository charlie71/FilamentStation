# Native FilamentStation-Tags

FilamentStation verwendet für eigene Tags ausschließlich einen NFC Forum
Type-2-NDEF-Textdatensatz mit folgendem Inhalt:

```text
spoolman:<spool_id>
```

Die Spool-ID muss eine positive dezimale 32-Bit-Zahl sein. Weitere
Filamentstammdaten werden nicht auf dem Tag gespeichert. Spoolman bleibt die
führende Datenquelle.

Unterstützt werden NTAG213, NTAG215 und NTAG216. Für neue Tags wird NTAG215
empfohlen. Die Identifikation verwendet den acht Byte langen `GET_VERSION`
Response und prüft zusätzlich den Capability Container:

| Tag | GET_VERSION Speichergröße | CC Byte 2 | NDEF-Speicher |
| --- | ---: | ---: | ---: |
| NTAG213 | `0x0F` | `0x12` | 144 Byte |
| NTAG215 | `0x11` | `0x3E` | 496 Byte |
| NTAG216 | `0x13` | `0x6D` | 872 Byte |

Vor dem Schreiben werden Capability Container, statische Lockbytes, dynamische
Lockbytes und `AUTH0` geprüft. Die Prüfung ist absichtlich konservativ: Ist die
Schreibfähigkeit nicht eindeutig nachweisbar, wird nicht geschrieben.

Nach Schreiben oder Löschen wird der Tag erneut ausgewählt. Erfolg wird nur
gemeldet, wenn die UID unverändert ist und die erneut gelesene Payload exakt der
erwarteten Spool-ID beziehungsweise einem leeren NDEF entspricht.

Quelle: [NXP NTAG213/215/216 Produktseite und Datenblatt Rev. 3.2](https://www.nxp.com/products/NTAG213_215_216)

