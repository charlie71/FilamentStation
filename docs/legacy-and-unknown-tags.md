# Legacy and unknown NFC tags

## Supported legacy format

Version 1 recognizes exactly one legacy format: an NFC Forum Type-2 NDEF Text
record containing `spool:<positive decimal spool id>`, for example
`spool:42`. This format was already present in the phase 5.3 payload
classifier. `LegacyTagParser` converts it into a `TagDefinition` containing
only the known Spoolman ID. No manufacturer, material, color, weight, or
temperature is inferred.

A recognized legacy payload on a verified writable NTAG213/215/216 may be
migrated to the native `spoolman:<id>` payload. The normal write path reads the
tag again and verifies the new payload. Erasing is permitted only under the
same conditions. No encrypted legacy format and no Security-Key-dependent
format is recognized or modified.

## Unknown tags

Unknown tags show their detected technology, UID, NDEF presence/readability,
and physical write capability only when the NTAG capability/lock checks made
that property reliable. Regardless of physical capability, an unknown format
never receives write or erase permission.

The user may assign the UID to an existing Spoolman spool. The canonical UID
is stored exclusively in the spool's Spoolman `extra.tag` field; the physical
tag data is not changed. Unknown MIFARE Classic memory is never interpreted or
written.

## Obsolete local mapping files

`/mappings/nfc-spools.json`, `/mappings/bambu-tags.json`, and
`/mappings/open-tags.json` are accepted only by the one-time online migration.
They are never queried or updated by normal NFC workflows. Each legacy file is
removed only after every valid entry was transferred conflict-free to
Spoolman. Missing files are the expected normal state.

`/mappings/printer-slots.json` is unrelated to NFC identities. No current
runtime implementation reads or writes it; it is therefore not retained as an
NFC mapping source.

## Migration implementation inventory

The three obsolete mapping paths occur only in the transition state machine
in `AppTask`, the transition-only `LegacyNfcMappingEntry` queue value, and the
corresponding StorageTask/JSON reader. There is no runtime mapping repository
or mapping service. Normal tag lookup, assignment, and removal go directly
from AppTask to SpoolmanTask; the UI has no dependency on mapping files.
Mapping-shaped JSON tests cover the one-time importer only. Runtime assignment
tests use `extra.tag` through the Spoolman client.
