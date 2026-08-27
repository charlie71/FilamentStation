/**
 * @file
 * @brief Unused early-scaffolding placeholder; superseded by the filament
 *        fields on models::TagDefinition/models::SpoolmanCatalog. No code
 *        references this type.
 */
#pragma once

#include <cstdint>

namespace filament_station::models {
/// @deprecated Unused; superseded by TagDefinition/SpoolmanCatalog filament fields.
struct Filament {
  std::uint32_t id;  ///< Unused.
  char name[48];      ///< Unused.
};
}

