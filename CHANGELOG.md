# Changelog

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/), Versionierung an [SemVer](https://semver.org/) (siehe `docs/release.md`, Abschnitt "Version"). Diese Datei fasst zusammen, was sich für Nutzer/Entwickler ändert; die vollständige, chronologische Entwicklungshistorie mit Begründungen steht in `TASKS.md`.

## [Unreleased] -- 0.1.0-dev

Erster durchgängiger Funktionsumfang vor dem ersten getaggten Release.

### Added

- Grundarchitektur: FreeRTOS-Tasks pro Peripherie/Dienst (UI, Waage, NFC,
  Storage, Netzwerk, Spoolman, Bambu, Energiesparen, Firmware-Update), siehe
  `docs/architecture.md`.
- Waage (HX711): Messen, Tarieren, Kalibrierung.
- NFC (PN532): eigenes FilamentStation-Format sowie Lesesupport für
  Bambu-Lab-RFID, OpenPrintTag, OpenTag3D und ein Legacy-Format; Zuordnung
  und Entfernung ausschließlich über Spoolmans `extra.tag`-Feld, siehe
  `docs/tag-identity.md`, `docs/workflows.md`.
- Spoolman-Integration: Spulensuche, Gewichtsaktualisierung,
  Extrafeld-Automatik (`tag` wird bei Bedarf selbst angelegt).
- Bambu-Lab-Mehrdruckerunterstützung über das lokale LAN-MQTT-Protokoll:
  Verbindung, AMS-/Tray-Übersicht, Slot-Zuordnung inkl. lokal persistierter
  Drucker/Fach→Spule-Zuordnung, siehe `docs/bambu-protocol.md`,
  `docs/storage.md`.
- Energiesparen: Aktiv-/Gedimmt-/Sleep-Statemachine mit Touch-Wake.
- Firmware-Update über GitHub Releases: Versionscheck, Download mit
  SHA-256-Verifikation, App-Rollback-Absicherung, siehe `TASKS.md` Phase 13.
- SD-Karten-Speicherung aller Konfigurationen mit atomarem Schreiben und
  Backup/Wiederherstellung, siehe `docs/storage.md`.

### Known Issues

Siehe `docs/release.md`, Abschnitt "Known Issues", für die vollständige,
aus `TASKS.md` konsolidierte Liste (u. a. noch ausstehender mehrstündiger
Dauertest, reale Strommessung je Energiesparstufe, OTA-Rollback-Test).
