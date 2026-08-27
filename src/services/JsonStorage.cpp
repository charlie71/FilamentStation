#include "services/JsonStorage.h"

#include <cctype>
#include <cstring>

#include "models/BambuPrinterConfig.h"
#include "models/TraySpoolCache.h"

namespace filament_station::services {
namespace {

constexpr std::size_t kSmallConfigMaxSize = 4U * 1024U;
constexpr std::size_t kConfigMaxSize = 8U * 1024U;
constexpr std::size_t kLargeConfigMaxSize = 16U * 1024U;
constexpr std::size_t kDiagnosticsMaxSize = 32U * 1024U;
constexpr std::size_t kAtomicPathCapacity = 112U;

struct AtomicPaths {
  char temporary[kAtomicPathCapacity]{};
  char backup[kAtomicPathCapacity]{};
};

bool makeAtomicPaths(const char* targetPath, AtomicPaths& paths) {
  if (targetPath == nullptr || targetPath[0] != '/') {
    return false;
  }

  constexpr char kJsonSuffix[] = ".json";
  constexpr char kTemporarySuffix[] = ".tmp.json";
  constexpr char kBackupSuffix[] = ".bak.json";
  const std::size_t targetLength = std::strlen(targetPath);
  const std::size_t jsonSuffixLength = sizeof(kJsonSuffix) - 1U;
  if (targetLength <= jsonSuffixLength ||
      std::strcmp(targetPath + targetLength - jsonSuffixLength,
                  kJsonSuffix) != 0) {
    return false;
  }

  const std::size_t stemLength = targetLength - jsonSuffixLength;
  if (stemLength + sizeof(kTemporarySuffix) > sizeof(paths.temporary) ||
      stemLength + sizeof(kBackupSuffix) > sizeof(paths.backup)) {
    return false;
  }

  std::memcpy(paths.temporary, targetPath, stemLength);
  std::memcpy(paths.temporary + stemLength, kTemporarySuffix,
              sizeof(kTemporarySuffix));
  std::memcpy(paths.backup, targetPath, stemLength);
  std::memcpy(paths.backup + stemLength, kBackupSuffix,
              sizeof(kBackupSuffix));
  return true;
}

bool removeIfPresent(fs::FS& filesystem, const char* path) {
  return !filesystem.exists(path) || filesystem.remove(path);
}

bool validMappingFormat(const char* format) {
  return format != nullptr &&
         (std::strcmp(format, "filamentStation") == 0 ||
          std::strcmp(format, "bambuLab") == 0 ||
          std::strcmp(format, "openPrintTag") == 0 ||
          std::strcmp(format, "openTag3D") == 0 ||
          std::strcmp(format, "legacy") == 0 ||
          std::strcmp(format, "unknown") == 0);
}

bool validNormalizedUid(const char* uid) {
  if (uid == nullptr) return false;
  const std::size_t length = std::strlen(uid);
  if (length < 8 || length > 20 || (length & 1U) != 0) return false;
  for (std::size_t index = 0; index < length; ++index) {
    if (!std::isdigit(static_cast<unsigned char>(uid[index])) &&
        (uid[index] < 'A' || uid[index] > 'F'))
      return false;
  }
  return true;
}

bool isValidDocumentFile(fs::FS& filesystem, const char* path,
                         rtos::StorageDocumentType documentType) {
  File file = filesystem.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  JsonDocument document;
  const JsonStorageResult result =
      JsonStorage::load(file, documentType, document);
  file.close();
  return result.ok();
}

bool replaceWith(fs::FS& filesystem, const char* source,
                 const char* destination) {
  return removeIfPresent(filesystem, destination) &&
         filesystem.rename(source, destination);
}

bool isDigit(char value) {
  return std::isdigit(static_cast<unsigned char>(value)) != 0;
}

bool isUtcTimestamp(const char* value) {
  if (value == nullptr || std::strlen(value) != 20U) {
    return false;
  }

  constexpr std::size_t kDigitPositions[] = {
      0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
  for (const std::size_t position : kDigitPositions) {
    if (!isDigit(value[position])) {
      return false;
    }
  }

  return value[4] == '-' && value[7] == '-' && value[10] == 'T' &&
         value[13] == ':' && value[16] == ':' && value[19] == 'Z';
}

bool isNonEmptyString(const JsonVariantConst value) {
  return value.is<const char*>() && value.as<const char*>()[0] != '\0';
}

bool isOptionalString(const JsonVariantConst value) {
  return value.is<const char*>();
}

bool isValidIpv4(const char* value, bool allowEmpty) {
  if (value == nullptr || value[0] == '\0') return allowEmpty;
  std::uint8_t octets = 0;
  const char* cursor = value;
  while (*cursor != '\0') {
    if (octets == 4 || !isDigit(*cursor)) return false;
    unsigned valuePart = 0;
    unsigned digits = 0;
    while (isDigit(*cursor)) {
      valuePart = valuePart * 10U + static_cast<unsigned>(*cursor - '0');
      if (++digits > 3U || valuePart > 255U) return false;
      ++cursor;
    }
    ++octets;
    if (*cursor == '\0') break;
    if (*cursor != '.') return false;
    ++cursor;
    if (*cursor == '\0') return false;
  }
  return octets == 4;
}

bool isValidHostname(const char* value) {
  if (value == nullptr) return false;
  const std::size_t length = std::strlen(value);
  if (length == 0 || length > 32 || value[0] == '-' ||
      value[length - 1] == '-')
    return false;
  for (std::size_t index = 0; index < length; ++index) {
    const unsigned char character = static_cast<unsigned char>(value[index]);
    if (!std::isalnum(character) && character != '-') return false;
  }
  return true;
}

void applyNetworkDefaults(JsonDocument& document) {
  if (document["hostname"].isNull()) document["hostname"] = "filamentstation";
  if (document["dhcp"].isNull()) document["dhcp"] = true;
  if (document["ipAddress"].isNull()) document["ipAddress"] = "";
  if (document["gateway"].isNull()) document["gateway"] = "";
  if (document["subnetMask"].isNull()) document["subnetMask"] = "";
  if (document["dns"].isNull()) document["dns"] = "";
  if (document["portalName"].isNull())
    document["portalName"] = "FilamentStation";
  if (document["portalTimeoutSeconds"].isNull())
    document["portalTimeoutSeconds"] = 180;
  if (document["connectTimeoutSeconds"].isNull())
    document["connectTimeoutSeconds"] = 20;
}

void applySpoolmanDefaults(JsonDocument& document) {
  if (document["enabled"].isNull()) document["enabled"] = false;
  if (document["name"].isNull()) document["name"] = "Spoolman";
  if (document["serverUrl"].isNull()) document["serverUrl"] = "";
  if (document["timeoutMs"].isNull()) document["timeoutMs"] = 5000;
}

void applyBambuDefaults(JsonDocument& document) {
  if (document["selectedPrinterId"].isNull())
    document["selectedPrinterId"] = 0;
  if (document["defaultPrinterId"].isNull()) document["defaultPrinterId"] = 0;
  if (!document["printers"].is<JsonArrayConst>())
    document["printers"].to<JsonArray>();
}

void applyTraySpoolCacheDefaults(JsonDocument& document) {
  if (!document["entries"].is<JsonArrayConst>())
    document["entries"].to<JsonArray>();
}

bool isValidBambuHost(const char* value) {
  return isValidHostname(value) || isValidIpv4(value, false);
}

// Serial numbers and LAN access codes are opaque Bambu-assigned strings;
// only bounded, non-empty, printable content is enforced here, not an
// invented length or character pattern for the external protocol.
bool isValidBambuIdentifier(const JsonVariantConst value,
                            std::size_t maxLength) {
  if (!isNonEmptyString(value)) return false;
  const char* text = value.as<const char*>();
  const std::size_t length = std::strlen(text);
  if (length >= maxLength) return false;
  for (std::size_t index = 0; index < length; ++index) {
    if (!std::isprint(static_cast<unsigned char>(text[index]))) return false;
  }
  return true;
}

JsonStorageError validateBambuPrinters(const JsonDocument& document) {
  if (!document["selectedPrinterId"].is<std::uint16_t>() ||
      !document["defaultPrinterId"].is<std::uint16_t>() ||
      !document["printers"].is<JsonArrayConst>()) {
    return JsonStorageError::InvalidDocumentField;
  }

  const JsonArrayConst printers = document["printers"].as<JsonArrayConst>();
  if (printers.size() > models::kMaximumPrinters) {
    return JsonStorageError::InvalidDocumentField;
  }

  const auto selectedPrinterId =
      document["selectedPrinterId"].as<std::uint16_t>();
  const auto defaultPrinterId =
      document["defaultPrinterId"].as<std::uint16_t>();

  if (printers.size() == 0) {
    return selectedPrinterId == 0 && defaultPrinterId == 0
               ? JsonStorageError::Ok
               : JsonStorageError::InvalidDocumentField;
  }

  std::size_t defaultCount = 0;
  std::size_t selectedCount = 0;
  for (std::size_t index = 0; index < printers.size(); ++index) {
    const JsonObjectConst printer = printers[index].as<JsonObjectConst>();
    if (!printer["printerId"].is<std::uint16_t>() ||
        printer["printerId"].as<std::uint16_t>() == 0 ||
        !isValidBambuIdentifier(printer["name"], 32U) ||
        !isValidBambuHost(printer["host"] | static_cast<const char*>(nullptr)) ||
        !isValidBambuIdentifier(printer["serialNumber"], 24U) ||
        !isValidBambuIdentifier(printer["accessCode"], 24U) ||
        !printer["enabled"].is<bool>() ||
        !printer["default"].is<bool>() ||
        !printer["selected"].is<bool>()) {
      return JsonStorageError::InvalidDocumentField;
    }

    const auto printerId = printer["printerId"].as<std::uint16_t>();
    for (std::size_t other = 0; other < index; ++other) {
      if (printers[other]["printerId"].as<std::uint16_t>() == printerId) {
        return JsonStorageError::InvalidDocumentField;
      }
    }

    const bool isDefault = printer["default"].as<bool>();
    const bool isSelected = printer["selected"].as<bool>();
    if (isDefault) {
      ++defaultCount;
      if (printerId != defaultPrinterId) return JsonStorageError::InvalidDocumentField;
    }
    if (isSelected) {
      ++selectedCount;
      if (printerId != selectedPrinterId) return JsonStorageError::InvalidDocumentField;
    }
  }

  if (defaultCount != 1) return JsonStorageError::InvalidDocumentField;
  if (selectedPrinterId == 0) {
    if (selectedCount != 0) return JsonStorageError::InvalidDocumentField;
  } else if (selectedCount != 1) {
    return JsonStorageError::InvalidDocumentField;
  }
  return JsonStorageError::Ok;
}

// A slot address is either the external/manual holder (both amsId and
// trayId == kExternalTraySentinel) or a real AMS slot (both in their
// respective valid ranges) -- amsId valid but trayId the sentinel (or vice
// versa) is not a real address and is rejected.
bool isValidTraySlotAddress(std::uint8_t amsId, std::uint8_t trayId) {
  if (amsId == models::kExternalTraySentinel ||
      trayId == models::kExternalTraySentinel) {
    return amsId == models::kExternalTraySentinel &&
           trayId == models::kExternalTraySentinel;
  }
  return amsId < models::kMaximumAmsPerPrinter &&
         trayId < models::kSlotsPerAms;
}

JsonStorageError validateTraySpoolCacheEntries(const JsonDocument& document) {
  if (!document["entries"].is<JsonArrayConst>()) {
    return JsonStorageError::InvalidDocumentField;
  }
  const JsonArrayConst entries = document["entries"].as<JsonArrayConst>();
  if (entries.size() > models::kMaximumTraySpoolCacheEntries) {
    return JsonStorageError::InvalidDocumentField;
  }
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const JsonObjectConst entry = entries[index].as<JsonObjectConst>();
    if (!entry["printerId"].is<std::uint16_t>() ||
        entry["printerId"].as<std::uint16_t>() == 0 ||
        !entry["amsId"].is<std::uint8_t>() ||
        !entry["trayId"].is<std::uint8_t>() ||
        !isValidTraySlotAddress(entry["amsId"].as<std::uint8_t>(),
                                entry["trayId"].as<std::uint8_t>()) ||
        !entry["spoolId"].is<std::uint32_t>() ||
        entry["spoolId"].as<std::uint32_t>() == 0 ||
        !isNonEmptyString(entry["material"]) ||
        // Muss zu models::TraySpoolCacheEntry::material[16] passen -- diese
        // Grenze blieb bei 12 stehen, als der Puffer selbst schon auf 16
        // vergroessert war (TASKS.md 2026-08-26, Support-for-PLA-Fix), und
        // verwarf dadurch die gesamte Datei als "ungueltig", sobald
        // irgendein Eintrag ein 12+ Zeichen langes Material enthielt (z. B.
        // "Support for PLA", 15 Zeichen) -- nicht nur diesen einen Eintrag,
        // wegen der einmaligen Validierung des kompletten Dokuments.
        std::strlen(entry["material"].as<const char*>()) >= 16U ||
        // Nutzerbericht 2026-08-27: eine Spoolman-Spule ohne konfigurierte
        // Farbe fuehrt zu einem leeren tray_color im gesendeten
        // AssignTray-Kommando und damit auch im persistierten
        // Cache-Eintrag -- das ist ein legitimer, kein ungueltiger
        // Zustand. `isNonEmptyString()` verwarf solche Eintraege (und damit
        // wegen der Ganzdokument-Validierung das gesamte Dokument, siehe
        // die zwei vorherigen Nachtraege) faelschlich als ungueltig; nur
        // noch der Typ (String) und die Laenge werden geprueft, leer ist
        // ausdruecklich erlaubt.
        !isOptionalString(entry["colorHex"]) ||
        std::strlen(entry["colorHex"].as<const char*>()) >= 9U) {
      return JsonStorageError::InvalidDocumentField;
    }
    const auto printerId = entry["printerId"].as<std::uint16_t>();
    const auto amsId = entry["amsId"].as<std::uint8_t>();
    const auto trayId = entry["trayId"].as<std::uint8_t>();
    for (std::size_t other = 0; other < index; ++other) {
      const JsonObjectConst otherEntry = entries[other].as<JsonObjectConst>();
      if (otherEntry["printerId"].as<std::uint16_t>() == printerId &&
          otherEntry["amsId"].as<std::uint8_t>() == amsId &&
          otherEntry["trayId"].as<std::uint8_t>() == trayId) {
        return JsonStorageError::InvalidDocumentField;
      }
    }
  }
  return JsonStorageError::Ok;
}

}  // namespace

std::size_t JsonStorage::maxSizeFor(
    rtos::StorageDocumentType documentType) {
  switch (documentType) {
    case rtos::StorageDocumentType::Scale:
      return kSmallConfigMaxSize;
    case rtos::StorageDocumentType::Device:
    case rtos::StorageDocumentType::Network:
    case rtos::StorageDocumentType::Spoolman:
    case rtos::StorageDocumentType::Bambu:
    case rtos::StorageDocumentType::TraySpoolCache:
      return kConfigMaxSize;
    case rtos::StorageDocumentType::Ui:
    case rtos::StorageDocumentType::Nfc:
      return kLargeConfigMaxSize;
    case rtos::StorageDocumentType::Diagnostics:
      return kDiagnosticsMaxSize;
  }
  return 0;
}

JsonStorageResult JsonStorage::load(
    File& file, rtos::StorageDocumentType documentType,
    JsonDocument& document) {
  if (!file || file.isDirectory()) {
    return {JsonStorageError::FileUnavailable, 0};
  }

  const std::size_t maximumSize = maxSizeFor(documentType);
  if (maximumSize == 0) {
    return {JsonStorageError::InvalidArgument, 0};
  }

  const std::size_t fileSize = file.size();
  if (fileSize == 0) {
    return {JsonStorageError::EmptyDocument, 0};
  }
  if (fileSize > maximumSize) {
    return {JsonStorageError::FileTooLarge, 0};
  }
  if (!file.seek(0)) {
    return {JsonStorageError::ReadFailed, 0};
  }

  document.clear();
  const DeserializationError parseError = deserializeJson(document, file);
  if (parseError) {
    document.clear();
    return {JsonStorageError::ParseFailed, fileSize};
  }

  JsonStorageError error = applyDefaults(document);
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::Scale) {
    if (document["tareOffsetCounts"].isNull()) {
      document["tareOffsetCounts"] = 0;
    }
    if (document["factorCountsPerGram"].isNull()) {
      document["factorCountsPerGram"] = 1.0F;
    }
  }
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::Network) {
    applyNetworkDefaults(document);
  }
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::Spoolman) {
    applySpoolmanDefaults(document);
  }
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::Bambu) {
    applyBambuDefaults(document);
  }
  if (error == JsonStorageError::Ok &&
      documentType == rtos::StorageDocumentType::TraySpoolCache) {
    applyTraySpoolCacheDefaults(document);
  }
  if (error == JsonStorageError::Ok) {
    error = validate(document, documentType);
  }
  if (error != JsonStorageError::Ok) {
    document.clear();
  }
  return {error, fileSize};
}

JsonStorageError JsonStorage::applyDefaults(JsonDocument& document) {
  if (!document.is<JsonObject>()) {
    return JsonStorageError::RootNotObject;
  }

  if (document["schemaVersion"].isNull()) {
    document["schemaVersion"] = kCurrentJsonSchemaVersion;
  }
  if (document["updatedAt"].isNull()) {
    document["updatedAt"] = kDefaultUpdatedAt;
  }
  return JsonStorageError::Ok;
}

JsonStorageError JsonStorage::validate(const JsonDocument& document) {
  if (!document.is<JsonObjectConst>()) {
    return JsonStorageError::RootNotObject;
  }
  if (!document["schemaVersion"].is<std::uint32_t>()) {
    return JsonStorageError::InvalidSchemaVersion;
  }

  const std::uint32_t schemaVersion = document["schemaVersion"];
  if (schemaVersion != kCurrentJsonSchemaVersion) {
    return JsonStorageError::UnsupportedSchemaVersion;
  }
  if (!document["updatedAt"].is<const char*>() ||
      !isUtcTimestamp(document["updatedAt"].as<const char*>())) {
    return JsonStorageError::InvalidUpdatedAt;
  }
  return JsonStorageError::Ok;
}

const char* JsonStorage::documentTypeName(
    rtos::StorageDocumentType documentType) {
  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      return "device";
    case rtos::StorageDocumentType::Network:
      return "network";
    case rtos::StorageDocumentType::Spoolman:
      return "spoolman";
    case rtos::StorageDocumentType::Bambu:
      return "bambu";
    case rtos::StorageDocumentType::Ui:
      return "ui";
    case rtos::StorageDocumentType::Scale:
      return "scale";
    case rtos::StorageDocumentType::Nfc:
      return "nfc";
    case rtos::StorageDocumentType::Diagnostics:
      return "diagnostics";
    case rtos::StorageDocumentType::TraySpoolCache:
      return "traySpoolCache";
  }
  return nullptr;
}

JsonStorageError JsonStorage::createDefault(
    rtos::StorageDocumentType documentType, JsonDocument& document) {
  const char* typeName = documentTypeName(documentType);
  if (typeName == nullptr) {
    return JsonStorageError::InvalidArgument;
  }
  document.clear();
  document.to<JsonObject>();
  document["schemaVersion"] = kCurrentJsonSchemaVersion;
  document["updatedAt"] = kDefaultUpdatedAt;
  document["documentType"] = typeName;

  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      document["deviceName"] = "FilamentStation";
      break;
    case rtos::StorageDocumentType::Network:
      applyNetworkDefaults(document);
      break;
    case rtos::StorageDocumentType::Spoolman:
      applySpoolmanDefaults(document);
      break;
    case rtos::StorageDocumentType::Bambu:
      document["selectedPrinterId"] = 0;
      document["defaultPrinterId"] = 0;
      document["printers"].to<JsonArray>();
      break;
    case rtos::StorageDocumentType::Ui:
      document["language"] = "de";
      document["weightUnit"] = "g";
      break;
    case rtos::StorageDocumentType::Scale:
      document["calibrated"] = false;
      document["tareOffsetCounts"] = 0;
      document["factorCountsPerGram"] = 1.0F;
      break;
    case rtos::StorageDocumentType::Nfc:
      document["tagSchemaVersion"] = 1;
      document["mappings"].to<JsonArray>();
      break;
    case rtos::StorageDocumentType::Diagnostics:
      break;
    case rtos::StorageDocumentType::TraySpoolCache:
      document["entries"].to<JsonArray>();
      break;
  }
  return validate(document, documentType);
}

JsonStorageError JsonStorage::validate(
    const JsonDocument& document,
    rtos::StorageDocumentType documentType) {
  JsonStorageError error = validate(document);
  if (error != JsonStorageError::Ok) {
    return error;
  }
  const char* expectedType = documentTypeName(documentType);
  if (expectedType == nullptr || !document["documentType"].is<const char*>() ||
      std::strcmp(document["documentType"].as<const char*>(), expectedType) !=
          0) {
    return JsonStorageError::InvalidDocumentType;
  }

  switch (documentType) {
    case rtos::StorageDocumentType::Device:
      return isNonEmptyString(document["deviceName"])
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Network:
      if (!document["dhcp"].is<bool>() ||
          !document["portalTimeoutSeconds"].is<std::uint16_t>() ||
          !document["connectTimeoutSeconds"].is<std::uint16_t>() ||
          !isOptionalString(document["ipAddress"]) ||
          !isOptionalString(document["gateway"]) ||
          !isOptionalString(document["subnetMask"]) ||
          !isOptionalString(document["dns"]) ||
          !isNonEmptyString(document["portalName"]) ||
          !isValidHostname(document["hostname"].as<const char*>()) ||
          std::strlen(document["portalName"].as<const char*>()) > 25U ||
          document["portalTimeoutSeconds"].as<std::uint16_t>() < 30U ||
          document["portalTimeoutSeconds"].as<std::uint16_t>() > 900U ||
          document["connectTimeoutSeconds"].as<std::uint16_t>() < 1U ||
          document["connectTimeoutSeconds"].as<std::uint16_t>() > 60U ||
          !isValidIpv4(document["dns"].as<const char*>(), true)) {
        return JsonStorageError::InvalidDocumentField;
      }
      if (!document["dhcp"].as<bool>() &&
          (!isValidIpv4(document["ipAddress"].as<const char*>(), false) ||
           !isValidIpv4(document["gateway"].as<const char*>(), false) ||
           !isValidIpv4(document["subnetMask"].as<const char*>(), false))) {
        return JsonStorageError::InvalidDocumentField;
      }
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::Spoolman:
      return document["enabled"].is<bool>() &&
                     isNonEmptyString(document["name"]) &&
                     std::strlen(document["name"].as<const char*>()) < 32U &&
                     document["serverUrl"].is<const char*>() &&
                     std::strlen(document["serverUrl"].as<const char*>()) < 128U &&
                     document["timeoutMs"].is<std::uint32_t>() &&
                     document["timeoutMs"].as<std::uint32_t>() >= 1000U &&
                     document["timeoutMs"].as<std::uint32_t>() <= 60000U &&
                     (!document["enabled"].as<bool>() ||
                      isNonEmptyString(document["serverUrl"]))
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Ui:
      return isNonEmptyString(document["language"]) &&
                     isNonEmptyString(document["weightUnit"])
                 ? JsonStorageError::Ok
                 : JsonStorageError::InvalidDocumentField;
    case rtos::StorageDocumentType::Scale:
      if (!document["calibrated"].is<bool>() ||
          !document["tareOffsetCounts"].is<std::int32_t>() ||
          !document["factorCountsPerGram"].is<float>()) {
        return JsonStorageError::InvalidDocumentField;
      }
      if (document["calibrated"].as<bool>() &&
          document["factorCountsPerGram"].as<float>() == 0.0F) {
        return JsonStorageError::InvalidDocumentField;
      }
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::Nfc:
      if (!document["tagSchemaVersion"].is<std::uint32_t>() ||
          document["tagSchemaVersion"].as<std::uint32_t>() != 1U)
        return JsonStorageError::InvalidDocumentField;
      // /config/nfc.json and the mapping files intentionally share the NFC
      // document type.  Only mapping documents contain this array; their
      // path-specific mandatory validation is performed by StorageTask.
      if (document["mappings"].isNull()) return JsonStorageError::Ok;
      if (!document["mappings"].is<JsonArrayConst>())
        return JsonStorageError::InvalidDocumentField;
      {
        const JsonArrayConst mappings = document["mappings"].as<JsonArrayConst>();
        for (std::size_t index = 0; index < mappings.size(); ++index) {
          const JsonObjectConst mapping = mappings[index].as<JsonObjectConst>();
          const char* uid = mapping["uid"] | static_cast<const char*>(nullptr);
          const char* format =
              mapping["format"] | static_cast<const char*>(nullptr);
          if (!validNormalizedUid(uid) ||
              !mapping["spoolId"].is<std::uint32_t>() ||
              mapping["spoolId"].as<std::uint32_t>() == 0 ||
              (format != nullptr && !validMappingFormat(format)))
            return JsonStorageError::InvalidDocumentField;
          for (std::size_t other = 0; other < index; ++other) {
            const char* otherUid =
                mappings[other]["uid"] | static_cast<const char*>(nullptr);
            if (otherUid != nullptr && std::strcmp(uid, otherUid) == 0)
              return JsonStorageError::InvalidDocumentField;
          }
        }
      }
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::Bambu:
      return validateBambuPrinters(document);
    case rtos::StorageDocumentType::Diagnostics:
      return JsonStorageError::Ok;
    case rtos::StorageDocumentType::TraySpoolCache:
      return validateTraySpoolCacheEntries(document);
  }
  return JsonStorageError::InvalidArgument;
}

JsonStorageResult JsonStorage::serialize(
    const JsonDocument& document, rtos::StorageDocumentType documentType,
    Print& output) {
  const JsonStorageError validationError = validate(document, documentType);
  if (validationError != JsonStorageError::Ok) {
    return {validationError, 0};
  }

  const std::size_t maximumSize = maxSizeFor(documentType);
  if (maximumSize == 0) {
    return {JsonStorageError::InvalidArgument, 0};
  }

  const std::size_t requiredSize = measureJson(document);
  if (requiredSize == 0) {
    return {JsonStorageError::SerializeFailed, 0};
  }
  if (requiredSize > maximumSize) {
    return {JsonStorageError::OutputTooLarge, 0};
  }

  const std::size_t written = serializeJson(document, output);
  if (written != requiredSize) {
    return {JsonStorageError::SerializeFailed, written};
  }
  return {JsonStorageError::Ok, written};
}

JsonStorageResult JsonStorage::recoverAtomicSave(
    fs::FS& filesystem, const char* targetPath,
    rtos::StorageDocumentType documentType) {
  AtomicPaths paths{};
  if (!makeAtomicPaths(targetPath, paths)) {
    return {JsonStorageError::InvalidPath, 0};
  }

  const bool targetExists = filesystem.exists(targetPath);
  const bool temporaryExists = filesystem.exists(paths.temporary);
  const bool backupExists = filesystem.exists(paths.backup);
  const bool targetValid =
      targetExists && isValidDocumentFile(filesystem, targetPath, documentType);

  if (targetValid) {
    if (!removeIfPresent(filesystem, paths.temporary) ||
        !removeIfPresent(filesystem, paths.backup)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  const bool temporaryValid = temporaryExists &&
                              isValidDocumentFile(filesystem, paths.temporary,
                                                  documentType);
  const bool backupValid = backupExists &&
                           isValidDocumentFile(filesystem, paths.backup,
                                               documentType);

  if (temporaryValid) {
    if (!removeIfPresent(filesystem, targetPath) ||
        !filesystem.rename(paths.temporary, targetPath) ||
        !isValidDocumentFile(filesystem, targetPath, documentType) ||
        !removeIfPresent(filesystem, paths.backup)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  if (backupValid) {
    if (!removeIfPresent(filesystem, targetPath) ||
        !filesystem.rename(paths.backup, targetPath) ||
        !isValidDocumentFile(filesystem, targetPath, documentType) ||
        !removeIfPresent(filesystem, paths.temporary)) {
      return {JsonStorageError::RecoveryFailed, 0};
    }
    return {JsonStorageError::Ok, 0};
  }

  if (!targetExists && !temporaryExists && !backupExists) {
    return {JsonStorageError::Ok, 0};
  }
  return {JsonStorageError::RecoveryFailed, 0};
}

JsonStorageResult JsonStorage::atomicSave(
    fs::FS& filesystem, const char* targetPath,
    rtos::StorageDocumentType documentType, const JsonDocument& document) {
  AtomicPaths paths{};
  if (!makeAtomicPaths(targetPath, paths)) {
    return {JsonStorageError::InvalidPath, 0};
  }

  const JsonStorageError validationError = validate(document, documentType);
  if (validationError != JsonStorageError::Ok) {
    return {validationError, 0};
  }
  if (filesystem.exists(paths.temporary) || filesystem.exists(paths.backup)) {
    const JsonStorageResult recoveryResult =
        recoverAtomicSave(filesystem, targetPath, documentType);
    if (!recoveryResult.ok()) {
      return recoveryResult;
    }
  }
  if (!removeIfPresent(filesystem, paths.temporary)) {
    return {JsonStorageError::TemporaryFileFailed, 0};
  }

  File temporaryFile = filesystem.open(paths.temporary, FILE_WRITE);
  if (!temporaryFile) {
    return {JsonStorageError::TemporaryFileFailed, 0};
  }
  JsonStorageResult writeResult =
      serialize(document, documentType, temporaryFile);
  temporaryFile.flush();
  const bool writeFailed = temporaryFile.getWriteError() != 0;
  temporaryFile.close();
  if (!writeResult.ok() || writeFailed) {
    removeIfPresent(filesystem, paths.temporary);
    return {writeResult.ok() ? JsonStorageError::TemporaryFileFailed
                             : writeResult.error,
            writeResult.bytesProcessed};
  }

  if (!isValidDocumentFile(filesystem, paths.temporary, documentType)) {
    removeIfPresent(filesystem, paths.temporary);
    return {JsonStorageError::TemporaryValidationFailed,
            writeResult.bytesProcessed};
  }
  if (!removeIfPresent(filesystem, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }

  const bool hadTarget = filesystem.exists(targetPath);
  if (hadTarget && !filesystem.rename(targetPath, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }
  if (!filesystem.rename(paths.temporary, targetPath)) {
    if (hadTarget) {
      replaceWith(filesystem, paths.backup, targetPath);
    }
    return {JsonStorageError::CommitFailed, writeResult.bytesProcessed};
  }
  if (!isValidDocumentFile(filesystem, targetPath, documentType)) {
    if (hadTarget) {
      replaceWith(filesystem, paths.backup, targetPath);
    }
    return {JsonStorageError::CommitFailed, writeResult.bytesProcessed};
  }
  if (!removeIfPresent(filesystem, paths.backup)) {
    return {JsonStorageError::BackupFailed, writeResult.bytesProcessed};
  }
  return writeResult;
}

const char* JsonStorage::errorName(JsonStorageError error) {
  switch (error) {
    case JsonStorageError::Ok:
      return "ok";
    case JsonStorageError::InvalidArgument:
      return "invalid_argument";
    case JsonStorageError::InvalidPath:
      return "invalid_path";
    case JsonStorageError::FileUnavailable:
      return "file_unavailable";
    case JsonStorageError::EmptyDocument:
      return "empty_document";
    case JsonStorageError::FileTooLarge:
      return "file_too_large";
    case JsonStorageError::ReadFailed:
      return "read_failed";
    case JsonStorageError::ParseFailed:
      return "parse_failed";
    case JsonStorageError::RootNotObject:
      return "root_not_object";
    case JsonStorageError::InvalidSchemaVersion:
      return "invalid_schema_version";
    case JsonStorageError::UnsupportedSchemaVersion:
      return "unsupported_schema_version";
    case JsonStorageError::InvalidUpdatedAt:
      return "invalid_updated_at";
    case JsonStorageError::InvalidDocumentType:
      return "invalid_document_type";
    case JsonStorageError::InvalidDocumentField:
      return "invalid_document_field";
    case JsonStorageError::OutputTooLarge:
      return "output_too_large";
    case JsonStorageError::SerializeFailed:
      return "serialize_failed";
    case JsonStorageError::TemporaryFileFailed:
      return "temporary_file_failed";
    case JsonStorageError::TemporaryValidationFailed:
      return "temporary_validation_failed";
    case JsonStorageError::BackupFailed:
      return "backup_failed";
    case JsonStorageError::CommitFailed:
      return "commit_failed";
    case JsonStorageError::RecoveryFailed:
      return "recovery_failed";
  }
  return "unknown";
}

}  // namespace filament_station::services
