#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANCODEC_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANCODEC_H

#include "TransportPlan.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
struct TransportPlanCodecLimits {
  size_t PackageBytesMost = 0;
  size_t DecodedCapacityMost = 0;
};

[[nodiscard]] size_t TransportPlanCapacityBytes(const TransportPlan &plan) noexcept;

enum class TransportPlanCodecError : uint8_t {
  CapacityExceeded,
  UnsupportedFormat,
  Malformed,
  InvalidAddress,
  UnknownCoverage,
  InvalidInputs,
  InvalidPath,
  InvalidJunction,
  InvalidPort,
  InvalidBody,
  InvalidContact,
  InvalidOmission
};

[[nodiscard]] std::expected<std::array<uint8_t, 32>, TransportPlanCodecError>
TransportPlanInputBinding(std::span<const TransportInputReference> inputs, size_t bytesMost);
[[nodiscard]] std::expected<void, TransportPlanCodecError>
ValidateTransportPlan(const TransportPlan &plan);
[[nodiscard]] std::expected<std::vector<uint8_t>, TransportPlanCodecError>
EncodeTransportPlan(const TransportPlan &plan, TransportPlanCodecLimits limits);
[[nodiscard]] std::expected<TransportPlan, TransportPlanCodecError>
DecodeTransportPlan(std::span<const uint8_t> bytes, TransportPlanCodecLimits limits);
[[nodiscard]] std::expected<std::string, TransportPlanCodecError>
TransportPlanContentKey(const TransportPlanDefinition &definition,
                        const std::array<uint8_t, 32> &inputBinding);
}
#endif
