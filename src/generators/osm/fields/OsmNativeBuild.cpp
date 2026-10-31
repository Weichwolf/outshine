#include "OsmField.h"
#include "OsmStorageUsage.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
namespace {
constexpr size_t kFixedPacketBytesMost = (1 + 3 + 10 + 3 + 8) * sizeof(uint32_t) +
                                         sizeof(uint64_t) + sizeof(uint8_t) + 2 * sizeof(double) +
                                         64;
constexpr size_t kFeatureBytesMost = 5 * sizeof(uint32_t) + sizeof(uint16_t) + 3 * sizeof(uint8_t) +
                                     sizeof(uint64_t) + 32 + 4 * sizeof(double);
constexpr std::string_view kInvalidInput = "invalid MVT storage plan";
constexpr std::string_view kCapacity = "MVT generation exceeds native storage capacity";

struct BuildStorage {
  size_t Bytes = 0;

  bool Add(size_t count, size_t width = 1) {
    if (count > (std::numeric_limits<size_t>::max() - Bytes) / width) { return false; }
    Bytes += count * width;
    return true;
  }
};

struct LayerStorage {
  BuildStorage Parsed;
  OsmStorageUsage Native{.Tiles = 1};
  size_t Scratch = 0;
  size_t Text = 0;

  bool Add(const MvtLayer::AllocationPlan &plan) {
    const size_t pairs = plan.Tags / 2;
    const OsmStorageUsage growth{.Features = plan.Features,
                                 .Rings = plan.Rings,
                                 .Points = plan.Points / 2,
                                 .Tags = plan.Tags,
                                 .Values = pairs,
                                 .Keys = std::min(plan.Keys, pairs),
                                 .Strings = std::min(plan.Values, pairs)};
    if (!Native.TryAdd(growth) || !Parsed.Add(plan.HeapBytesMost) ||
        plan.TextBytes > std::numeric_limits<size_t>::max() - Text) {
      return false;
    }
    Text += plan.TextBytes;
    Scratch = std::max(Scratch, plan.PeakBytesMost - plan.HeapBytesMost);
    return true;
  }
};
}

std::expected<OsmField::NativeBuildPlan, std::string_view> OsmField::NativeTileBuildBytes(
    std::span<const uint8_t> bytes, std::span<const std::string> layers, MvtSchema schema) {
  if (layers.size() > size_t{std::numeric_limits<uint16_t>::max()} + 1) {
    return std::unexpected(kCapacity);
  }
  LayerStorage storage;
  BuildStorage names;
  BuildStorage encoded;
  if (!encoded.Add(kFixedPacketBytesMost)) { return std::unexpected(kCapacity); }
  for (const auto &name : layers) {
    if (!encoded.Add(name.size()) || !encoded.Add(sizeof(uint32_t)) || !names.Add(name.size(), 4) ||
        !names.Add(1, 4 * sizeof(std::string) + 4)) {
      return std::unexpected(kCapacity);
    }
    const auto source = MvtLayerName(schema, name);
    if (source.empty()) { continue; }
    const auto plan = MvtLayer::Plan(bytes, source);
    if (!plan) {
      if (plan.error() == MvtLayer::ParseError::MissingLayer) { continue; }
      return std::unexpected(kInvalidInput);
    }
    if (!storage.Add(*plan)) { return std::unexpected(kCapacity); }
  }
  const auto &native = storage.Native;
  BuildStorage total;
  if (!total.Add(storage.Parsed.Bytes) || !total.Add(storage.Scratch) || !total.Add(names.Bytes) ||
      !total.Add(layers.size(), sizeof(std::optional<MvtLayer>)) ||
      !total.Add(2, sizeof(OsmField)) || !total.Add(1, sizeof(ParsedTile)) ||
      !total.Add(1, sizeof(Tile)) || !total.Add(1, sizeof(uint64_t) + sizeof(ParsedTile *)) ||
      !total.Add(4, 4 * sizeof(void *) + sizeof(uint8_t)) ||
      !total.Add(2, 2 * size_t{64} + sizeof(std::string) + 2) ||
      !total.Add(native.Features, 3 * sizeof(Feature)) ||
      !total.Add(native.Rings, 3 * sizeof(Ring)) || !total.Add(native.Points, 6 * sizeof(double)) ||
      !total.Add(native.Tags, 3 * sizeof(uint32_t)) ||
      !total.Add(native.Values, 3 * sizeof(Value)) ||
      !total.Add(native.Keys, 3 * sizeof(std::string)) ||
      !total.Add(native.Strings, 3 * sizeof(std::string)) || !total.Add(storage.Text, 2) ||
      !total.Add(native.Keys, sizeof(std::string) + 2) ||
      !total.Add(native.Strings, sizeof(std::string) + 2) ||
      !total.Add(OsmStringIndex::PeakBytes(native.Keys)) ||
      !total.Add(OsmStringIndex::PeakBytes(native.Strings))) {
    return std::unexpected(kCapacity);
  }
  if (!encoded.Add(native.Points, 2 * sizeof(double)) ||
      !encoded.Add(native.Tags, sizeof(uint32_t)) || !encoded.Add(native.Keys, sizeof(uint32_t)) ||
      !encoded.Add(native.Strings, sizeof(uint32_t)) || !encoded.Add(storage.Text) ||
      !encoded.Add(native.Values, sizeof(double) + sizeof(uint32_t) + sizeof(uint8_t)) ||
      !encoded.Add(native.Rings, 2 * sizeof(uint32_t) + sizeof(uint8_t)) ||
      !encoded.Add(native.Features, kFeatureBytesMost)) {
    return std::unexpected(kCapacity);
  }
  return NativeBuildPlan{.WorkingBytesMost = total.Bytes, .EncodedBytesMost = encoded.Bytes};
}
}
