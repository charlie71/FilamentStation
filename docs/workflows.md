# NFC-Zuordnungsworkflows

Die Benutzeroberfläche stellt ausschließlich die semantischen Aktionen
`Tag zuordnen` und `Tag-Zuordnung entfernen` bereit. Technische Schreib- und
Löschbefehle sind interne `NfcCommand`-Operationen und keine `UiAction`.

## Tag zuordnen

Der `AppTask` prüft Tag, UID und gewählte Spoolman-Spule. Anschließend wird
das UID-Mapping ausschließlich über den `StorageTask` gespeichert. Die
`TagCapabilities` bestimmen den weiteren Ablauf:

- Native, sicher beschreibbare NTAG213/215/216 und explizit freigegebene
  Legacy-Tags: Mapping speichern, `spoolman:<id>` schreiben und verifizieren.
- Bambu, OpenPrintTag, OpenTag3D, unbekannte sowie zu erhaltende Legacy-Tags:
  nur Mapping speichern; der Originalinhalt bleibt unverändert.

Schlägt das Schreiben nach erfolgreicher Speicherung fehl, bleibt das Mapping
bestehen und die Ergebnisanzeige weist auf den Teilerfolg hin.

## Tag-Zuordnung entfernen

Zuerst wird das lokale UID-Mapping durch den `StorageTask` entfernt. Nur wenn
`canClearFilamentStationPayload` gesetzt ist, entfernt der `NfcTask` danach den
eigenen Payload und verifiziert den leeren Zustand. Fremde Inhalte werden nie
gelöscht. Schlägt die optionale Tagbereinigung fehl, bleibt das bereits
entfernte Mapping entfernt und der Teilerfolg wird angezeigt.

## Sicherheitsbedingungen

- Vor Schreiben und Löschen werden UID und Capabilities erneut geprüft.
- Tagentfernung und UID-Wechsel brechen technische Tagoperationen ab.
- Bambu-, OpenPrintTag- und OpenTag3D-Inhalte bleiben in Version 1 read-only.
- Der AppTask greift nicht auf NFC-Hardware oder SD zu.
- Nur der NfcTask führt PN532-Schreib-, Lösch- und Verifikationsbefehle aus.
