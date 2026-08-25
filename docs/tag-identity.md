# Tag-Identität, Capabilities und Spoolman-Zuordnung

Formatspezifische Details (NDEF-Payload, Blockzuordnung, Schreibschutz) stehen
in `docs/nfc-tags.md`, `docs/bambu-rfid.md`, `docs/openprinttag.md`,
`docs/opentag3d.md` und `docs/legacy-and-unknown-tags.md`; der prozedurale
Ablauf von Zuordnen/Entfernen in `docs/workflows.md`. Dieses Dokument deckt
die dazwischenliegende, formatunabhängige Schicht ab: wie aus einem
gelesenen Tag eine kanonische Identität wird, welche Fähigkeiten daraus
folgen, und wie diese Identität mit Spoolman abgeglichen wird.

## TagIdentity

`models::TagIdentity` (`models/TagIdentity.h`) ist ein trivial kopierbarer
Werttyp:

```cpp
enum class TagIdentitySource : std::uint8_t { Unknown, NfcUid, BambuUuid };
struct TagIdentity {
  TagIdentitySource source = TagIdentitySource::Unknown;
  char value[40]{};
};
```

Sie wird einmal beim Lesen eines Tags bestimmt und danach als Wert durch den
gesamten Workflow kopiert (`TagReadResult::identity`) -- nach einem
asynchronen Schritt (z. B. einer Spoolman-Anfrage) wird sie nicht erneut aus
der Hardware abgeleitet, sondern der zuvor eingefrorene Wert weiterverwendet.
Das schützt vor einer stillschweigend geänderten Identität, falls der
physische Tag zwischenzeitlich entfernt oder getauscht wurde (siehe
"Sicherheitsbedingungen" in `docs/workflows.md`).

## UID-Normalisierung

`services/TagIdentity.cpp` stellt drei Konstruktionsfunktionen bereit, die
alle denselben Zielstring erzeugen: Großbuchstaben-Hex ohne Trennzeichen,
maximal 39 Zeichen plus Nullterminierung.

* `tagIdentityFromUid(uid, uidLength, identity)` -- Quelle `NfcUid`. Wandelt
  die rohen UID-Bytes direkt in Großbuchstaben-Hex um (z. B. `04:A1:B2` →
  `04A1B2`), keine inhaltliche Prüfung außer der Längengrenze.
* `tagIdentityFromBambuUuid(uuid: const char*, identity)` -- Quelle
  `BambuUuid`. Nutzt `normalizeTagIdentity()`: entfernt `:`, `-` und
  Leerzeichen, verlangt danach ausschließlich Hexziffern, erzwingt
  Großschreibung und eine geradzahlige, nicht-leere Länge. Zusätzlich wird
  hier genau 32 Zeichen (16 Byte) verlangt.
* `tagIdentityFromBambuUuid(uuid: const std::uint8_t*, uuidLength, identity)`
  -- Byte-Variante für die authentifiziert gelesene Tray-UID (Block 9, siehe
  `docs/bambu-rfid.md`). Verlangt exakt 16 Byte und lehnt eine komplett aus
  `0x00` oder komplett aus `0xFF` bestehende UID als ungültig ab (typisches
  Muster für einen nicht beschriebenen/nicht lesbaren Speicherbereich, keine
  echte UID).

Jede der drei Funktionen setzt bei einem Fehler `identity = {}` zurück
(`source = Unknown`, leerer `value`) statt einen Teilzustand stehen zu
lassen. Eine Identität mit `source == Unknown` gilt überall im Code als
"keine Identität vorhanden" und wird nie für einen Spoolman-Abgleich
verwendet.

## Capabilities

`models::TagCapabilities` (`models/TagReadResult.h`) fasst zusammen, was mit
einem konkret gelesenen Tag zulässig ist -- pro Tag neu bestimmt, nie global
angenommen:

```cpp
struct TagCapabilities {
  bool canAssociateByUid = false;
  bool canWriteFilamentStationPayload = false;
  bool canClearFilamentStationPayload = false;
  bool preserveOriginalContent = true;
};
```

* `canAssociateByUid`: die Identität darf für eine Spoolman-`extra.tag`-
  Zuordnung verwendet werden (gilt für alle erkannten Formate mit gültiger
  `TagIdentity`, auch read-only Formate wie Bambu/OpenPrintTag/OpenTag3D).
* `canWriteFilamentStationPayload` / `canClearFilamentStationPayload`: nur
  bei nachweislich sicher beschreibbaren NTAG213/215/216 (Capability
  Container, Lockbytes, `AUTH0` erfolgreich geprüft, siehe
  `docs/nfc-tags.md`) beziehungsweise explizit freigegebenen Legacy-Tags
  gesetzt.
* `preserveOriginalContent`: Default `true` -- der native Schreib-/Löschpfad
  ist die Ausnahme, nicht die Regel. Bambu-, OpenPrintTag-, OpenTag3D- und
  die meisten Legacy-/Unknown-Tags behalten diesen Wert, sodass
  `AssignTag`/`RemoveTagAssignment` ausschließlich `extra.tag` in Spoolman
  ändern und den physischen Tag unangetastet lassen.

Die tatsächliche Fallunterscheidung anhand dieser Felder steht in
`docs/workflows.md` (Abschnitte "Tag zuordnen"/"Tag-Zuordnung entfernen").

## Spoolman `extra.tag`

Die kanonische `TagIdentity.value` ist der einzige Ort, an dem FilamentStation
eine Tag-Zuordnung dauerhaft speichert -- als Klartext im Spoolman-Textfeld
`extra.tag` der jeweiligen Spule. Es gibt keine lokale Zuordnungsdatenbank
(siehe Abschnitt "Kein lokales Mapping" unten).

Der Nachschlagepfad (`SpoolmanClient::findSpoolByTag()`,
`services/SpoolmanClient.cpp:111`) fragt Spoolmans REST-Filter ab:

```text
GET /spool?allow_archived=true&limit=3&extra.tag="<identity>"
```

Der Server-Filter ist bei Spoolman 0.26.x ein exakter Textvergleich bei
doppelt zitiertem Wert; die Antwort wird dennoch client-seitig erneut exakt
gegen die angefragte `identity` verglichen (`decodeTextExtraField()` +
`strcmp`), bevor ein Treffer gezählt wird -- kein Vertrauen in serverseitige
Filtersemantik allein. `limit=3` genügt, um "genau einer", "keiner" und
"mehrdeutig (≥2)" zu unterscheiden, ohne bei einem Datenfehler beliebig viele
Spulen zu laden.

## Duplicate Handling

Ergebnis der Suche ist `TagLookupStatus`: `NotFound` (0 Treffer), `Found` (1
Treffer) oder `Duplicate` (≥2 Treffer, `spoolId` wird dabei bewusst auf `0`
zurückgesetzt statt einen der Treffer willkürlich zu wählen). `Duplicate`
kann nur durch eine bereits inkonsistente Spoolman-Datenbank entstehen (zwei
Spulen mit identischem `extra.tag`-Wert) -- dass der Code diesen Fall
überhaupt eigens behandelt, zeigt: FilamentStation setzt keinen
serverseitig erzwungenen Unique-Constraint auf `extra.tag` voraus. Einen
bereits bestehenden Konflikt behandelt es aber überall gleich: **die
betroffene Aktion wird abgebrochen, nie automatisch
aufgelöst**, mit einer expliziten Fehlermeldung, die auf die Spoolman-Seite
verweist (`AppTask.cpp`, alle drei Aufrufstellen):

| Kontext | Nutzertext |
|---|---|
| Nativer Konsistenzcheck (NDEF nennt Spule X, Duplicate in Spoolman) | "Diese Tag-ID ist mehreren Spulen zugeordnet. Der NDEF-Payload wird nicht als Zuordnung verwendet." |
| `RemoveTagAssignment` | "Diese Tag-ID ist mehreren Spulen zugeordnet. Bitte die Spoolman-Daten korrigieren." |
| `AssignTag` | "Dieselbe Tag-ID ist mehreren Spulen zugeordnet. Bitte den Konflikt in Spoolman beheben." |

Der Konflikt lässt sich nur außerhalb von FilamentStation beheben (in
Spoolman selbst eines der beiden `extra.tag`-Felder leeren oder ändern).
