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

The user may link the UID to an existing Spoolman spool. This stores only a
local entry in `/mappings/nfc-spools.json` through `StorageTask`; the tag data
is not changed. Unknown MIFARE Classic memory is never interpreted or written.
