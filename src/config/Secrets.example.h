#pragma once

// Vorlage fuer spaetere, lokale Geheimnisse. Keine Zugangsdaten eintragen und
// Secrets.h nicht versionieren.
//
// Das WiFiManager-Konfigurationsportal benoetigt kein eingechecktes Geheimnis:
// SSID und AP-Passwort werden zur Laufzeit aus der ESP32-Geraete-ID gebildet.
// Router-SSID und Router-Passwort verwaltet WiFiManager im ESP32-Flash und sie
// werden weder hier noch auf SD gespeichert.
