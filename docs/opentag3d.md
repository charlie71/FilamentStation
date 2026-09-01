# OpenTag3D read support

FilamentStation implements read-only support for OpenTag3D version 1.x from
the public primary specification:

- Specification: <https://opentag3d.info/spec.html>
- Machine-readable memory map: <https://opentag3d.info/spec.json>
- Source repository: <https://github.com/GooborgStudios/OpenTag3D>

## Detection and technology

An OpenTag3D tag is recognized only by an NDEF MIME record whose type is
exactly `application/opentag3d`. Other NDEF records and similar MIME names do
not match. The payload is the unencrypted binary OpenTag3D memory map.

The Core format requires an NDEF Type-2 compatible tag with at least 144 bytes
and is intended for NTAG213/215/216. Extended fields require more capacity and
are supported by NTAG215/216 and SLIX2. The installed PN532 can read the
ISO/IEC 14443 Type-A NTAG variants, but not the ISO/IEC 15693 SLIX2 variant.

## Normalized fields

The parser maps only specified fields into `TagDefinition`:

- manufacturer -> `vendor`
- base material -> `material`
- base material plus optional modifier -> `filamentName`
- optional color name and primary RGB value -> color fields
- target filament weight -> `nominalFilamentWeightG`
- optional empty spool weight -> `emptySpoolWeightG`
- optional minimum/maximum print temperatures, otherwise the required target
  print temperature -> nozzle temperature range

Fields without a corresponding `TagDefinition` member remain unused. Reserved
and optional bytes do not cause a parse failure. A version with a newer minor
number in major version 1 is parsed; an unsupported major version is rejected.

## Safety

OpenTag3D is read-only in FilamentStation version 1. Its normalized definition
can be linked to an existing Spoolman spool or passed to the generic Spoolman
import command. The NFC tag itself is never written or erased.

The native parser test vector is constructed directly from the addresses,
endianness, scaling rules, and examples in the machine-readable official
memory map. It covers Core and Extended fields, unrelated optional bytes,
strict MIME matching, unsupported major versions, and write protection.
