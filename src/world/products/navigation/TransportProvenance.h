#ifndef OUTSHINE_WORLD_PRODUCTS_NAVIGATION_TRANSPORTPROVENANCE_H
#define OUTSHINE_WORLD_PRODUCTS_NAVIGATION_TRANSPORTPROVENANCE_H

#include "SourceIdentity.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine::World {
class TransportTopology;

enum class TransportIdentitySpace : uint8_t { SourceElements, DerivedFragments };
enum class TransportAttribute : uint16_t {
  Modes = 1,
  Access = 2,
  Facility = 4,
  Surface = 8,
  Width = 16,
  Lanes = 32,
  Layer = 64,
  Bridge = 128,
  Tunnel = 256,
  Direction = 512
};

struct TransportSource {
  Data::SourceIdentity Identity;
  std::string Item;
  std::array<uint8_t, 32> Content{};
};

struct TransportSourceReference {
  uint32_t Source = 0;
  // Input segment ordinal; native splits have their own edge ordinals.
  uint32_t SegmentOrdinal = 0;
  std::optional<uint64_t> ProviderElementId;
  std::array<uint8_t, 32> Fragment{};
};

struct TransportPathSource {
  uint64_t PathId = 0;
  uint32_t FirstReference = 0;
  uint32_t ReferenceCount = 0;
  // Overlap records a changed supplied value; neither bit means unknown.
  uint16_t SuppliedFields = 0;
  uint16_t DerivedFields = 0;
};

struct TransportProvenance {
  TransportIdentitySpace IdentitySpace = TransportIdentitySpace::SourceElements;
  std::vector<TransportSource> Sources;
  std::vector<TransportPathSource> Paths;
  std::vector<TransportSourceReference> References;
};

[[nodiscard]] bool ValidateTransportProvenance(const TransportTopology &topology,
                                               const TransportProvenance &provenance);
}
#endif
