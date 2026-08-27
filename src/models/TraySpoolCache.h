#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "models/PrinterState.h"

namespace filament_station {
namespace models {

// Sized for realistic usage (one printer with one fully occupied AMS is 5
// slots incl. external) rather than the theoretical worst case (every AMS
// slot on every configured printer simultaneously assigned) -- keeps the
// struct's footprint in the same ballpark as BambuConfigCollection, since
// both cross task boundaries embedded in the (16-deep-queued) AppEvent.
// upsertTraySpoolCacheEntry() degrades gracefully (returns false, caller
// logs a warning) if this is ever exceeded rather than corrupting anything.
constexpr std::size_t kMaximumTraySpoolCacheEntries = 16;

// External-tray sentinel, matching the amsId/trayId == 0xFF convention
// already used throughout UiBridge.cpp/AppTask.cpp for the non-AMS/manual
// spool holder.
constexpr std::uint8_t kExternalTraySentinel = 0xFF;

// Persisted /mappings/printer-slots.json entry: the last known Spoolman
// association for one printer/AMS/tray slot, plus the material/colorHex the
// printer reported *at assignment time*. The printer itself keeps no memory
// of Spoolman identities (a previous attempt to round-trip this through a
// custom MQTT field was hardware-tested and abandoned, see
// docs/bambu-protocol.md) -- so this association is only trustworthy as
// long as the printer keeps reporting the same material/color for that
// slot. A later mismatch means someone changed the physical spool without
// going through this app, and the association must be treated as unknown.
struct TraySpoolCacheEntry {
  PrinterId printerId = kInvalidPrinterId;
  std::uint8_t amsId = 0;
  std::uint8_t trayId = 0;
  std::uint32_t spoolId = 0;
  // 16 bytes to match models::PrinterSlotStateData::material -- this is
  // filled straight from that field (AppTask.cpp), a smaller buffer here
  // would truncate it again independently and could falsely flag a valid
  // entry as stale in resolveTraySpoolCacheSpoolId()'s material comparison.
  char material[16]{};
  char colorHex[9]{};
};

struct TraySpoolCache {
  std::array<TraySpoolCacheEntry, kMaximumTraySpoolCacheEntries> entries{};
  std::uint8_t entryCount = 0;
};

inline TraySpoolCacheEntry* findTraySpoolCacheEntry(TraySpoolCache& cache,
                                                     PrinterId printerId,
                                                     std::uint8_t amsId,
                                                     std::uint8_t trayId) {
  for (std::uint8_t index = 0; index < cache.entryCount; ++index) {
    TraySpoolCacheEntry& entry = cache.entries[index];
    if (entry.printerId == printerId && entry.amsId == amsId &&
        entry.trayId == trayId)
      return &entry;
  }
  return nullptr;
}

inline const TraySpoolCacheEntry* findTraySpoolCacheEntry(
    const TraySpoolCache& cache, PrinterId printerId, std::uint8_t amsId,
    std::uint8_t trayId) {
  for (std::uint8_t index = 0; index < cache.entryCount; ++index) {
    const TraySpoolCacheEntry& entry = cache.entries[index];
    if (entry.printerId == printerId && entry.amsId == amsId &&
        entry.trayId == trayId)
      return &entry;
  }
  return nullptr;
}

// Inserts or overwrites the entry for (printerId, amsId, trayId). Returns
// false only if the cache is full and no matching entry already existed.
inline bool upsertTraySpoolCacheEntry(TraySpoolCache& cache,
                                      const TraySpoolCacheEntry& value) {
  TraySpoolCacheEntry* existing = findTraySpoolCacheEntry(
      cache, value.printerId, value.amsId, value.trayId);
  if (existing != nullptr) {
    *existing = value;
    return true;
  }
  if (cache.entryCount >= kMaximumTraySpoolCacheEntries) return false;
  cache.entries[cache.entryCount] = value;
  ++cache.entryCount;
  return true;
}

// Removes the entry for (printerId, amsId, trayId), if any -- swaps in the
// last entry to fill the gap, order doesn't matter for this collection.
inline void removeTraySpoolCacheEntry(TraySpoolCache& cache,
                                      PrinterId printerId,
                                      std::uint8_t amsId,
                                      std::uint8_t trayId) {
  for (std::uint8_t index = 0; index < cache.entryCount; ++index) {
    TraySpoolCacheEntry& entry = cache.entries[index];
    if (entry.printerId == printerId && entry.amsId == amsId &&
        entry.trayId == trayId) {
      entry = cache.entries[cache.entryCount - 1];
      --cache.entryCount;
      return;
    }
  }
}

static_assert(std::is_trivially_copyable<TraySpoolCacheEntry>::value,
              "TraySpoolCacheEntry must be trivially copyable");
static_assert(std::is_trivially_copyable<TraySpoolCache>::value,
              "TraySpoolCache must be trivially copyable");

}  // namespace models
}  // namespace filament_station
