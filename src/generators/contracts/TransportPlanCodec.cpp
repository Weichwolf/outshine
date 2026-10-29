#include "TransportPlanFormat.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kPreviousFormat = 0x0003314e4c5054ULL;
constexpr uint64_t kFormat = 0x0004314e4c5054ULL;
constexpr uint32_t kPreviousRecipe = 3;

bool DefinitionFields(BinaryValueWriter &out, const TransportPlanDefinition &definition) {
  return out(static_cast<uint32_t>(definition.Core.Level),
             definition.Core.X,
             definition.Core.Y,
             definition.Representation,
             definition.RepresentationVersion);
}

bool DefinitionFields(TransportFormat::Reader &in, TransportPlanDefinition &definition) {
  uint32_t level = 0;
  if (!in(level,
          definition.Core.X,
          definition.Core.Y,
          definition.Representation,
          definition.RepresentationVersion) ||
      level > static_cast<uint32_t>(GeoCellId::MaximumLevel)) {
    return false;
  }
  definition.Core.Level = static_cast<int>(level);
  return true;
}

bool WriteInputs(BinaryValueWriter &out, std::span<const TransportInputReference> inputs) {
  return out.Text(TransportFormat::InputsDomain) &&
         TransportFormat::List(
             out, inputs, [&](const auto &input) { return TransportFormat::Input(out, input); });
}

bool ReadInputs(TransportFormat::Reader &in, std::vector<TransportInputReference> &inputs) {
  uint32_t length = 0;
  if (!in(length) || length != TransportFormat::InputsDomain.size()) { return false; }
  const auto domain = in.TakeBytes(length);
  return domain && std::ranges::equal(*domain, TransportFormat::InputsDomain) &&
         in.List(inputs, 4 * sizeof(uint32_t) + 32, [&](auto &input) {
           return TransportFormat::Input(in, input);
         });
}

bool WriteProducts(BinaryValueWriter &out, const TransportPlan &plan) {
  using TransportFormat::List;
  return TransportFormat::Network(out, plan.Network) &&
         List(out,
              std::span(plan.Paths),
              [&](const auto &path) { return TransportFormat::Path(out, path); }) &&
         List(out,
              std::span(plan.Stations),
              [&](const auto &station) { return TransportFormat::Station(out, station); }) &&
         List(out,
              std::span(plan.Junctions),
              [&](const auto &junction) { return TransportFormat::Junction(out, junction); }) &&
         List(out,
              std::span(plan.Ports),
              [&](const auto &port) { return TransportFormat::Port(out, port); }) &&
         List(out,
              std::span(plan.SourceEnds),
              [&](const auto &end) { return TransportFormat::SourceEnd(out, end); }) &&
         List(out,
              std::span(plan.Bodies),
              [&](const auto &body) { return TransportFormat::Body(out, body); }) &&
         List(out,
              std::span(plan.TerrainContacts),
              [&](const auto &stamp) { return TransportFormat::Contact(out, stamp); }) &&
         List(out, std::span(plan.Omissions), [&](const auto &omission) {
           return TransportFormat::Omission(out, omission);
         });
}

bool ReadProducts(TransportFormat::Reader &in, TransportPlan &plan, bool sourceEnds) {
  constexpr size_t pathBytes = 83;
  constexpr size_t junctionBytes = 36;
  constexpr size_t portBytes = 168;
  constexpr size_t bodyBytes = 32;
  constexpr size_t contactBytes = 123;
  return TransportFormat::Network(in, plan.Network) &&
         in.List(
             plan.Paths, pathBytes, [&](auto &path) { return TransportFormat::Path(in, path); }) &&
         in.List(plan.Stations,
                 3 * sizeof(double) + sizeof(uint64_t),
                 [&](auto &station) { return TransportFormat::Station(in, station); }) &&
         in.List(plan.Junctions,
                 junctionBytes,
                 [&](auto &junction) { return TransportFormat::Junction(in, junction); }) &&
         in.List(
             plan.Ports, portBytes, [&](auto &port) { return TransportFormat::Port(in, port); }) &&
         (!sourceEnds || in.List(plan.SourceEnds,
                                 256,
                                 [&](auto &end) { return TransportFormat::SourceEnd(in, end); })) &&
         in.List(
             plan.Bodies, bodyBytes, [&](auto &body) { return TransportFormat::Body(in, body); }) &&
         in.List(plan.TerrainContacts,
                 contactBytes,
                 [&](auto &stamp) { return TransportFormat::Contact(in, stamp); }) &&
         in.List(plan.Omissions, 2 * sizeof(uint32_t), [&](auto &omission) {
           return TransportFormat::Omission(in, omission);
         });
}
}

std::expected<std::array<uint8_t, 32>, TransportPlanCodecError>
TransportPlanInputBinding(std::span<const TransportInputReference> inputs, size_t bytesMost) {
  if (!TransportFormat::InputsValid(inputs)) {
    return std::unexpected(TransportPlanCodecError::InvalidInputs);
  }
  BinaryValueWriter out(bytesMost);
  if (!WriteInputs(out, inputs)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  return Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<std::vector<uint8_t>, TransportPlanCodecError>
EncodeTransportPlan(const TransportPlan &plan, TransportPlanCodecLimits limits) {
  const auto valid = ValidateTransportPlan(plan);
  if (!valid) { return std::unexpected(valid.error()); }
  const size_t capacity = TransportPlanCapacityBytes(plan);
  if (capacity > limits.DecodedCapacityMost) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  BinaryValueWriter out(limits.PackageBytesMost);
  out.Out.Reserve(std::min(limits.PackageBytesMost, capacity));
  if (!out(kFormat, kTransportPlanRecipeVersion) || !DefinitionFields(out, plan.Definition) ||
      !out(plan.Anchor.LongitudeDeg, plan.Anchor.LatitudeDeg, plan.InputBinding, plan.Coverage)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  const size_t inputsAt = out.Out.Bytes().size();
  if (!WriteInputs(out, plan.Inputs)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  const auto manifest = out.Out.Bytes().subspan(inputsAt);
  if (Sha256Digest(manifest.data(), manifest.size()) != plan.InputBinding) {
    return std::unexpected(TransportPlanCodecError::InvalidInputs);
  }
  if (!WriteProducts(out, plan)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<TransportPlan, TransportPlanCodecError>
DecodeTransportPlan(std::span<const uint8_t> bytes, TransportPlanCodecLimits limits) {
  if (bytes.size() > limits.PackageBytesMost ||
      sizeof(TransportPlan) > limits.DecodedCapacityMost) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  TransportFormat::Reader in(bytes, limits.DecodedCapacityMost);
  if (!in.Charge(sizeof(TransportPlan))) { return std::unexpected(in.Failure()); }
  uint64_t format = 0;
  uint32_t recipe = 0;
  if (!in(format, recipe)) { return std::unexpected(TransportPlanCodecError::Malformed); }
  if ((format != kFormat || recipe != kTransportPlanRecipeVersion) &&
      (format != kPreviousFormat || recipe != kPreviousRecipe)) {
    return std::unexpected(TransportPlanCodecError::UnsupportedFormat);
  }
  TransportPlan plan(TransportPlanDefinition{});
  if (!DefinitionFields(in, plan.Definition) ||
      !in(plan.Anchor.LongitudeDeg, plan.Anchor.LatitudeDeg, plan.InputBinding, plan.Coverage)) {
    return std::unexpected(in.Failure());
  }
  const size_t inputsAt = bytes.size() - in.RemainingBytes();
  if (!ReadInputs(in, plan.Inputs)) { return std::unexpected(in.Failure()); }
  const auto manifest = bytes.subspan(inputsAt, bytes.size() - in.RemainingBytes() - inputsAt);
  if (Sha256Digest(manifest.data(), manifest.size()) != plan.InputBinding) {
    return std::unexpected(TransportPlanCodecError::InvalidInputs);
  }
  if (!ReadProducts(in, plan, format == kFormat) || in.RemainingBytes() != 0) {
    return std::unexpected(in.Failure());
  }
  const auto valid = ValidateTransportPlan(plan);
  if (!valid) { return std::unexpected(valid.error()); }
  return plan;
}

std::expected<std::string, TransportPlanCodecError>
TransportPlanContentKey(const TransportPlanDefinition &definition,
                        const std::array<uint8_t, 32> &inputBinding) {
  if (!definition.Core.Valid() || !ValidTransportRepresentation(definition)) {
    return std::unexpected(TransportPlanCodecError::InvalidAddress);
  }
  BinaryValueWriter out(128);
  if (!out.Text("outshine/functional-transport-product") || !out(kTransportPlanRecipeVersion) ||
      !DefinitionFields(out, definition) || !out(inputBinding)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}
