#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANADDRESS_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANADDRESS_H

#include "GeoCellId.h"
#include <array>
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Generators {
inline constexpr uint32_t kTransportPlanRecipeVersion = 4;

struct TransportInputScopeId {
  explicit TransportInputScopeId(std::array<uint8_t, 32> bytes) noexcept : Bytes(bytes) {}

  std::array<uint8_t, 32> Bytes;

  [[nodiscard]] bool operator==(const TransportInputScopeId &) const noexcept = default;
};

enum class TransportRepresentation : uint8_t { Functional = 1, FunctionalSubset = 2 };

struct TransportPlanDefinition {
  GeoCellId Core;
  TransportRepresentation Representation = TransportRepresentation::Functional;
  uint32_t RepresentationVersion = 2;

  [[nodiscard]] bool operator==(const TransportPlanDefinition &) const noexcept = default;
};

struct TransportPlanAddress {
  TransportPlanDefinition Definition;
  TransportInputScopeId Inputs;

  [[nodiscard]] bool operator==(const TransportPlanAddress &) const noexcept = default;
};

enum class TransportPlanRequestError : uint8_t {
  InvalidCore,
  UnsupportedRepresentation,
  EncodingFailed
};

[[nodiscard]] bool ValidTransportRepresentation(const TransportPlanDefinition &definition) noexcept;

[[nodiscard]] std::expected<std::string, TransportPlanRequestError>
TransportPlanRequestKey(const TransportPlanAddress &address);
}
#endif
