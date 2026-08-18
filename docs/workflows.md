# NFC-Zuordnungsworkflows

Die Benutzeroberfläche stellt ausschließlich die semantischen Aktionen
`Tag zuordnen` und `Tag-Zuordnung entfernen` bereit. Technische Schreib- und
Löschbefehle sind interne `NfcCommand`-Operationen und keine `UiAction`.

## Tag zuordnen

Der `AppTask` prüft TagIdentity und gewählte Spoolman-Spule, sucht eine
bestehende Zuordnung über `SpoolmanTask` und aktualisiert anschließend das
Spulenfeld `extra.tag`. Die `TagCapabilities` bestimmen den weiteren Ablauf:

- Native, sicher beschreibbare NTAG213/215/216 und explizit freigegebene
  Legacy-Tags: `extra.tag` setzen, `spoolman:<id>` schreiben und verifizieren.
- Bambu, OpenPrintTag, OpenTag3D, unbekannte sowie zu erhaltende Legacy-Tags:
  nur `extra.tag` setzen; der Originalinhalt bleibt unverändert.

Schlägt das Schreiben nach erfolgreicher Spoolman-Aktualisierung fehl, bleibt
`extra.tag` bestehen und die Ergebnisanzeige weist auf den Teilerfolg hin.

## Tag-Zuordnung entfernen

Zuerst wird die eindeutige Spule über `extra.tag` ermittelt und das Feld durch
den `SpoolmanTask` geleert. Nur wenn `canClearFilamentStationPayload` gesetzt
ist, entfernt der `NfcTask` danach den eigenen Payload und verifiziert den
leeren Zustand. Fremde Inhalte werden nie gelöscht. Schlägt die optionale
Tagbereinigung fehl, bleibt die Spoolman-Zuordnung entfernt und der Teilerfolg
wird angezeigt.

## Sicherheitsbedingungen

- Vor Schreiben und Löschen werden UID und Capabilities erneut geprüft.
- Tagentfernung und UID-Wechsel brechen technische Tagoperationen ab.
- Bambu-, OpenPrintTag- und OpenTag3D-Inhalte bleiben in Version 1 read-only.
- Der AppTask greift nicht auf NFC-Hardware oder SD zu.
- Nur der NfcTask führt PN532-Schreib-, Lösch- und Verifikationsbefehle aus.
- Der StorageTask ist an normalen Tag-Zuordnungen nicht beteiligt.
